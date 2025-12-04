--
--  Create the "jobs" table
--
--  author: Max Kellermann <max.kellermann@ionos.com>
--

-- DROP TABLE jobs;

CREATE TABLE jobs (
    --------------------------------
    -- Internal PostgreSQL columns
    --------------------------------

    id SERIAL PRIMARY KEY,

    --------------------------------
    -- UI columns (not used by Workshop)
    --------------------------------

    -- non-unique name of the job
    name varchar(64) NULL,
    -- human readable long description of this job
    description varchar(4096) NULL,

    --------------------------------
    -- Execution parameters
    --------------------------------

    -- the time this job was created
    time_created timestamp NOT NULL DEFAULT now(),
    -- the time this record was last modified
    time_modified timestamp NOT NULL DEFAULT now(),
    -- the job will not be executed before this time
    scheduled_time timestamp NULL,
    -- is this job enabled?
    enabled boolean NOT NULL DEFAULT TRUE,
    -- priority of this job; negative value means higher priority
    priority int NOT NULL DEFAULT 0,

    -- which plan executes this job?
    plan_name varchar(64) NOT NULL,
    -- command line arguments appended to the plan executable
    args varchar(4096)[] NULL,
    -- environment variables in the form NAME=VALUE
    env varchar(4096)[] NULL,
    -- optional data fed into stdin
    stdin bytea NULL,

    --------------------------------
    -- Scheduler control (Workshop internal)
    --------------------------------

    -- which cm4all-workshop node is executing this job?
    node_name varchar(256) NULL,
    -- which time can we assume the node is dead?
    node_timeout timestamp NULL,

    --------------------------------
    -- State / Result / Completion
    --------------------------------

    -- how much is done? 0 to 100 percent
    progress int NOT NULL DEFAULT 0,

    -- the time this job was most recently started
    time_started timestamp NULL,
    -- the time this job was completed
    time_done timestamp NULL,
    -- CPU usage in microseconds
    cpu_usage interval NULL,
    -- the output logged by the process to stderr
    log text NULL,
    -- the process' exit code
    exit_status int NULL
);

-- this index is used when determining the next free job
CREATE INDEX jobs_sorted2 ON jobs(priority, time_created)
    WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL;

-- find scheduled jobs
CREATE INDEX jobs_scheduled2 ON jobs(scheduled_time)
    WHERE enabled AND node_name IS NULL AND time_done IS NULL AND exit_status IS NULL AND scheduled_time IS NOT NULL;

-- for finding jobs to release
CREATE INDEX jobs_release ON jobs(node_name, node_timeout)
    WHERE node_name IS NOT NULL AND time_done IS NULL AND exit_status IS NULL;

-- for finding a job by its name
CREATE INDEX jobs_name ON jobs(name);

-- find recently executed jobs, for checking rate limits
CREATE INDEX IF NOT EXISTS jobs_rate_limit ON jobs(plan_name, time_started);

-- notify all cm4all-workshop daemons when a new job is added
CREATE RULE new_job AS ON INSERT TO jobs DO NOTIFY new_job;

-- notify all cm4all-workshop daemons when a job was enabled (requires PostgreSQL 9.x+)
CREATE OR REPLACE RULE job_enabled AS ON UPDATE TO jobs
    WHERE NOT OLD.enabled AND NEW.enabled
    AND NEW.node_name IS NULL AND NEW.time_done IS NULL AND NEW.exit_status IS NULL
    DO SELECT pg_notify('new_job', NULL);

-- notify all clients when a job was finished (requires PostgreSQL 9.x+)
CREATE OR REPLACE RULE job_done AS ON UPDATE TO jobs
    WHERE OLD.time_done IS NULL AND OLD.exit_status IS NULL
    AND NEW.time_done IS NOT NULL AND NEW.exit_status IS NOT NULL
    DO SELECT pg_notify('job_done', NEW.id::text);
