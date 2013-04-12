/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_DATABASE_CONNECTION_HXX
#define SNOWBALL_DATABASE_CONNECTION_HXX

#include "ParamWrapper.hxx"
#include "DynamicParamWrapper.hxx"
#include "DatabaseResult.hxx"
#include "DatabaseNotify.hxx"

#include <inline/compiler.h>

#include <postgresql/libpq-fe.h>

#include <new>
#include <memory>
#include <string>
#include <cassert>

/**
 * A thin C++ wrapper for a PGconn pointer.
 */
class DatabaseConnection {
    PGconn *conn;

public:
    DatabaseConnection():conn(nullptr) {}
    DatabaseConnection(const DatabaseConnection &other) = delete;

    DatabaseConnection(DatabaseConnection &&other):conn(other.conn) {
        other.conn = nullptr;
    }

    DatabaseConnection &operator=(const DatabaseConnection &other) = delete;

    DatabaseConnection &operator=(DatabaseConnection &&other) {
        if (conn != nullptr)
            ::PQfinish(conn);

        conn = other.conn;
        other.conn = nullptr;

        return *this;
    }

    ~DatabaseConnection() {
        Disconnect();
    }

    bool IsDefined() const {
        return conn != nullptr;
    }

    gcc_pure
    ConnStatusType GetStatus() const {
        assert(IsDefined());

        return ::PQstatus(conn);
    }

    gcc_pure
    const char *GetErrorMessage() const {
        assert(IsDefined());

        return ::PQerrorMessage(conn);
    }

    gcc_pure
    int GetProtocolVersion() const {
        assert(IsDefined());

        return ::PQprotocolVersion (conn);
    }

    gcc_pure
    int GetServerVersion() const {
        assert(IsDefined());

        return ::PQserverVersion (conn);
    }

    gcc_pure
    int GetBackendPID() const {
        assert(IsDefined());

        return ::PQbackendPID (conn);
    }

    gcc_pure
    int GetSocket() const {
        assert(IsDefined());

        return ::PQsocket(conn);
    }

    void Disconnect() {
        if (conn != nullptr) {
            ::PQfinish(conn);
            conn = nullptr;
        }
    }

    void StartConnect(const char *conninfo) {
        assert(!IsDefined());

        conn = ::PQconnectStart(conninfo);
        if (conn == nullptr)
            throw std::bad_alloc();
    }

    PostgresPollingStatusType PollConnect() {
        assert(IsDefined());

        return ::PQconnectPoll(conn);
    }

    void StartReconnect() {
        assert(IsDefined());

        ::PQresetStart(conn);
    }

    PostgresPollingStatusType PollReconnect() {
        assert(IsDefined());

        return ::PQresetPoll(conn);
    }

    void ConsumeInput() {
        assert(IsDefined());

        ::PQconsumeInput(conn);
    }

    DatabaseNotify GetNextNotify() {
        assert(IsDefined());

        return DatabaseNotify(::PQnotifies(conn));
    }

protected:
    DatabaseResult CheckResult(PGresult *result) {
        if (result == nullptr)
            throw std::bad_alloc();

        return DatabaseResult(result);
    }

    template<size_t i, typename... Params>
    DatabaseResult ExecuteParams3(bool result_binary,
                                  const char *query,
                                  const char *const*values) {
        assert(IsDefined());
        assert(query != nullptr);

        return CheckResult(::PQexecParams(conn, query, i,
                                          nullptr, values, nullptr, nullptr,
                                          result_binary));
    }

    template<size_t i, typename T, typename... Params>
    DatabaseResult ExecuteParams3(bool result_binary,
                                  const char *query, const char **values,
                                  const T &t, Params... params) {
        assert(IsDefined());
        assert(query != nullptr);

        ParamWrapper<T> p(t);
        values[i] = p.GetValue();

        return ExecuteParams3<i + 1, Params...>(result_binary, query,
                                                values, params...);
    }

    static size_t CountDynamic() {
        return 0;
    }

    template<typename T, typename... Params>
    static size_t CountDynamic(const T &t, Params... params) {
        return DynamicParamWrapper<T>::Count(t) + CountDynamic(params...);
    }

    DatabaseResult ExecuteDynamic2(const char *query,
                                   const char *const*values,
                                   unsigned n) {
        assert(IsDefined());
        assert(query != nullptr);

        return CheckResult(::PQexecParams(conn, query, n,
                                          nullptr, values, nullptr, nullptr,
                                          false));
    }

    template<typename T, typename... Params>
    DatabaseResult ExecuteDynamic2(const char *query,
                                   const char **values,
                                   unsigned n,
                                   const T &t, Params... params) {
        assert(IsDefined());
        assert(query != nullptr);

        const DynamicParamWrapper<T> w(t);
        n += w.Fill(values + n);

        return ExecuteDynamic2(query, values, n, params...);
    }

public:
    DatabaseResult Execute(const char *query) {
        assert(IsDefined());
        assert(query != nullptr);

        return CheckResult(::PQexec(conn, query));
    }

    template<typename... Params>
    DatabaseResult ExecuteParams(bool result_binary,
                                 const char *query, Params... params) {
        assert(IsDefined());
        assert(query != nullptr);

        constexpr size_t n = sizeof...(Params);
        const char *values[n];

        return ExecuteParams3<0, Params...>(result_binary, query,
                                            values, params...);
    }

    template<typename... Params>
    DatabaseResult ExecuteParams(const char *query, Params... params) {
        return ExecuteParams(false, query, params...);
    }

    /**
     * Execute with dynamic parameter list: this variant of
     * ExecuteParams() allows std::vector arguments which get
     * expanded.
     */
    template<typename... Params>
    DatabaseResult ExecuteDynamic(const char *query, Params... params) {
        assert(IsDefined());
        assert(query != nullptr);

        const size_t n = CountDynamic(params...);
        std::unique_ptr<const char *[]> values(new const char *[n]);

        return ExecuteDynamic2<Params...>(query, values.get(), 0,
                                          params...);
    }

    bool BeginSerializable() {
        const auto result = Execute("BEGIN ISOLATION LEVEL SERIALIZABLE");
        return result.IsCommandSuccessful();
    }

    bool Commit() {
        const auto result = Execute("COMMIT");
        return result.IsCommandSuccessful();
    }

    bool Rollback() {
        const auto result = Execute("ROLLBACK");
        return result.IsCommandSuccessful();
    }

    gcc_pure
    bool IsBusy() const {
        assert(IsDefined());

        return ::PQisBusy(conn) != 0;
    }

    bool SendQuery(const char *query) {
        assert(IsDefined());
        assert(query != nullptr);

        return ::PQsendQuery(conn, query) != 0;
    }

    DatabaseResult ReceiveResult() {
        assert(IsDefined());

        return CheckResult(::PQgetResult(conn));
    }

    gcc_pure
    std::string Escape(const char *p, size_t length) const;

    gcc_pure
    std::string Escape(const char *p) const;

    gcc_pure
    std::string Escape(const std::string &p) const {
        return Escape(p.data(), p.length());
    }
};

#endif
