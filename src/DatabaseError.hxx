/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_DATABASE_ERROR_HXX
#define SNOWBALL_DATABASE_ERROR_HXX

#include "DatabaseResult.hxx"

class DatabaseError {
    DatabaseResult result;

public:
    DatabaseError(const DatabaseError &other) = delete;
    DatabaseError(DatabaseError &&other)
        :result(std::move(other.result)) {}
    explicit DatabaseError(DatabaseResult &&_result)
        :result(std::move(_result)) {}

    DatabaseResult &operator=(const DatabaseResult &other) = delete;

    DatabaseError &operator=(DatabaseError &&other) {
        result = std::move(other.result);
        return *this;
    }

    DatabaseError &operator=(DatabaseResult &&other) {
        result = std::move(other);
        return *this;
    }

    gcc_pure
    ExecStatusType GetStatus() const {
        return result.GetStatus();
    }

    gcc_pure
    const char *GetMessage() const {
        return result.GetErrorMessage();
    }
};

#endif
