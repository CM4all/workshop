--
--  $Id$
--
--  Create the "jobs" table
--
--  author: Max Kellermann <mk@cm4all.com>
--

DROP TABLE jobs;
CREATE TABLE jobs (
        id SERIAL,

        -- human readable long description of this job
        description VARCHAR(4096) NULL,

        -- the time this job was created
        time_created TIMESTAMP NOT NULL DEFAULT NOW(),
        -- the job will not be executed before this time
        scheduled_time TIMESTAMP NULL,
        -- priority of this job; negative value means higher priority
        priority INT NOT NULL DEFAULT 0,

        -- which plan executes this job?
        plan_name VARCHAR(64) NOT NULL,
        -- command line arguments appended to the plan executable
        args VARCHAR(4096)[] NULL,

        -- syslog server which receives stderr output
        syslog_server VARCHAR(256) NULL,

        -- which cm4all-workshop node is executing this job?
        node_name VARCHAR(256) NULL,
        -- which time can we assume the node is dead?
        node_timeout TIMESTAMP NULL,

        -- how much is done? 0 to 100 percent
        progress INT NOT NULL DEFAULT 0,

        -- the time this job was completed
        time_done TIMESTAMP NULL,
        -- the process' exit code
        exit_status INT NULL
);

-- this index is used when determining the next free job
CREATE INDEX jobs_sorted ON jobs(priority, time_created);

-- notify all cm4all-workshop daemons when a new job is added
CREATE RULE new_job AS ON INSERT TO jobs DO NOTIFY new_job;
