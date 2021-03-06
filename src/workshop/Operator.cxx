/*
 * Copyright 2006-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Operator.hxx"
#include "ProgressReader.hxx"
#include "ControlChannelServer.hxx"
#include "Expand.hxx"
#include "Workplace.hxx"
#include "Plan.hxx"
#include "Job.hxx"
#include "LogBridge.hxx"
#include "event/net/UdpListener.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringFormat.hxx"
#include "util/UTF8.hxx"

#include <map>

#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

WorkshopOperator::WorkshopOperator(EventLoop &_event_loop,
				   WorkshopWorkplace &_workplace,
				   const WorkshopJob &_job,
				   const std::shared_ptr<Plan> &_plan,
				   UniqueFileDescriptor stderr_read_pipe,
				   UniqueSocketDescriptor control_socket,
				   size_t max_log_buffer,
				   bool enable_journal) noexcept
	:event_loop(_event_loop),
	 workplace(_workplace), job(_job), plan(_plan),
	 logger(*this),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout)),
	 control_channel(control_socket.IsDefined()
			 ? new WorkshopControlChannelServer(event_loop,
							    std::move(control_socket),
							    *this)
			 : nullptr),
	 log(event_loop, job.plan_name.c_str(), job.id.c_str(),
	     std::move(stderr_read_pipe))
{
	ScheduleTimeout();

	if (max_log_buffer > 0)
		log.EnableBuffer(max_log_buffer);

	if (enable_journal)
		log.EnableJournal();
}

WorkshopOperator::~WorkshopOperator() noexcept
{
	timeout_event.Cancel();
}

void
WorkshopOperator::ScheduleTimeout() noexcept
{
	const auto t = plan->parsed_timeout;
	if (t > t.zero())
		timeout_event.Schedule(t);
}

void
WorkshopOperator::OnTimeout() noexcept
{
	logger(2, "timed out; sending SIGTERM");

	job.SetDone(-1, "Timeout");

	workplace.OnTimeout(this, pid);
}

void
WorkshopOperator::OnProgress(unsigned progress) noexcept
{
	if (exited)
		/* after the child process has exited, it's pointless to
		   update the progress because it will be set to 100% anyway;
		   this state can occur in OnChildProcessExit() during the
		   LogBridge::Flush() call */
		return;

	job.SetProgress(progress, plan->timeout.c_str());

	/* refresh the timeout */
	ScheduleTimeout();
}

void
WorkshopOperator::SetOutput(UniqueFileDescriptor fd) noexcept
{
	assert(fd.IsDefined());
	assert(!progress_reader);

	progress_reader.reset(new ProgressReader(event_loop, std::move(fd),
						 BIND_THIS_METHOD(OnProgress)));

}

void
WorkshopOperator::CreateSyslogClient(const char *me,
				     int facility,
				     const char *host_and_port)
{
	try {
		log.CreateSyslog(host_and_port, me, facility);
	} catch (...) {
		std::throw_with_nested(FormatRuntimeError("syslog_open(%s) failed",
							  host_and_port));
	}
}

void
WorkshopOperator::Expand(std::list<std::string> &args) const noexcept
{
	assert(!args.empty());

	StringMap vars;
	vars.emplace("0", args.front());
	vars.emplace("NODE", workplace.GetNodeName());
	vars.emplace("JOB", job.id);
	vars.emplace("PLAN", job.plan_name);

	for (auto &i : args)
		::Expand(i, vars);
}

void
WorkshopOperator::OnChildProcessExit(int status) noexcept
{
	exited = true;

	if (control_channel) {
		try {
			control_channel->ReceiveAll();
		} catch (...) {
			/* ignore control channel errors, the process
			   is gone anyway */
		}
	}

	log.Flush();

	int exit_status = WEXITSTATUS(status);

	if (WIFSIGNALED(status)) {
		logger(1, "died from signal ",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? " (core dumped)" : "");
		exit_status = -1;
	} else if (exit_status == 0)
		logger(3, "exited with success");
	else
		logger(2, "exited with status ", exit_status);

	const char *log_text = log.GetBuffer();
	if (log_text != nullptr && !ValidateUTF8(log_text)) {
		/* TODO: purge illegal UTF-8 sequences instead of
		   replacing the log text? */
		log_text = "Invalid UTF-8 output";
		logger(2, log_text);
	}

	if (again >= std::chrono::seconds())
		job.SetAgain(again, log_text);
	else
		job.SetDone(exit_status, log_text);

	workplace.OnExit(this);
}

std::string
WorkshopOperator::MakeLoggerDomain() const noexcept
{
	return StringFormat<64>("job %s plan=%s pid=%d", job.id.c_str(),
				job.plan_name.c_str(), (pid)).c_str();
}

void
WorkshopOperator::OnControlProgress(unsigned progress) noexcept
{
	OnProgress(progress);
}

void
WorkshopOperator::OnControlSetEnv(const char *s) noexcept
{
	try {
		job.SetEnv(s);
	} catch (...) {
		logger(1, "Failed to 'setenv': ", std::current_exception());
	}
}

void
WorkshopOperator::OnControlAgain(std::chrono::seconds d) noexcept
{
	again = d;
}

void
WorkshopOperator::OnControlTemporaryError(std::exception_ptr ep) noexcept
{
	logger(3, "error on control channel: ", ep);
}

void
WorkshopOperator::OnControlPermanentError(std::exception_ptr ep) noexcept
{
	control_channel.reset();
	logger(3, "error on control channel: ", ep);
}

void
WorkshopOperator::OnControlClosed() noexcept
{
	control_channel.reset();
}
