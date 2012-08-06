/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_DATABASE_RESULT_HXX
#define SNOWBALL_DATABASE_RESULT_HXX

#include <inline/compiler.h>

#include <postgresql/libpq-fe.h>

#include <cassert>
#include <cstdlib>
#include <string>

/**
 * A thin C++ wrapper for a PGresult pointer.
 */
class DatabaseResult {
    PGresult *result;

public:
    DatabaseResult():result(nullptr) {}
    explicit DatabaseResult(PGresult *_result):result(_result) {}

    DatabaseResult(const DatabaseResult &other) = delete;
    DatabaseResult(DatabaseResult &&other):result(other.result) {
        other.result = nullptr;
    }

    ~DatabaseResult() {
        if (result != nullptr)
            ::PQclear(result);
    }

    bool IsDefined() const {
        return result != nullptr;
    }

    DatabaseResult &operator=(const DatabaseResult &other) = delete;
    DatabaseResult &operator=(DatabaseResult &&other) {
        if (result != nullptr)
            ::PQclear(result);
        result = other.result;
        other.result = nullptr;
        return *this;
    }

    gcc_pure
    ExecStatusType GetStatus() const {
        assert(IsDefined());

        return ::PQresultStatus(result);
    }

    gcc_pure
    bool IsCommandSuccessful() const {
        return GetStatus() == PGRES_COMMAND_OK;
    }

    gcc_pure
    bool IsQuerySuccessful() const {
        return GetStatus() == PGRES_TUPLES_OK;
    }

    gcc_pure
    bool IsError() const {
        const auto status = GetStatus();
        return status == PGRES_BAD_RESPONSE ||
            status == PGRES_NONFATAL_ERROR ||
            status == PGRES_FATAL_ERROR;
    }

    gcc_pure
    const char *GetErrorMessage() const {
        assert(IsDefined());

        return ::PQresultErrorMessage(result);
    }

    /**
     * Returns the number of rows that were affected by the command.
     * The caller is responsible for checking GetStatus().
     */
    gcc_pure
    unsigned GetAffectedRows() const {
        assert(IsDefined());
        assert(IsCommandSuccessful());

        return std::strtoul(::PQcmdTuples(result), NULL, 10);
    }

    /**
     * Returns true if there are no rows in the result.
     */
    gcc_pure
    bool IsEmpty() const {
        assert(IsDefined());

        return ::PQntuples(result) == 0;
    }

    gcc_pure
    unsigned GetRowCount() const {
        assert(IsDefined());

        return ::PQntuples(result);
    }

    gcc_pure
    unsigned GetColumnCount() const {
        assert(IsDefined());

        return ::PQnfields(result);
    }

    gcc_pure
    const char *GetColumnName(unsigned column) const {
        assert(IsDefined());

        return ::PQfname(result, column);
    }

    gcc_pure
    const char *GetValue(unsigned row, unsigned column) const {
        assert(IsDefined());

        return ::PQgetvalue(result, row, column);
    }

    gcc_pure
    unsigned GetValueLength(unsigned row, unsigned column) const {
        assert(IsDefined());

        return ::PQgetlength(result, row, column);
    }

    gcc_pure
    bool IsValueNull(unsigned row, unsigned column) const {
        assert(IsDefined());

        return ::PQgetisnull(result, row, column);
    }

    /**
     * Returns the only value (row 0, column 0) from the result.
     * Returns an empty string if the result is not valid or if there
     * is no row or if the value is NULL.
     */
    gcc_pure
    std::string GetOnlyStringChecked() const;
};

#endif
