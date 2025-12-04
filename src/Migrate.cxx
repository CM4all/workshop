// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lib/fmt/RuntimeError.hxx"
#include "pg/Connection.hxx"
#include "pg/Reflection.hxx"
#include "util/PrintException.hxx"
#include "util/StringAPI.hxx"

#include <span>
#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>

static void
MigrateWorkshopDatabase(Pg::Connection &c, const char *schema)
{
	(void)schema;

	/* since Workshop 2.0.13 */
	c.Execute("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS log text NULL");

	/* since Workshop 2.0.23 */
	c.Execute("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS enabled boolean NOT NULL DEFAULT TRUE");

	c.Execute("DROP INDEX IF EXISTS jobs_sorted");
	c.Execute("CREATE INDEX IF NOT EXISTS jobs_sorted2 ON jobs(priority, time_created)"
		  " WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL");

	c.Execute("DROP INDEX IF EXISTS jobs_scheduled");
	c.Execute("CREATE INDEX IF NOT EXISTS jobs_scheduled2 ON jobs(scheduled_time)"
		  " WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL AND scheduled_time IS NOT NULL;");

	c.Execute("CREATE OR REPLACE RULE job_enabled AS ON UPDATE TO jobs"
		  " WHERE NOT OLD.enabled AND NEW.enabled"
		  " AND NEW.node_name IS NULL AND NEW.time_done IS NULL AND NEW.exit_status IS NULL"
		  " DO SELECT pg_notify('new_job', NULL)");

	/* since Workshop 2.0.37 */
	c.Execute("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS env varchar(4096)[] NULL");

	/* since Workshop 4.0.1 */
	c.Execute("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS time_started timestamp NULL");
	c.Execute("CREATE INDEX IF NOT EXISTS jobs_rate_limit ON jobs(plan_name, time_started)");

	/* since Workshop 6.0.1 */
	c.Execute("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS cpu_usage interval NULL");

	/* since Workshop 7.1 */
	c.Execute("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS stdin bytea NULL");

	// since Workshop 7.3
	c.Execute("ALTER TABLE jobs ADD COLUMN IF NOT EXISTS time_modified timestamp NOT NULL DEFAULT now()");
}

static void
MigrateCronDatabase(Pg::Connection &c, const char *schema)
{
	(void)schema;

	/* since Workshop 2.0.25 */
	c.Execute("ALTER TABLE cronjobs"
		  " ADD COLUMN IF NOT EXISTS delay interval SECOND(0) NULL,"
		  " ADD COLUMN IF NOT EXISTS delay_range interval SECOND(0) NULL");

	/* since Workshop 2.0.27: next_run can be 'infinity' for expired @once jobs */
	c.Execute("DROP INDEX IF EXISTS cronjobs_scheduled");
	c.Execute("CREATE INDEX IF NOT EXISTS cronjobs_scheduled2 ON cronjobs(next_run)"
		  " WHERE enabled AND node_name IS NULL"
		  " AND next_run IS NOT NULL AND next_run != 'infinity'");
	c.Execute("CREATE OR REPLACE RULE schedule_cronjob AS ON UPDATE TO cronjobs"
		  " WHERE NEW.enabled AND NEW.node_name IS NULL"
		  " AND NEW.next_run IS NOT NULL AND NEW.next_run != 'infinity' AND ("
		  "  OLD.next_run IS NULL OR"
		  "  NEW.next_run != OLD.next_run"
		  ")"
		  " DO SELECT pg_notify('cronjobs_scheduled', NULL)");

	/* since Workshop 2.0.28 */
	if (Pg::GetColumnType(c, schema, "cronjobs", "next_run") == "timestamp without time zone") {
		/* we need to drop those rules because they reference the
		   columns to be edited, or else PostgreSQL won't allow the
		   change */
		c.Execute("DROP RULE IF EXISTS finish_cronjob ON cronjobs");
		c.Execute("DROP RULE IF EXISTS schedule_cronjob ON cronjobs");

		/* drop the index due to "functions in index predicate must be
		   marked IMMUTABLE" (because the 'infinity' literal was
		   implicitly a "timestamp without timezone") */
		c.Execute("DROP INDEX cronjobs_scheduled2");

		/* add time zones to timestamps */
		c.Execute("ALTER TABLE cronjobs"
			  " ALTER COLUMN last_run TYPE timestamp with time zone,"
			  " ALTER COLUMN next_run TYPE timestamp with time zone,"
			  " ALTER COLUMN node_timeout TYPE timestamp with time zone");

		/* recreate the rules */
		c.Execute("CREATE OR REPLACE RULE finish_cronjob AS ON UPDATE TO cronjobs"
			  " WHERE NEW.enabled AND NEW.node_name IS NULL AND NEW.next_run IS NULL AND OLD.next_run IS NOT NULL"
			  " DO SELECT pg_notify('cronjobs_modified', NULL)");
		c.Execute("CREATE OR REPLACE RULE schedule_cronjob AS ON UPDATE TO cronjobs"
			  " WHERE NEW.enabled AND NEW.node_name IS NULL AND NEW.next_run IS NOT NULL AND ("
			  "   OLD.next_run IS NULL OR"
			  "   NEW.next_run != OLD.next_run"
			  " )"
			  " DO SELECT pg_notify('cronjobs_scheduled', NULL)");

		/* recreate the index */
		c.Execute("CREATE INDEX cronjobs_scheduled2 ON cronjobs(next_run)"
			  " WHERE enabled AND node_name IS NULL"
			  " AND next_run IS NOT NULL AND next_run != 'infinity'");

	}

	if (Pg::GetColumnType(c, schema, "cronresults", "start_time") == "timestamp without time zone") {
		/* add time zones to timestamps */
		c.Execute("ALTER TABLE cronresults"
			  " ALTER COLUMN start_time TYPE timestamp with time zone,"
			  " ALTER COLUMN finish_time TYPE timestamp with time zone");
	}

	c.Execute("CREATE INDEX IF NOT EXISTS cronresults_finished ON cronresults(finish_time)");

	c.Execute("ALTER TABLE cronjobs"
		  " ADD COLUMN IF NOT EXISTS tz varchar(64) NULL");
}

int
main(int argc, char **argv)
try {
	std::span<const char *const> args{argv + 1, static_cast<std::size_t>(argc - 1)};

	const char *set_role = nullptr;

	while (!args.empty() && *args.front() == '-') {
		if (strncmp(args.front(), "--set-role=", 11) == 0) {
			set_role = args.front() + 11;
			args = args.subspan(1);
			if (*set_role == 0)
				throw "Role name missing";
		} else {
			fmt::print(stderr, "Unknown option: {}\n\n", args.front());
			/* clear the list to trigger printing the usage */
			args = {};
		}
	}

	if (args.size() < 1 || args.size() > 2) {
		fmt::print(stderr, "Usage: {} [OPTIONS] CONNINFO [SCHEMA]\n"
			   "\n"
			   "Options:\n"
			   "  --set-role=ROLE       Execute \"SET ROLE ...\"\n"
			   "\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *const conninfo = args.front();
	args = args.subspan(1);
	const char *const schema = args.empty() ? "public" : args.front();
	if (!args.empty())
		args = args.subspan(1);
	assert(args.empty());

	Pg::Connection c(conninfo);

	if (c.GetServerVersion() < 90600)
		throw FmtRuntimeError("PostgreSQL version {:?} is too old, need at least 9.6",
				      c.GetParameterStatus("server_version"));

	if (set_role != nullptr)
		c.SetRole(set_role);

	if (!StringIsEqual(schema, "public"))
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
		throw "No table 'jobs' or 'cronjobs' - not a Workshop/Cron database";

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
