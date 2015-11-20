/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_RESULT_ITERATOR_HXX
#define PG_RESULT_ITERATOR_HXX

#include "Result.hxx"

#include <cassert>
#include <utility>

/**
 * This class helps iterating over the rows of a #PgResult.
 */
class PgResultIterator : public PgResult {
    unsigned current_row;

public:
    PgResultIterator() = default;
    explicit PgResultIterator(PGresult *_result)
        :PgResult(_result), current_row(0) {}
    explicit PgResultIterator(PgResult &&_result)
        :PgResult(std::move(_result)), current_row(0) {}

    PgResultIterator &operator=(PgResult &&other) {
        PgResult::operator=(std::move(other));
        current_row = 0;
        return *this;
    }

    PgResultIterator &operator=(PgResultIterator &&other) {
        PgResult::operator=(std::move(other));
        current_row = other.current_row;
        return *this;
    }

    PgResultIterator &operator++() {
        ++current_row;
        return *this;
    }

    gcc_pure
    operator bool() const {
        return current_row < GetRowCount();
    }

    gcc_pure
    const char *GetValue(unsigned column) const {
        assert(current_row < GetRowCount());

        return PgResult::GetValue(current_row, column);
    }

    gcc_pure
    unsigned GetValueLength(unsigned column) const {
        assert(current_row < GetRowCount());

        return PgResult::GetValueLength(current_row, column);
    }

    gcc_pure
    bool IsValueNull(unsigned column) const {
        assert(current_row < GetRowCount());

        return PgResult::IsValueNull(current_row, column);
    }

    gcc_pure
    PgBinaryValue GetBinaryValue(unsigned column) const {
        assert(current_row < GetRowCount());

        return PgResult::GetBinaryValue(current_row, column);
    }
};

#endif
