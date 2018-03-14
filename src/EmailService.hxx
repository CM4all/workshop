/*
 * Copyright 2006-2018 Content Management AG
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
