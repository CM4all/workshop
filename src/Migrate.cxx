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
MigrateWorkshopDatabase(Pg::Connection &c, const char *schema)
{
    /* since Workshop 2.0.13 */
    CheckCreateColumn(c, schema, "jobs", "log",
                      "ALTER TABLE jobs ADD COLUMN log text NULL");
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
