Workshop
========

What is Workshop?
-----------------

Workshop is a daemon which executes jobs from a queue stored in a
PostgreSQL database.  Multiple instances can run in parallel on
different hosts.

The jobs can either be one-time executions according to a
preconfigured plan (Workshop classic) or periodic executions managed
by hosted users ("cron").


Installation & Configuration
----------------------------

Workshop (classic):

#. Create a PostgreSQL database for the queue.
#. :samp:`apt-get install cm4all-workshop cm4all-workshop-database`
#. Execute :file:`/usr/share/cm4all/workshop/sql/jobs.sql` in the
   PostgreSQL database to create the `jobs` table.
#. Grant the required permissions to the Workshop daemon: :samp:`GRANT
   SELECT, UPDATE ON jobs TO "cm4all-workshop";`
#. The PostgreSQL user which manages jobs can be configured like this:
   :samp:`GRANT INSERT, SELECT, DELETE ON jobs TO workshop_client;`
   and :samp:`GRANT UPDATE, SELECT ON jobs_id_seq TO workshop_client`
#. Edit :file:`/etc/cm4all/workshop/workshop.conf` and set the variable
   :envvar:`database` (`PostgreSQL documentation
   <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
#. :samp:`systemctl start cm4all-workshop`

Cron:

#. Create a PostgreSQL database for the queue.
#. :samp:`apt-get install cm4all-workshop`
#. Execute :file:`/usr/share/cm4all/workshop/sql/cronjobs.sql` in the
   PostgreSQL database to create the `cronjobs` table.
#. Grant the required permissions to the Workshop daemon: :samp:`GRANT
   SELECT, UPDATE ON cronjobs TO "cm4all-workshop";`
#. The PostgreSQL user which manages jobs can be configured like this:
   :samp:`GRANT INSERT, SELECT, DELETE ON cronjobs TO cron_client;` and
   :samp:`GRANT UPDATE, SELECT ON cronjobs_id_seq TO cron_client`
#. Edit :file:`/etc/cm4all/cron/workshop.conf` and set the variable
   :envvar:`database` (`PostgreSQL documentation
   <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
#. :samp:`systemctl start cm4all-workshop`

Settings in :file:`/etc/cm4all/workshop/workshop.conf`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The file :file:`/etc/cm4all/workshop/workshop.conf` configures Workshop.
The following settings are recognized:

* :envvar:`node_name`: This node's name, for example the
  fully-qualified host name.  Must be unique in the cluster.  By
  default, the hostname is used.
* :envvar:`concurrency`: How many jobs shall this node concurrently?
  Rule of thumb: number of CPUs, not much more.
* :envvar:`spawn`: opens a block (with curly braces), which
  configures the process spawner:

  * :envvar:`allow_user`: allow child processes to impersonate the
    given user
  * :envvar:`allow_group`: allow child processes to impersonate the
    given group
* :envvar:`workshop`: opens a block (with curly braces), which
  configures a Workshop classic database:

  * :envvar:`database`: the PostgreSQL connect string (`PostgreSQL
    documentation
    <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
  * :envvar:`database_schema`: the PostgreSQL schema name (optional)
* :envvar:`cron`: opens a block (with curly braces), which
  configures a cron database:

  * :envvar:`database`: the PostgreSQL connect string (`PostgreSQL
    documentation
    <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
  * :envvar:`database_schema`: the PostgreSQL schema name (optional)

The default configuration file includes :file:`local.conf` and
:file:`conf.d/*.conf`, and you should probably better edit these files
instead of editing the main :file:`workshop.conf`.

Settings in :file:`/etc/default/cm4all-workshop`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Additionally, the file :file:`/etc/default/cm4all-workshop` configures
how the Workshop daemon is launched.  The following settings are
recognized:

* :envvar:`OPTIONS`: Other options to be passed to the daemon, for
  example :option:`--verbose`.

This file is Workshop 1.0 legacy, and should not be used anymore.

Migrating from Workhop 1.0.x
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In Workshop 1.0.x, all configuration options were in the shell-style
file :file:`/etc/default/cm4all-workshop`.  That format lacks
flexibility and complexity needed for new features in version 2.
Therefore, version 2 introduced the structured configuration file
:file:`/etc/cm4all/workshop/workshop.conf`.

Loggers can no longer be configured, because Workshop now relies on
systemd and its Journal.

Move :envvar:`CONCURRENCY` to :file:`workshop.conf` as
:envvar:`concurrency` (lower case).

Move :envvar:`DATABASE` to :file:`workshop.conf` as :envvar:`database`
in a :envvar:`workshop` section, e.g.::

  workshop {
    database "dbname=workshop"
  }

For security reasons, Workshop version 2 requires you to specify which
users and groups can be impersonated by job processes::

  spawn {
    allow_user hans
    allow_user foo
    allow_group bar
  }


Concept
-------

The queue (a PostgreSQL table) contains a list of *jobs*.  Every
daemon instance monitors this queue.

Every job refers to a *plan*, which must be installed on the node.
The plan describes how to execute the job.  If a plan is not
installed, the node will ignore jobs referring to that plan.

Every cron job contains a schedule in classic `cron` syntax and a
command line to be executed by the shell (:file:`/bin/sh`).


Using Workshop
--------------

The directory :file:`/usr/share/cm4all/workshop/plans/` contains a
text file for each plan.  Example::

  exec /usr/bin/my-plan --foo
  user "bar"
  nice 5

The program :command:`/usr/bin/my-plan` is executed as user `bar` with
a CPU scheduler priority of 5 (10 is the default if not specified).

The following options are available:

* :samp:`exec PROGRAM ARG1 ...`: Command line.  The program path must
  be absolute, because Workshop will not consider the :envvar:`PATH`.

* :samp:`timeout INTERVAL`: A timeout for this plan.  If the process
  does not finish or update its state within this time span, it is
  assumed to be dead; the process will be killed and the job will be
  released, to be executed by another node.  Example: :samp:`20
  minutes` or :samp:`2 hours`.

* :samp:`user USERNAME`: The name of the UNIX user which is
  impersonated by the process.  `root` is not allowed.

* :samp:`nice PRIO`: The CPU scheduler priority, ranging from
  :samp:`-20` (high priority) to :samp:`+19` (low priority).  Negative
  values should be avoided.  The default is :samp:`+10`.

* :samp:`chroot PATH`: Change the root directory prior to executing
  the process.

* :samp:`concurrency NUM`: Limit the number of processes of this
  plan.  The global concurrency setting is still obeyed.

In the :samp:`exec` line, the following variables in the form
:samp:`${NAME}` are expanded:

* :envvar:`NODE`: Name of the Workshop node which executes the job.
* :envvar:`JOB`: Id of the job database record.
* :envvar:`PLAN`: Plan name.

Queueing a job
^^^^^^^^^^^^^^

A job consists of a row in the PostgreSQL table.  Example::

  INSERT INTO jobs(plan_name,args)
  VALUES('foo', ARRAY['--bar', 'vol01/foo/bar'])

During job execution, the columns `node_name` and `progress` are set.
Upon completion, the columns `time_done` and `status` contain
interesting data.

Using Cron
----------

A cron job consists of a row in the PostgreSQL table.  Example::

  INSERT INTO cronjobs(account_id, schedule, command)
  VALUES('foo', '*/15 * * * *', 'echo Hello World');

During job execution, the column `node_name` is set.

Reference
---------

Plan Protocol
^^^^^^^^^^^^^

The environment is empty.  There are only two file handles: 1
(standard output, `stdout`) and 2 (standard error, `stderr`).  0
(standard input) is not usable; it may point to :file:`/dev/null`.

The process writes its progress to `stdout`, i.e. an integer number
between 0 and 100 per line.  At the end of a line, Workshop writes
this number into the job's database row.

The process may log errors and other messages to `stderr`.  They will
be forwarded to the configured syslog server, or will be logged to
Workshop's journal.

Upon successful completion, the process exits with status 0.

Workshop attempts to execute a job exactly once.  Under certain rare
circumstances, a job can be executed twice (e.g. when the network, the
database or the executing host fails).  A well-written plan should be
reasonably safe when executed twice.

Plans should operate atomic whenever possible.  For example, files
should be written to a temporary path name first, and only renamed to
the final name after all data is committed (or with
:samp:`O_TMPFILE`).

The plan should clean up after itself in any case (e.g. delete its
temporary files), whether successful or not.

The `jobs` table
^^^^^^^^^^^^^^^^

* :envvar:`id`: The primary key.
* :envvar:`name`: An optional name assigned by the job creator.  Not
  used by Workshop.
* :envvar:`description`: Human readable description.  Not used by
  Workshop.
* :envvar:`time_created`: The time stamp when this job was created.
* :envvar:`scheduled_time`: The time when the job will be executed.
  The database server's clock is the authoritative reference.
* :envvar:`priority`: Smaller number means higher priority.  Default
  is 0.
* :envvar:`plan_name`: The name of the plan which is used to execute
  this job.
* :envvar:`args`: Additional command-line arguments for the plan.
* :envvar:`syslog_server`: If this column is not :samp:`NULL`, then
  all `stderr` lines are sent to this address with the syslog protocol
  (see :rfc:`3164`)
* :envvar:`node_name`: Name of the node which is currently executing
  this job, or :samp:`NULL`.
* :envvar:`node_timeout`: When this time stamp has passed, then the
  executing node is assumed to be dead, and the record can be released
  and reassigned to another node.
* :envvar:`progress`: Progress of job execution in percent.  Note that
  you cannot assume the job is done when this number reaches 100.
* :envvar:`time_done`: Time stamp when the job has completed
  execution.
* :envvar:`exit_status`: Exit code of the plan process.  Negative when
  the process was killed by a signal.

To find out whether a job is done, check the column
:envvar:`time_done` or :envvar:`exit_status` on :samp:`NOT NULL`.  To
wait for completion, listen on PostgreSQL notify :envvar:`job_done`
(:samp:`LISTEN job_done`).  Its payload is the id of the job record.

Old records of completed jobs are not deleted by Workshop.  The
creator may find useful information here, and he is responsible for
deleting it.

The client is allowed to execute the following operations:

* Create new jobs (only :envvar:`name`, :envvar:`description`,
  :envvar:`scheduled_time`, :envvar:`priority`, :envvar:`plan_name`,
  :envvar:`args`, :envvar:`syslog_server` may be set).
* Modify jobs which have not yet been assigned, i.e. :samp:`node_name
  IS NULL`.  Afterwards, send the notify :envvar:`new_job`, so
  Workshop gets notified of the change.
* Delete jobs which have not yet been assigned, i.e.  :samp:`node_name
  IS NULL`.
* Delete jobs which have been completed, i.e.  :samp:`time_done
  IS NOT NULL`.

Cron Schedule
^^^^^^^^^^^^^

The :envvar:`schedule` column follows the classic `cron` schedule
syntax (see :manpage:`crontab(5)`), though the special `@` strings are
not yet implemented.

The `cronjobs` table
^^^^^^^^^^^^^^^^^^^^

* :envvar:`id`: The primary key.
* :envvar:`account_id`: The user account which owns this job.  This
  gets passed to the translation server to determine the process
  parameters.
* :envvar:`schedule`: A :manpage:`crontab(5)`-like schedule.
* :envvar:`command`: A command to be executed by :file:`/bin/sh`.  If
  it starts with :samp:`http://` or :samp:`https://`, a HTTP GET
  request is sent instead of spawning a child process.
* :envvar:`translate_param`: An opaque parameter to be passed to the
  translation server.
* :envvar:`enabled`: The cron job is never run when not enabled.
* :envvar:`overlapping`: If false, then there is only ever one running
  process at a time.
* :envvar:`notification`: An email address which gets notified after
  each completion.
* :envvar:`last_run`: Time stamp of the most recent run (internal, do
  not use).
* :envvar:`next_run`: Time stamp of the next run (internal, do
  not use).
* :envvar:`node_name`: Name of the node which is currently executing
  this job, or :samp:`NULL`.
* :envvar:`node_timeout`: When this time stamp has passed, then the
  executing node is assumed to be dead, and the record can be released
  and reassigned to another node.
* :envvar:`description`: Human readable description.  Not used by
  Cron.

Security
--------

Workshop is a service which executes programs based on data stored in
a database.  That concept is potentially dangerous, when the database
has been compromised.

This makes not Workshop the target of an attack; the plans are.  They
should be designed in a way which makes an attack by job injection
impossible.  The job arguments should be validated.  Jobs should not
be able to pass arbitrary file paths, but codes and ids which can be
validated.  No generic interfaces which manipulate data, but only very
concrete procedures to apply one well-defined specific job.  Processes
should run with the least privileges possible to reduce the potential
damage from a successful attack.

The plan author is responsible for the security of his plan.

Cron
^^^^

This service executes programs based on data stored in a database.
That concept is potentially dangerous when the database has been
compromised.

This software is designed so that untrusted clients can add new cron
jobs with arbitrary commands.  It is very hard to make that secure.
The process spawner incorporated here gives you many tools to secure
the child processes, controlled by the translation server.  The
`beng-proxy` documentation gives more details about which security
features are available.

However, these security features are only effective if the Linux
kernel is secure.  One single kernel security vulnerability can easily
compromise a Cron server remotely.  It is important to always run the
latest stable kernel with all known bugs fixed.
