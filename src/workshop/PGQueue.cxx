/*
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "PGQueue.hxx"
#include "pg/Connection.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

bool
pg_listen(Pg::Connection &db)
{
    const auto result = db.Execute("LISTEN new_job");
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "LISTEN new_job failed: %s\n",
                result.GetErrorMessage());
        return false;
    }

    return true;
}

bool
pg_notify(Pg::Connection &db)
{
    const auto result = db.Execute("NOTIFY new_job");
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "NOTIFY new_job failed: %s\n",
                result.GetErrorMessage());
        return false;
    }

    return true;
}

int
pg_release_jobs(Pg::Connection &db, const char *node_name)
{
    const auto result =
        db.ExecuteParams("UPDATE jobs "
                         "SET node_name=NULL, node_timeout=NULL, progress=0 "
                         "WHERE node_name=$1 AND time_done IS NULL AND exit_status IS NULL",
                         node_name);
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/claim on jobs failed: %s\n",
                result.GetErrorMessage());
        return -1;
    }

    return result.GetAffectedRows();
}

int
pg_expire_jobs(Pg::Connection &db, const char *except_node_name)
{
    const auto result =
        db.ExecuteParams("UPDATE jobs "
                         "SET node_name=NULL, node_timeout=NULL, progress=0 "
                         "WHERE time_done IS NULL AND exit_status IS NULL AND "
                         "node_name IS NOT NULL AND node_name <> $1 AND "
                         "node_timeout IS NOT NULL AND now() > node_timeout",
                         except_node_name);
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/expire on jobs failed: %s\n",
                result.GetErrorMessage());
        return -1;
    }

    return result.GetAffectedRows();
}

int
pg_next_scheduled_job(Pg::Connection &db, bool has_enabled_column,
                      const char *plans_include,
                      long *span_r)
{
    assert(plans_include != nullptr && *plans_include == '{');

    char sql[1024] = "SELECT EXTRACT(EPOCH FROM (MIN(scheduled_time) - now())) "
        "FROM jobs WHERE node_name IS NULL AND time_done IS NULL AND exit_status IS NULL "
        "AND scheduled_time IS NOT NULL "

        /* ignore jobs which are scheduled deep into
           the future; some Workshop clients (such as
           URO) do this, and it slows down the
           PostgreSQL query */
        "AND scheduled_time < now() + '1 year'::interval "

        "AND plan_name = ANY ($1::TEXT[])";

    if (has_enabled_column)
        strcat(sql, " AND enabled");

    const auto result = db.ExecuteParams(sql, plans_include);
    if (!result.IsQuerySuccessful()) {
        fprintf(stderr, "SELECT on jobs failed: %s\n",
                result.GetErrorMessage());
        return -1;
    }

    if (result.IsEmpty())
        return 0;

    const char *value = result.GetValue(0, 0);
    if (value == nullptr || *value == 0)
        return 0;

    *span_r = strtol(value, nullptr, 0);
    return 1;
}

Pg::Result
pg_select_new_jobs(Pg::Connection &db, bool has_enabled_column,
                   const char *plans_include, const char *plans_exclude,
                   const char *plans_lowprio,
                   unsigned limit)
{
    assert(plans_include != nullptr && *plans_include == '{');
    assert(plans_exclude != nullptr && *plans_exclude == '{');
    assert(plans_lowprio != nullptr && *plans_lowprio == '{');

    char sql[1024] = "SELECT id,plan_name,args,syslog_server "
        "FROM jobs "
        "WHERE node_name IS NULL "
        "AND time_done IS NULL AND exit_status IS NULL "
        "AND (scheduled_time IS NULL OR now() >= scheduled_time) "
        "AND plan_name = ANY ($1::TEXT[]) "
        "AND plan_name <> ALL ($2::TEXT[] || $3::TEXT[]) ";

    if (has_enabled_column)
        strcat(sql, "AND enabled ");

    strcat(sql, "ORDER BY priority, time_created "
           "LIMIT $4");

    auto result =
        db.ExecuteParams(sql,
                         plans_include, plans_exclude, plans_lowprio, limit);
    if (!result.IsQuerySuccessful())
        fprintf(stderr, "SELECT on jobs failed: %s\n",
                result.GetErrorMessage());

    return result;
}

int
pg_claim_job(Pg::Connection &db, bool has_enabled_column,
             const char *job_id, const char *node_name,
             const char *timeout)
{
    char sql[1024] = "UPDATE jobs "
        "SET node_name=$1, node_timeout=now()+$3::INTERVAL "
        "WHERE id=$2 AND node_name IS NULL";

    if (has_enabled_column)
        strcat(sql, " AND enabled");

    const auto result =
        db.ExecuteParams(sql, node_name, job_id, timeout);
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/claim on jobs failed: %s\n",
                result.GetErrorMessage());
        return -1;
    }

    return result.GetAffectedRows();
}

int
pg_set_job_progress(Pg::Connection &db, const char *job_id,
                    unsigned progress, const char *timeout)
{
    const auto result =
        db.ExecuteParams("UPDATE jobs "
                         "SET progress=$2, node_timeout=now()+$3::INTERVAL "
                         "WHERE id=$1",
                         job_id, progress, timeout);
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/progress on jobs failed: %s\n",
                result.GetErrorMessage());
        return -1;
    }

    return result.GetAffectedRows();
}

int
pg_rollback_job(Pg::Connection &db, const char *id)
{
    const auto result =
        db.ExecuteParams("UPDATE jobs "
                         "SET node_name=NULL, node_timeout=NULL, progress=0 "
                         "WHERE id=$1 AND node_name IS NOT NULL "
                         "AND time_done IS NULL",
                         id);
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/done on jobs failed: %s\n",
                result.GetErrorMessage());
        return -1;
    }

    return result.GetAffectedRows();
}

int
pg_set_job_done(Pg::Connection &db, const char *id, int status,
                const char *log)
{
    const auto result = log != nullptr
        ? db.ExecuteParams("UPDATE jobs "
                           "SET time_done=now(), progress=100, exit_status=$2, log=$3 "
                           "WHERE id=$1",
                           id, status, log)
        /* keep the old UPDATE statement, because the "log" column is
           optional */
        : db.ExecuteParams("UPDATE jobs "
                           "SET time_done=now(), progress=100, exit_status=$2 "
                           "WHERE id=$1",
                           id, status);
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/done on jobs failed: %s\n",
                result.GetErrorMessage());
        return -1;
    }

    return result.GetAffectedRows();
}
