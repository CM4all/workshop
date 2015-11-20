/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_DATABASE_GLUE_HXX
#define SNOWBALL_DATABASE_GLUE_HXX

#include "pg/Connection.hxx"
#include "event/FunctionEvent.hxx"

#include <inline/compiler.h>

#include <postgresql/libpq-fe.h>

#include <cassert>

class DatabaseGlue;

class DatabaseHandler {
public:
    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;
    virtual void OnNotify(const char *name) = 0;
};

/**
 * A PostgreSQL database connection that connects asynchronously,
 * reconnects automatically and provides an asynchronous notify
 * handler.
 */
class DatabaseGlue : public PgConnection {
    std::string schema;

    DatabaseHandler &handler;

    enum class State {
        DISCONNECTED,
        CONNECTING,
        RECONNECTING,
        READY,
        WAITING,
    };

    State state;

    /**
     * DISCONNECTED: not used.
     *
     * CONNECTING: used by PollConnect().
     *
     * RECONNECTING: used by PollReconnect().
     *
     * READY: used by PollNotify().
     *
     * WAITING: a timer which reconnects.
     */
    FunctionEvent event;

public:
    DatabaseGlue(const char *conninfo, const char *schema,
                 DatabaseHandler &handler);

    ~DatabaseGlue() {
        Disconnect();
    }

    const std::string &GetSchemaName() const {
        return schema;
    }

    bool IsReady() {
        assert(IsDefined());

        return state == State::READY;
    }

    void Reconnect();

    void Disconnect();

    void CheckNotify() {
        if (IsDefined() && IsReady())
            PollNotify();
    }

protected:
    /**
     * This method is called when an fatal error on the connection has
     * occurred.  It will set the state to DISCONNECT, notify the
     * handler, and schedule a reconnect.
     */
    void Error();

    void Poll(PostgresPollingStatusType status);

    void PollConnect();
    void PollReconnect();
    void PollNotify();

    void ScheduleReconnect();

private:
    void OnEvent();
};

#endif
