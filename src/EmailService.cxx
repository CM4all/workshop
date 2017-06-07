/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "EmailService.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/PrintException.hxx"

#include <memory>

EmailService::Job::Job(EmailService &_service, Email &&_email)
    :service(_service),
     email(std::move(_email)),
     connect(service.event_loop, *this),
     client(service.event_loop, *this)
{
}

void
EmailService::Job::Start()
{
    connect.Connect(service.address);
}

void
EmailService::Job::OnSocketConnectSuccess(UniqueSocketDescriptor &&_fd)
{
    client.Begin({email.message.data(), email.message.length()},
                 {email.sender.data(), email.sender.length()});
    for (const auto &i : email.recipients)
        client.AddRecipient({i.data(), i.length()});

    const int fd = _fd.Steal();
    client.Commit(fd, fd);
}

void
EmailService::Job::OnSocketConnectError(std::exception_ptr error)
{
    // TODO add context to log message
    PrintException(error);
    service.DeleteJob(*this);
}

void
EmailService::Job::OnQmqpClientSuccess(StringView description)
{
    // TODO log?
    (void)description;

    service.DeleteJob(*this);
}

void
EmailService::Job::OnQmqpClientError(std::exception_ptr error)
{
    // TODO add context to log message
    PrintException(error);
    service.DeleteJob(*this);
}

EmailService::~EmailService()
{
    CancelAll();
}

void
EmailService::CancelAll()
{
    jobs.clear_and_dispose(DeleteDisposer());
}

void
EmailService::Submit(Email &&email)
{
    auto *job = new Job(*this, std::move(email));
    jobs.push_front(*job);
    job->Start();
}

inline void
EmailService::DeleteJob(Job &job)
{
    jobs.erase_and_dispose(jobs.iterator_to(job), DeleteDisposer());
}
