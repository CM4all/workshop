/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef EMAIL_SERVICE_HXX
#define EMAIL_SERVICE_HXX

#include "net/AllocatedSocketAddress.hxx"
#include "event/net/ConnectSocket.hxx"
#include "event/net/djb/QmqpClient.hxx"

#include <boost/intrusive/list.hpp>

#include <string>
#include <forward_list>

struct Email {
    std::string sender;
    std::forward_list<std::string> recipients;
    std::string message;

    explicit Email(const char *_sender)
        :sender(_sender) {}

    void AddRecipient(const char *recipient) {
        recipients.emplace_front(recipient);
    }
};

class EmailService {
    EventLoop &event_loop;
    const AllocatedSocketAddress address;

    class Job final
        : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
          ConnectSocketHandler, QmqpClientHandler {
        EmailService &service;
        Email email;

        ConnectSocket connect;
        QmqpClient client;

    public:
        Job(EmailService &_service, Email &&_email);
        void Start();

    private:
        /* virtual methods from ConnectSocketHandler */
        void OnSocketConnectSuccess(UniqueSocketDescriptor &&fd) override;
        void OnSocketConnectError(std::exception_ptr error) override;

        /* virtual methods from QmqpClientHandler */
        void OnQmqpClientSuccess(StringView description) override;
        void OnQmqpClientError(std::exception_ptr error) override;
    };

    typedef boost::intrusive::list<Job,
                                   boost::intrusive::constant_time_size<false>> JobList;

    JobList jobs;

public:
    EmailService(EventLoop &_event_loop, SocketAddress _address)
        :event_loop(_event_loop), address(_address) {}

    ~EmailService();

    void CancelAll();

    void Submit(Email &&email);

private:
    void DeleteJob(Job &job);
};

#endif
