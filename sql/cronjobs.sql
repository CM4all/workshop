--
--  Create the "cronjobs" table
--
--  author: Max Kellermann <mk@cm4all.com>
--

CREATE TABLE cronjobs (
    --------------------------------
    -- Internal PostgreSQL columns
    --------------------------------

    id serial PRIMARY KEY,

    --------------------------------
    -- Execution parameters
    --------------------------------

    -- the owner's account id
    account_id varchar(32) NOT NULL,

    -- the schedule in crontab(5) syntax
    schedule varchar(256) NOT NULL,

    -- the command to be executed by /bin/sh
    command varchar(1024) NOT NULL,

    -- a parameter for the translation server
    translate_param varchar(1024) NULL,

    -- is this cronjob enabled?
    enabled boolean NOT NULL DEFAULT TRUE,

    -- shall overlapped execution of slow instances be allowed?
    overlapping boolean NOT NULL DEFAULT TRUE,

    -- email address to receive notification
    notification varchar(512) NULL,

    --------------------------------
    -- Scheduler state/control (calculated)
    --------------------------------

    -- a random delay which is applied to the raw schedule values
    delay interval SECOND(0) NULL,

    -- the range which was used to generate the "delay" value (to
    -- detect whether a new delay is necessary)
    delay_range interval SECOND(0) NULL,

    -- the last time this job was run; NULL means never
    last_run timestamp with time zone NULL,

    -- the next time this job will run; this was calculated from the schedule
    next_run timestamp with time zone NULL,

    -- which node is executing this job?
    node_name varchar(256) NULL,
    -- which time can we assume the node is dead?
    node_timeout timestamp with time zone NULL,

    --------------------------------
    -- UI columns
    --------------------------------

    -- human readable long description of this job
    description varchar(4096) NULL
);

-- find scheduled jobs
CREATE INDEX cronjobs_scheduled2 ON cronjobs(next_run)
    WHERE enabled AND node_name IS NULL
    AND next_run IS NOT NULL AND next_run != 'infinity';

-- for finding jobs to release
CREATE INDEX cronjobs_release ON cronjobs(node_name, node_timeout)
    WHERE node_name IS NOT NULL;

-- notify all nodes when a new cronjob is added or edited
CREATE OR REPLACE RULE new_cronjob AS ON INSERT TO cronjobs
    WHERE NEW.enabled AND NEW.node_name IS NULL
    DO SELECT pg_notify('cronjobs_modified', NULL);

CREATE OR REPLACE RULE edit_cronjob AS ON UPDATE TO cronjobs
    WHERE NEW.enabled AND NEW.node_name IS NULL AND (
      NOT OLD.enabled
      OR
      NEW.schedule != OLD.schedule
    )
    DO SELECT pg_notify('cronjobs_modified', NULL);

-- notify all nodes when a cronjob has finished, to run the scheduler
CREATE OR REPLACE RULE finish_cronjob AS ON UPDATE TO cronjobs
    WHERE NEW.enabled AND NEW.node_name IS NULL AND NEW.next_run IS NULL AND OLD.next_run IS NOT NULL
    DO SELECT pg_notify('cronjobs_modified', NULL);

-- notify all nodes when a cronjob has been scheduled
CREATE OR REPLACE RULE schedule_cronjob AS ON UPDATE TO cronjobs
    WHERE NEW.enabled AND NEW.node_name IS NULL
    AND NEW.next_run IS NOT NULL AND NEW.next_run != 'infinity' AND (
      OLD.next_run IS NULL OR
      NEW.next_run != OLD.next_run
    )
    DO SELECT pg_notify('cronjobs_scheduled', NULL);
