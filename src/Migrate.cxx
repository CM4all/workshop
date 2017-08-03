/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "pg/Connection.hxx"
#include "pg/Reflection.hxx"
#include "pg/CheckError.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>

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
CheckDropIndex(Pg::Connection &c, const char *schema,
               const char *table_name, const char *index_name,
               const char *drop_statement)
{
    if (!Pg::IndexExists(c, schema, table_name, index_name))
        return;

    printf("Dropping index %s.%s\n", table_name, index_name);
    Pg::CheckError(c.Execute(drop_statement));
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

    CheckDropIndex(c, schema, "jobs", "jobs_sorted", "DROP INDEX jobs_sorted");
    CheckCreateIndex(c, schema, "jobs", "jobs_sorted2",
                     "CREATE INDEX jobs_sorted2 ON jobs(priority, time_created)"
                     " WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL");

    CheckDropIndex(c, schema, "jobs", "jobs_scheduled",
                   "DROP INDEX jobs_scheduled");
    CheckCreateIndex(c, schema, "jobs", "jobs_scheduled2",
                     "CREATE INDEX jobs_scheduled2 ON jobs(scheduled_time)"
                     " WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL AND scheduled_time IS NOT NULL;");

    CheckCreateRule(c, schema, "jobs", "job_enabled",
                    "CREATE OR REPLACE RULE job_enabled AS ON UPDATE TO jobs"
                    " WHERE NOT OLD.enabled AND NEW.enabled"
                    " AND NEW.node_name IS NULL AND NEW.time_done IS NULL AND NEW.exit_status IS NULL"
                    " DO SELECT pg_notify('new_job', NULL)");
}

int
main(int argc, char **argv)
try {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s CONNINFO\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *const conninfo = argv[1];
    const char *const schema = "public";

    Pg::Connection c(conninfo);

    if (Pg::TableExists(c, schema, "jobs")) {
        MigrateWorkshopDatabase(c, schema);
    } else
        printf("No table 'jobs' - not a Workshop database\n");

    return EXIT_SUCCESS;
} catch (const std::exception &e) {
    PrintException(e);
    return EXIT_FAILURE;
}
