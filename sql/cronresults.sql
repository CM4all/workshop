--
--  Create the "cronresults" table
--
--  author: Max Kellermann <max.kellermann@ionos.com>
--

CREATE TABLE cronresults (
    --------------------------------
    -- Internal PostgreSQL columns
    --------------------------------

    id serial PRIMARY KEY,

    --------------------------------
    -- Custom columns
    --------------------------------

    cronjob_id integer NOT NULL REFERENCES cronjobs(id) ON DELETE CASCADE,

    -- which node has executed this job?
    node_name varchar(256) NULL,

    -- the time this job execution was started
    start_time timestamp with time zone NOT NULL,

    -- the time this job execution was completed
    finish_time timestamp with time zone NOT NULL DEFAULT now(),

    -- the process' exit code
    exit_status INT NOT NULL,

    -- the output logged by the process to stdout/stderr (if enabled)
    log text NULL
);

CREATE INDEX cronresults_job ON cronresults(cronjob_id);

-- For pruning old rows
CREATE INDEX cronresults_finished ON cronresults(finish_time);
