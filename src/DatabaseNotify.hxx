/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_DATABASE_NOTIFY_HXX
#define SNOWBALL_DATABASE_NOTIFY_HXX

#include <inline/compiler.h>

#include <postgresql/libpq-fe.h>

/**
 * A thin C++ wrapper for a PGnotify pointer.
 */
class DatabaseNotify {
    PGnotify *notify;

public:
    DatabaseNotify():notify(nullptr) {}
    explicit DatabaseNotify(PGnotify *_notify):notify(_notify) {}

    DatabaseNotify(const DatabaseNotify &other) = delete;
    DatabaseNotify(DatabaseNotify &&other):notify(other.notify) {
        other.notify = nullptr;
    }

    ~DatabaseNotify() {
        if (notify != nullptr)
            PQfreemem(notify);
    }

    operator bool() const {
        return notify != nullptr;
    }

    DatabaseNotify &operator=(const DatabaseNotify &other) = delete;
    DatabaseNotify &operator=(DatabaseNotify &&other) {
        if (notify != nullptr)
            PQfreemem(notify);
        notify = other.notify;
        other.notify = nullptr;
        return *this;
    }

    const PGnotify &operator*() const {
        return *notify;
    }

    const PGnotify *operator->() const {
        return notify;
    }
};

#endif
