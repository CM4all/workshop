/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "DatabaseGlue.hxx"
#include "event/Callback.hxx"

DatabaseGlue::DatabaseGlue(const char *conninfo, const char *_schema,
                           DatabaseHandler &_handler)
    :schema(_schema),
     handler(_handler), state(State::CONNECTING),
     event(-1, 0, MakeSimpleEventCallback(DatabaseGlue, OnEvent), this)
{
    StartConnect(conninfo);
    PollConnect();
}

void
DatabaseGlue::Error()
{
    assert(state == State::CONNECTING ||
           state == State::RECONNECTING ||
           state == State::READY);

    event.Delete();

    const bool was_connected = state == State::READY;
    state = State::DISCONNECTED;

    if (was_connected)
        handler.OnDisconnect();

    ScheduleReconnect();
}

void
DatabaseGlue::Poll(PostgresPollingStatusType status)
{
    switch (status) {
    case PGRES_POLLING_FAILED:
        handler.OnError("Failed to connect to database", GetErrorMessage());
        Error();
        break;

    case PGRES_POLLING_READING:
        event.Set(GetSocket(), EV_READ,
                  MakeSimpleEventCallback(DatabaseGlue, OnEvent), this);
        event.Add();
        break;

    case PGRES_POLLING_WRITING:
        event.Set(GetSocket(), EV_WRITE,
                  MakeSimpleEventCallback(DatabaseGlue, OnEvent), this);
        event.Add();
        break;

    case PGRES_POLLING_OK:
        if (!schema.empty() &&
            (state == State::CONNECTING || state == State::RECONNECTING) &&
            !SetSchema(schema.c_str())) {
            handler.OnError("Failed to set schema", GetErrorMessage());
            Error();
            break;
        }

        state = State::READY;
        event.Set(GetSocket(), EV_READ|EV_PERSIST,
                  MakeSimpleEventCallback(DatabaseGlue, OnEvent), this);
        event.Add();

        handler.OnConnect();

        /* check the connection status, just in case the handler
           method has done awful things */
        if (state == State::READY &&
            GetStatus() == CONNECTION_BAD)
            Error();
        break;

    case PGRES_POLLING_ACTIVE:
        /* deprecated enum value */
        assert(false);
        break;
    }
}

void
DatabaseGlue::PollConnect()
{
    assert(IsDefined());
    assert(state == State::CONNECTING);

    Poll(PgConnection::PollConnect());
}

void
DatabaseGlue::PollReconnect()
{
    assert(IsDefined());
    assert(state == State::RECONNECTING);

    Poll(PgConnection::PollReconnect());
}

void
DatabaseGlue::PollNotify()
{
    assert(IsDefined());
    assert(state == State::READY);

    ConsumeInput();

    PgNotify notify;
    switch (GetStatus()) {
    case CONNECTION_OK:
        while ((notify = GetNextNotify()))
            handler.OnNotify(notify->relname);
        break;

    case CONNECTION_BAD:
        Error();
        break;

    default:
        break;
    }
}

void
DatabaseGlue::Reconnect()
{
    event.Delete();
    StartReconnect();
    state = State::RECONNECTING;
    PollReconnect();
}

void
DatabaseGlue::Disconnect()
{
    event.Delete();
    PgConnection::Disconnect();
    state = State::DISCONNECTED;
}

void
DatabaseGlue::ScheduleReconnect()
{
    /* attempt to reconnect every 10 seconds */
    static constexpr struct timeval delay{ 10, 0 };

    assert(IsDefined());
    assert(state == State::DISCONNECTED);

    state = State::WAITING;
    event.SetTimer(MakeSimpleEventCallback(DatabaseGlue, OnReconnectTimer),
                   this);
    event.Add(delay);
}

inline void
DatabaseGlue::OnEvent()
{
    switch (state) {
    case State::DISCONNECTED:
    case State::WAITING:
        assert(false);
        gcc_unreachable();

    case State::CONNECTING:
        PollConnect();
        break;

    case State::RECONNECTING:
        PollReconnect();
        break;

    case State::READY:
        PollNotify();
        break;
    }
}

inline void
DatabaseGlue::OnReconnectTimer()
{
    assert(state == State::WAITING);

    Reconnect();
}
