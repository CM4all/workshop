/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Operator.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"
#include "EmailService.hxx"
#include "util/StringFormat.hxx"

CronOperator::CronOperator(CronQueue &_queue, CronWorkplace &_workplace,
                           CronJob &&_job,
                           std::string &&_start_time) noexcept
    :queue(_queue), workplace(_workplace), job(std::move(_job)),
     logger(*this),
     start_time(std::move(_start_time)),
     timeout_event(GetEventLoop(), BIND_THIS_METHOD(OnTimeout))
{
}

EventLoop &
CronOperator::GetEventLoop()
{
    return queue.GetEventLoop();
}

void
CronOperator::Finish(int exit_status, const char *log)
{
    queue.Finish(job);
    queue.InsertResult(job, start_time.c_str(), exit_status, log);

    if (!job.notification.empty()) {
        auto *email_service = workplace.GetEmailService();
        if (email_service != nullptr) {
            // TODO: configurable sender?
            Email email("cm4all-workshop");
            email.AddRecipient(job.notification.c_str());

            char buffer[1024];

            snprintf(buffer, sizeof(buffer),
                     "X-CM4all-Workshop: " VERSION "\n"
                     "X-CM4all-Workshop-Job: %s\n"
                     "X-CM4all-Workshop-Account: %s\n",
                     job.id.c_str(),
                     job.account_id.c_str());
            email.message += buffer;

            if (exit_status >= 0) {
                snprintf(buffer, sizeof(buffer), "X-CM4all-Workshop-Status: %d\n",
                         exit_status);
                email.message += buffer;
            }

            email.message += "\n";

            if (log != nullptr)
                email.message += log;

            email_service->Submit(std::move(email));
        }
    }
}

void
CronOperator::OnTimeout()
{
    logger(2, "Timeout");

    Cancel();
}

std::string
CronOperator::MakeLoggerDomain() const noexcept
{
    return StringFormat<64>("cron job=%s", job.id.c_str()).c_str();
}
