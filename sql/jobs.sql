--
--  Create the "jobs" table
--
--  author: Max Kellermann <mk@cm4all.com>
--

-- DROP TABLE jobs;

CREATE TABLE jobs (
        id SERIAL PRIMARY KEY,

        -- non-unique name of the job
        name varchar(64) NULL,
        -- human readable long description of this job
        description varchar(4096) NULL,

        -- the time this job was created
        time_created timestamp NOT NULL DEFAULT now(),
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

        -- syslog server which receives stderr output
        syslog_server varchar(256) NULL,

        -- which cm4all-workshop node is executing this job?
        node_name varchar(256) NULL,
        -- which time can we assume the node is dead?
        node_timeout timestamp NULL,

        -- how much is done? 0 to 100 percent
        progress int NOT NULL DEFAULT 0,

        -- the time this job was completed
        time_done timestamp NULL,
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
