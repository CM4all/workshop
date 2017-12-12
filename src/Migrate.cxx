/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "pg/Connection.hxx"
#include "pg/Reflection.hxx"
#include "pg/CheckError.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void
CheckCreateColumn(Pg::Connection &c, const char *schema,
                  const char *table_name, const char *column_name,
                  const char *alter_statement)
{
    if (Pg::ColumnExists(c, schema, table_name, column_name))
        return;

    printf("Creating column %s.%s\n", table_name, column_name);
    Pg::CheckError(c.Execute(alter_statement));
}

static void
CheckCreateIndex(Pg::Connection &c, const char *schema,
                 const char *table_name, const char *index_name,
                 const char *create_statement)
{
    if (Pg::IndexExists(c, schema, table_name, index_name))
        return;

    printf("Creating index %s.%s\n", table_name, index_name);
    Pg::CheckError(c.Execute(create_statement));
}

static void
CheckCreateRule(Pg::Connection &c, const char *schema,
                const char *table_name, const char *rule_name,
                const char *create_statement)
{
    if (Pg::RuleExists(c, schema, table_name, rule_name))
        return;

    printf("Creating rule %s.%s\n", table_name, rule_name);
    Pg::CheckError(c.Execute(create_statement));
}

static void
MigrateWorkshopDatabase(Pg::Connection &c, const char *schema)
{
    /* since Workshop 2.0.13 */
    CheckCreateColumn(c, schema, "jobs", "log",
                      "ALTER TABLE jobs ADD COLUMN log text NULL");

    /* since Workshop 2.0.23 */
    CheckCreateColumn(c, schema, "jobs", "enabled",
                      "ALTER TABLE jobs ADD COLUMN enabled boolean NOT NULL DEFAULT TRUE");

    Pg::CheckError(c.Execute("DROP INDEX IF EXISTS jobs_sorted"));
    CheckCreateIndex(c, schema, "jobs", "jobs_sorted2",
                     "CREATE INDEX jobs_sorted2 ON jobs(priority, time_created)"
                     " WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL");

    Pg::CheckError(c.Execute("DROP INDEX IF EXISTS jobs_scheduled"));
    CheckCreateIndex(c, schema, "jobs", "jobs_scheduled2",
                     "CREATE INDEX jobs_scheduled2 ON jobs(scheduled_time)"
                     " WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL AND scheduled_time IS NOT NULL;");

    CheckCreateRule(c, schema, "jobs", "job_enabled",
                    "CREATE OR REPLACE RULE job_enabled AS ON UPDATE TO jobs"
                    " WHERE NOT OLD.enabled AND NEW.enabled"
                    " AND NEW.node_name IS NULL AND NEW.time_done IS NULL AND NEW.exit_status IS NULL"
                    " DO SELECT pg_notify('new_job', NULL)");
}

static void
MigrateCronDatabase(Pg::Connection &c, const char *schema)
{
    (void)schema;

    /* since Workshop 2.0.25 */
    Pg::CheckError(c.Execute("ALTER TABLE cronjobs"
                             " ADD COLUMN IF NOT EXISTS delay interval SECOND(0) NULL,"
                             " ADD COLUMN IF NOT EXISTS delay_range interval SECOND(0) NULL"));

    /* since Workshop 2.0.27: next_run can be 'infinity' for expired @once jobs */
    Pg::CheckError(c.Execute("DROP INDEX IF EXISTS cronjobs_scheduled"));
    Pg::CheckError(c.Execute("CREATE INDEX IF NOT EXISTS cronjobs_scheduled2 ON cronjobs(next_run)"
                             " WHERE enabled AND node_name IS NULL"
                             " AND next_run IS NOT NULL AND next_run != 'infinity'"));
    Pg::CheckError(c.Execute("CREATE OR REPLACE RULE schedule_cronjob AS ON UPDATE TO cronjobs"
                             " WHERE NEW.enabled AND NEW.node_name IS NULL"
                             " AND NEW.next_run IS NOT NULL AND NEW.next_run != 'infinity' AND ("
                             "  OLD.next_run IS NULL OR"
                             "  NEW.next_run != OLD.next_run"
                             ")"
                             " DO SELECT pg_notify('cronjobs_scheduled', NULL)"));

    /* since Workshop 2.0.28 */
    if (Pg::GetColumnType(c, schema, "cronjobs", "next_run") == "timestamp without time zone") {
        /* we need to drop those rules because they reference the
           columns to be edited, or else PostgreSQL won't allow the
           change */
        Pg::CheckError(c.Execute("DROP RULE IF EXISTS finish_cronjob ON cronjobs"));
        Pg::CheckError(c.Execute("DROP RULE IF EXISTS schedule_cronjob ON cronjobs"));

        /* drop the index due to "functions in index predicate must be
           marked IMMUTABLE" (because the 'infinity' literal was
           implicitly a "timestamp without timezone") */
        Pg::CheckError(c.Execute("DROP INDEX cronjobs_scheduled2"));

        /* add time zones to timestamps */
        Pg::CheckError(c.Execute("ALTER TABLE cronjobs"
                                 " ALTER COLUMN last_run TYPE timestamp with time zone,"
                                 " ALTER COLUMN next_run TYPE timestamp with time zone,"
                                 " ALTER COLUMN node_timeout TYPE timestamp with time zone"));

        /* recreate the rules */
        Pg::CheckError(c.Execute("CREATE OR REPLACE RULE finish_cronjob AS ON UPDATE TO cronjobs"
                                 " WHERE NEW.enabled AND NEW.node_name IS NULL AND NEW.next_run IS NULL AND OLD.next_run IS NOT NULL"
                                 " DO SELECT pg_notify('cronjobs_modified', NULL)"));
        Pg::CheckError(c.Execute("CREATE OR REPLACE RULE schedule_cronjob AS ON UPDATE TO cronjobs"
                                 " WHERE NEW.enabled AND NEW.node_name IS NULL AND NEW.next_run IS NOT NULL AND ("
                                 "   OLD.next_run IS NULL OR"
                                 "   NEW.next_run != OLD.next_run"
                                 " )"
                                 " DO SELECT pg_notify('cronjobs_scheduled', NULL)"));

        /* recreate the index */
        Pg::CheckError(c.Execute("CREATE INDEX cronjobs_scheduled2 ON cronjobs(next_run)"
                                 " WHERE enabled AND node_name IS NULL"
                                 " AND next_run IS NOT NULL AND next_run != 'infinity'"));

    }

    if (Pg::GetColumnType(c, schema, "cronresults", "start_time") == "timestamp without time zone") {
        /* add time zones to timestamps */
        Pg::CheckError(c.Execute("ALTER TABLE cronresults"
                                 " ALTER COLUMN start_time TYPE timestamp with time zone,"
                                 " ALTER COLUMN finish_time TYPE timestamp with time zone"));
    }

    Pg::CheckError(c.Execute("ALTER TABLE cronjobs"
                             " ADD COLUMN IF NOT EXISTS tz varchar(64) NULL"));
}

int
main(int argc, char **argv)
try {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s CONNINFO [SCHEMA]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *const conninfo = argv[1];
    const char *const schema = argc >= 3 ? argv[2] : "public";

    Pg::Connection c(conninfo);

    if (strcmp(schema, "public") != 0)
        c.SetSchema(schema);

    bool found_table = false;

    if (Pg::TableExists(c, schema, "jobs")) {
        found_table = true;
        MigrateWorkshopDatabase(c, schema);
    }

    if (Pg::TableExists(c, schema, "cronjobs")) {
        found_table = true;
        MigrateCronDatabase(c, schema);
    }

    if (!found_table)
        throw std::runtime_error("No table 'jobs' or 'cronjobs' - not a Workshop/Cron database");

    return EXIT_SUCCESS;
} catch (const std::exception &e) {
    PrintException(e);
    return EXIT_FAILURE;
}
