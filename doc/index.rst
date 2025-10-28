Workshop
########

What is Workshop?
=================

Workshop is a daemon which executes jobs from a queue stored in a
PostgreSQL database.  Multiple instances can run in parallel on
different hosts.

The jobs can either be one-time executions according to a
preconfigured plan (Workshop classic) or periodic executions managed
by hosted users ("cron").


Installation & Configuration
============================

Workshop (classic):

#. Create a PostgreSQL database for the queue.
#. :samp:`apt-get install cm4all-workshop cm4all-workshop-database`
#. Execute :file:`/usr/share/cm4all/workshop/sql/jobs.sql` in the
   PostgreSQL database to create the `jobs` table.
#. Grant the required permissions to the Workshop daemon: :samp:`GRANT
   SELECT, UPDATE ON jobs TO "cm4all-workshop";`
#. The PostgreSQL user which manages jobs can be configured like this:
   :samp:`GRANT INSERT, SELECT, DELETE ON jobs TO workshop_client;`
   and :samp:`GRANT UPDATE, SELECT ON jobs_id_seq TO workshop_client;`
#. Edit :file:`/etc/cm4all/workshop/workshop.conf` and set the variable
   ``database`` (`PostgreSQL documentation
   <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
#. :samp:`systemctl start cm4all-workshop`

Cron:

#. Create a PostgreSQL database for the queue.
#. :samp:`apt-get install cm4all-workshop`
#. Execute :file:`/usr/share/cm4all/workshop/sql/cronjobs.sql` in the
   PostgreSQL database to create the `cronjobs` table.
#. Grant the required permissions to the Workshop daemon: :samp:`GRANT
   SELECT, UPDATE ON cronjobs TO "cm4all-workshop";`,
   :samp:`GRANT INSERT ON cronresults TO "cm4all-workshop";` and
   :samp:`GRANT UPDATE, SELECT ON cronresults_id_seq TO "cm4all-workshop";`
#. The PostgreSQL user which manages jobs can be configured like this:
   :samp:`GRANT INSERT, SELECT, DELETE ON cronjobs TO cron_client;` and
   :samp:`GRANT UPDATE, SELECT ON cronjobs_id_seq TO cron_client;`
#. Edit :file:`/etc/cm4all/cron/workshop.conf` and set the variable
   ``database`` (`PostgreSQL documentation
   <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
#. :samp:`systemctl start cm4all-workshop`


Settings in :file:`/etc/cm4all/workshop/workshop.conf`
------------------------------------------------------

The file :file:`/etc/cm4all/workshop/workshop.conf` configures Workshop.
The following settings are recognized:

* ``node_name``: This node's name, for example the
  fully-qualified host name.  Must be unique in the cluster.  By
  default, the hostname is used.
* ``concurrency``: How many jobs shall this node execute concurrently?
  Rule of thumb: number of CPUs, not much more.
* ``spawn``: opens a block (with curly braces), which
  configures the process spawner; see :ref:`config.spawn`.

* ``workshop``: opens a block (with curly braces), which configures a
  Workshop classic database.  It may optionally be followed by a
  partition name (right before the opening curly brace).

  * ``database``: the PostgreSQL connect string (`PostgreSQL
    documentation
    <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
  * ``database_schema``: the PostgreSQL schema name (optional)
  * ``max_log``: specifies the maximum amount of log data
    captured for the `log` column (units such as `kB` may be used)
  * ``journal``: set to :samp:`yes` to send structured log
    messages to the systemd journal

.. _workshop_translation_server:

  * ``translation_server``: address the translation server is
    listening to; must start with :file:`/` (absolute path) or
    :file:`@` (abstract socket)
  * ``tag``: a string which will be transmitted to the
    translation server in a ``LISTENER_TAG`` packet (optional)

* ``cron``: opens a block (with curly braces), which
  configures a cron database ("partition"):

  * ``cron`` (the top-level block) may optionally be followed by
    a partition name (right before the opening curly brace), which
    will be passed to the translation server in the ``CRON``
    packet
  * ``tag``: a string which will be transmitted to the
    translation server in a ``LISTENER_TAG`` packet (optional)
  * ``database``: the PostgreSQL connect string (`PostgreSQL
    documentation
    <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
  * ``database_schema``: the PostgreSQL schema name (optional)
  * ``translation_server``: address the translation server is
    listening to; must start with :file:`/` (absolute path) or
    :file:`@` (abstract socket)
  * ``qmqp_server`` (optional): address the QMQP server is
    listening to; it is used for email notifications
  * ``use_qrelay`` (optional): if ``yes``, then connect to the `qrelay
    <https://github.com/CM4all/qrelay/>`__ socket from within the
    container to submit notification emails
  * ``default_email_sender`` (optional): the default envelope sender
    for email notifications
  * ``pond_server`` (optional): send job log messages to this
    Pond server; the ``account_id`` value is passed as "site"

* ``control``: opens a block (with curly braces), which
  configures a control listener (see `Controlling the Daemon`_)

  * ``bind``: The address to bind to. May be the wildcard "*" or
    an IPv4/IPv6 address followed by a port.  IPv6 addresses should be
    enclosed in square brackets to disambiguate the port separator.
    Local sockets start with a slash "/", and abstract sockets start
    with an at symbol "@".
  * ``multicast_group``: Join this multicast group, which allows
    receiving multicast commands.  Value is a multicast IPv4/IPv6
    address.  IPv6 addresses may contain a scope identifier after a
    percent sign ('%').
  * ``interface``: Limit this listener to the given network
    interface.

The default configuration file includes :file:`local.conf` and
:file:`conf.d/*.conf`, and you should probably better edit these files
instead of editing the main :file:`workshop.conf`.

.. _config.spawn:

``spawn``
^^^^^^^^^

Configures the process spawner. Example::

   spawn {
     allow_user "www-data"
     allow_group "www-data"
     CPUWeight "50"
     TasksMax "100"
     MemoryHigh "12 GB"
     MemoryMax "16 GB"
     IOWeight "50"
   }

- ``allow_user``: allow child processes to impersonate the given
  user.  This can be a user name (from :file:`/etc/passwd`), a
  numeric user id or an open range (e.g. `2147483648-` which allows
  all user ids from 2147483648 on).

- ``allow_group``: allow child processes to impersonate the given
  group.

- ``CPUWeight``: CPU weight for all spawned processes combined
  (:math:`1..10000`).  :program:`systemd`'s default is 100.

- ``TasksMax``: maximum number of tasks
  (:math:`1..`). :program:`systemd` sets no limit by default.

- ``MemoryMin``: "If the memory usage of a cgroup is within its
  effective min boundary, the cgroup’s memory won’t be reclaimed under
  any conditions. If there is no unprotected reclaimable memory
  available, OOM killer is invoked."
  (https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html#memory-interface-files)

- ``MemoryLow``: "Best-effort memory protection. If the memory usage
  of a cgroup is within its effective low boundary, the cgroup’s
  memory won’t be reclaimed unless there is no reclaimable memory
  available in unprotected cgroups."
  (https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html#memory-interface-files)

- ``MemoryHigh``: "Specify the throttling limit on memory usage of the
  executed processes in this unit.  Memory usage may go above the
  limit if unavoidable, but the processes are heavily slowed down and
  memory is taken away aggressively in such cases.  This is the main
  mechanism to control memory usage of a unit."
  (:manpage:`systemd.resource-control(5)`)

- ``MemoryMax``: "Specify the absolute limit on memory usage of the
  executed processes in this unit. If memory usage cannot be contained
  under the limit, out-of-memory killer is invoked inside the unit."
  (:manpage:`systemd.resource-control(5)`)

- ``MemorySwapMax``: "Swap usage hard limit. If a cgroup’s swap usage
  reaches this limit, anonymous memory of the cgroup will not be
  swapped out."
  (https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html#memory-interface-files)

- ``IOWeight``: IO weight for all spawned processes combined
  (:math:`1..10000`).  :program:`systemd`'s default is 100.

Memory limits are in bytes and may be postfixed with ``kB``, ``MB``,
``GB`` or ``TB``.  Percent values are relative to total physical
memory.


Settings in :file:`/etc/default/cm4all-workshop`
------------------------------------------------

Additionally, the file :file:`/etc/default/cm4all-workshop` configures
how the Workshop daemon is launched.  The following settings are
recognized:

* :envvar:`OPTIONS`: Other options to be passed to the daemon, for
  example :option:`--verbose`.

This file is Workshop 1.0 legacy, and should not be used anymore.


Migrating from Workhop 1.0.x
----------------------------

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


Database Migration
------------------

Sometimes, new Workshop releases come with changes to the database
schema to allow new features.  For this, Workshop comes with a
migration tool which applies those changes to an existing Workshop
database.

To avoid compatibility problems, first upgrade all Workshop nodes and
stop all daemons.  Then migrate the schema and restart the daemons.

Install the package :file:`cm4all-workshop-migrate`, and run the tool
with the same name.

The regular Workshop user should only have :samp:`SELECT` and
:samp:`UPDATE` permissions on the database, and thus cannot run the
tool.  The easiest solution is to run the tool on the database server
as user :samp:`postgres` (the superuser)::

  su postgres -c 'cm4all-workshop-migrate dbname=workshop'


Concept
=======

The queue (a PostgreSQL table) contains a list of *jobs*.  Every
daemon instance monitors this queue.

Every job refers to a *plan*, which must be installed on the node.
The plan describes how to execute the job.  If a plan is not
installed, the node will ignore jobs referring to that plan.

Every cron job contains a schedule in classic `cron` syntax and a
command line to be executed by the shell (:file:`/bin/sh`).


Using Workshop
==============

Plan Files
----------

The directory :file:`/usr/share/cm4all/workshop/plans/` contains a
text file for each plan.  Example::

  exec /usr/bin/my-plan --foo
  user "bar"
  nice 5

The program :command:`/usr/bin/my-plan` is executed as user `bar` with
a CPU scheduler priority of 5 (10 is the default if not specified).

Instead of ``exec``, you can use ``translate`` to let a translation
server decide how to execute the job::

  translate

The following options are available:

* :samp:`exec PROGRAM ARG1 ...`: Command line.  The program path must
  be absolute, because Workshop will not consider the :envvar:`PATH`.

* :samp:`translate`: Can be used instead of ``exec``.  Queries the
  configured translation server for information on how to execute the
  job process.

  This sends a translation request with the following packets:

  - ``EXECUTE`` (no payload)
  - ``SERVICE=workshop``
  - ``PLAN=<name>``: name of the plan
  - ``TAG=<tag>``: value of the ``tag`` configuration option (only if
    one is configured
  - ``APPEND=<arg>``: one packet for each ``jobs.args`` item

  The plan may not contain any other process execute options, because
  that will be decided by the translation server.

  The ``control_channel`` option is allowed, but not ``allow_spawn``.

* :samp:`control_channel`: see `Control Channel`_.

.. _allow_spawn:

* :samp:`allow_spawn`: allow this plan to :ref:`spawn <spawn>` more
  child processes through the `Control Channel`_.

* :samp:`timeout INTERVAL`: A timeout for this plan.  If the process
  does not finish or update its state within this time span, it is
  assumed to be dead (the timer is reset after each update);
  the process will be killed and the job will be
  released, to be executed by another node.  Example: :samp:`20
  minutes` or :samp:`2 hours`.

* :samp:`reap_finished INTERVAL`: Finished jobs will be deleted by
  Workshop after the specified duration (though there is no guarantee
  when Workshop will actually do it).  By default, Workshop will never
  delete finished jobs.  Example: :samp:`1 hour` or :samp:`2 days`.

* :samp:`user USERNAME`: The name of the UNIX user which is
  impersonated by the process.  `root` is not allowed.

* :samp:`umask OCTAL`: Sets the process umask.  The value is an octal
  number starting with `0`.

* :samp:`nice PRIO`: The CPU scheduler priority, ranging from
  :samp:`-20` (high priority) to :samp:`+19` (low priority).  Negative
  values should be avoided.  The default is :samp:`+10`.

* :samp:`sched_idle`: Select the "idle" CPU scheduling policy,
  i.e. the process will only get CPU time when no other process runs.
  (With this policy, the `nice` value is ignored.)

* :samp:`ioprio_idle`: Select the "idle" I/O scheduling class,
  i.e. the process will only be able to access local hard disks when no
  other process needs them.  (Works only with I/O schedulers which
  support it, e.g. `cfq`, and has no effect on NFS.  Check
  :file:`/sys/block/*/queue/scheduler` to see which I/O scheduler is
  used for a specific device.)

* :samp:`idle`: Shortcut for `sched_idle` and `ioprio_idle`.  In this
  mode, the process should not affect the server's performance, even
  if it is a heavy workload.  It will only run when the server is
  idle, and no other tasks need resources.

* :samp:`private_network`: Run the process in an empty network
  namespace.  It can only use its own private loopback interface and
  has no network access to the outside world or even the regular
  loopback interface.

* :samp:`private_tmp`: Mount an empty ``tmpfs`` on ``/tmp``.  Its
  contents will be deleted automatically as soon as the process exits.

* :samp:`rlimits L`: Configure resource limits.  The syntax is the
  same as the `beng-proxy` ``RLIMITS`` translation packet.
  Check its documentation for details.

* :samp:`chroot PATH`: Change the root directory prior to executing
  the process.

* :samp:`concurrency NUM`: Limit the number of processes of this
  plan.  The global concurrency setting is still obeyed.

* :samp:`rate_limit "MAX/INTERVAL"`: Limit the rate in which this plan
  is going to be executed.  This rate is cluster-global and the
  interval is rolling.  Example: ":samp:`20 / 15 minutes`" allows no
  more than 20 executions within 15 minutes.  A plan may have multiple
  rate limits.

In the :samp:`exec` line, the following variables in the form
:samp:`${NAME}` are expanded:

* :envvar:`NODE`: Name of the Workshop node which executes the job.
* :envvar:`JOB`: Id of the job database record.
* :envvar:`PLAN`: Plan name.

Debian packages which install Workshop plans shall trigger
``cm4all-workshop-reload-plans``.  This can be done by writing the
following line to the file :file:`debian/PACKAGENAME.triggers`::

  activate cm4all-workshop-reload-plans

Queueing a job
--------------

A job consists of a row in the PostgreSQL table.  Example::

  INSERT INTO jobs(plan_name,args)
  VALUES('foo', ARRAY['--bar', 'vol01/foo/bar'])

During job execution, the columns `node_name` and `progress` are set.
Upon completion, the columns `time_done` and `status` contain
interesting data.

Development
-----------

The package :file:`cm4all-workshop-dev` contains the tool
:file:`cm4all-workshop-run-job` which allows running a job on the
command line without a Workshopm daemon and without a database.  This
may help during plan/job development.

Example::

  cm4all-workshop-run-job /usr/bin/my-plan --foo
  cm4all-workshop-run-job --control-channel /usr/bin/my-plan --foo

The option :option:`--control-channel` enables the control channel
which behaves like the plan option :samp:`control_channel`; without
it, the legacy protocol is enabled which reads progress values from a
pipe connected to the child's ``stdout``.


Using the systemd journal
=========================

If the `journal` option is enabled, then all log output from job
processes (text lines printed to `stderr`) are forwarded to the
systemd journal, along with structured data:

* ``WORKSHOP_PLAN``: the plan name
* ``WORKSHOP_JOB``: the job id

To see all fields, choose output format `verbose` or `json`::

  journalctl -u cm4all-workshop -o verbose

For example, to see all log messages of plan `foo`, type::

  journalctl -u cm4all-workshop WORKSHOP_PLAN=foo

To see the log of job `42`, type::

  journalctl -u cm4all-workshop WORKSHOP_JOB=42


Using Cron
==========

A cron job consists of a row in the PostgreSQL table.  Example::

  INSERT INTO cronjobs(account_id, schedule, command)
  VALUES('foo', '*/15 * * * *', 'echo Hello World');

During job execution, the column `node_name` is set.


Controlling the Daemon
======================

The ``control`` block in the configuration file sets up a
control listener.  The :file:`cm4all-workshop-control` program can
then be used to send control commands to the daemon.  Most commands
are only allowed when issued over a local socket by the *root* user.

The following commands are implemented:

* :samp:`nop`: No-op, does nothing.

.. note::

   This feature is not to be confused with the `Control Channel`_,
   which is a way for a job process to control its (re)execution.


Reference
=========

Plan Protocol
-------------

The environment is empty.  There are only two file handles: 1
(standard output, `stdout`) and 2 (standard error, `stderr`).  0
(standard input) is not usable; it may point to :file:`/dev/null`.

The process writes its progress to `stdout`, i.e. an integer number
between 0 and 100 per line.  At the end of a line, Workshop writes
this number into the job's database row.  (If the plan enables the
`Control Channel`_, then this feature is disabled, and the control
channel shall be used instead.)

The process may log errors and other messages to `stderr`.  They will
be logged to Workshop's journal.  Additionally, the log will be copied
to the job's `log` column.

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
----------------

* ``id``: The primary key.
* ``name``: An optional name assigned by the job creator.  Not
  used by Workshop.
* ``description``: Human readable description.  Not used by
  Workshop.
* ``time_created``: The time stamp when this job was created.
* ``scheduled_time``: The time when the job will be executed.
  The database server's clock is the authoritative reference.
* ``enabled``: If :samp:`FALSE`, this job will not be scheduled
  until somebody reverts the value to :samp:`TRUE`.
* ``priority``: Smaller number means higher priority.  Default
  is 0.
* ``plan_name``: The name of the plan which is used to execute
  this job.
* ``args``: Additional command-line arguments for the plan.
* ``env``: Additional environment variables.  Some dangerous
  environment variables cannot be set, though, for example
  ``LD_PRELOAD``.
* ``stdin``: If not ``NULL``, then the process is started with a file
  handle on standard input that reads this data.
* ``node_name``: Name of the node which is currently executing
  this job, or :samp:`NULL`.
* ``node_timeout``: When this time stamp has passed, then the
  executing node is assumed to be dead, and the record can be released
  and reassigned to another node.
* ``progress``: Progress of job execution in percent.  Note that
  you cannot assume the job is done when this number reaches 100.
* ``time_started``: Time stamp when the job has most recently
  started execution.
* ``time_done``: Time stamp when the job has completed
  execution.
* ``cpu_usage``: total CPU usage (user + system) of the job.
* ``log``: Log data written by the job to `stderr`.
* ``exit_status``: Exit code of the plan process.  Negative when
  the process was killed by a signal.

To find out whether a job is done, check the column
``time_done`` or ``exit_status`` on :samp:`NOT NULL`.  To
wait for completion, listen on PostgreSQL notify ``job_done``
(:samp:`LISTEN job_done`).  Its payload is the id of the job record.

Old records of completed jobs are not deleted by Workshop.  The
creator may find useful information here, and he is responsible for
deleting it.

The client is allowed to execute the following operations:

* Create new jobs (only ``name``, ``description``,
  ``scheduled_time``, ``enabled``, ``priority``,
  ``plan_name``, ``args`` may be set).
* Modify jobs which have not yet been assigned, i.e. :samp:`node_name
  IS NULL`.  Afterwards, send the notify ``new_job``, so
  Workshop gets notified of the change.
* Delete jobs which have not yet been assigned, i.e.  :samp:`node_name
  IS NULL`.
* Delete jobs which have been completed, i.e.  :samp:`time_done
  IS NOT NULL`.

Control Channel
---------------

With the ``control_channel`` option enabled, the child process
gets a SEQPACKET socket on file descriptor 3.  It can be used to
communicate with Workshop.

A datagram contains a brief text message.  The first word is the
command, and may be followed by space-separated parameters.

The following commands are available:

* :samp:`version`: Query the Workshop version number.  Workshop
  replies with a datagram containing :samp:`version 2.0.36` (for
  example).

* :samp:`progress VALUE`: update the job progress, which Workshop will
  write to the `progress` column.  (Note that the old `stdout`
  protocol for submitting job progress is disabled if there is a
  control channel.)

* :samp:`setenv NAME=VALUE`: Add another environment variable for the
  next execution via :samp:`again`.

* :samp:`again [SECONDS]`: execute the job again (which may occur on a
  different node).  The optional parameter specifies how many seconds
  shall pass at least; if present, then ``scheduled_time`` will
  be updated.

.. _spawn:

* :samp:`spawn TOKEN [PARAM]`: Spawn a new child process.  This
  queries the translation server, passing :samp:`EXECUTE=<token>`,
  :samp:`PARAM=<param>`, :samp:`PLAN=<plan name>`,
  :samp:`SERVICE="workshop"`.  Its response is expected to contain
  :samp:`EXECUTE=<executable>` etc., or :samp:`STATUS=<error code>`
  and :samp:`MESSAGE=<error message>` on error.  After spawning the
  child process, Workshop replies with a datagram containing
  :samp:`ok` and a pidfd the client can ``poll()`` on to wait for it
  to exit; or containing :samp:`error <error message>`.

  This command is only available if the plan's :ref:`allow_spawn
  <allow_spawn>` option is set and a :ref:`translation_server
  <workshop_translation_server>` was configured.


Cron Schedule
-------------

The ``schedule`` column follows the classic `cron` schedule
syntax (see :manpage:`crontab(5)`).

The following special time specification "nicknames" are supported:

- :samp:`@yearly`
- :samp:`@annually`
- :samp:`@monthly`
- :samp:`@weekly`
- :samp:`@daily`
- :samp:`@hourly`

(:samp:`@reboot` is not supported.)

Additionally, :samp:`hourly` can be preceded by a positive integral
number to multiply the interval; e.g. :samp:`@2hourly` runs every two
hours and :samp:`@24hourly` is equal to :samp:`@daily`.  This allows
running a job in custom intervals without having to hard-code the
minute - e.g. :samp:`2hourly` is better than ":samp:`0 */2 * * *`"
because it allows Workshop to choose an arbitrary delay within those
two hours.

The special schedule ":samp:`@once`" can be used to execute a job once
instead of periodically.  It will be executed as soon as possible, and
never again.

To avoid hogging the servers with too many concurrent cron jobs, a
random delay is added to the scheduled execution time.  The randomized
delay depends on the schedule; e.g. ":samp:`@hourly`" will be delayed
up to an hour and ":samp:`@daily`" will be delayed up to one day.

The `cronjobs` table
--------------------

* ``id``: The primary key.
* ``account_id``: The user account which owns this job.  This
  gets passed to the translation server to determine the process
  parameters.
* ``schedule``: A :manpage:`crontab(5)`-like schedule.
* ``tz``: A time zone which is used to calculate the given
  schedule.  This can be any `time zone understood by PostgreSQL
  <https://www.postgresql.org/docs/current/static/datatype-datetime.html#DATATYPE-TIMEZONES>`_.
  A :samp:`NULL` value selects the UTC time zone.
* ``command``: A command to be executed by :file:`/bin/sh`.  If
  it starts with :samp:`http://` or :samp:`https://`, a HTTP GET
  request is sent instead of spawning a child process.  If it starts
  with :samp:`urn:`, then that URN will be passed to the translation
  server as ``URI`` payload, and the response must contain
  ``EXECUTE`` (may be followed by ``APPEND``)
* ``translate_param``: An opaque parameter to be passed to the
  translation server.
* ``enabled``: The cron job is never run when not enabled.
* ``overlapping``: If false, then there is only ever one running
  process at a time.
* ``notification``: An email address which gets notified after
  each completion.
* ``last_run``: Time stamp of the most recent run (internal, do
  not use).
* ``next_run``: Time stamp of the next run (internal, do
  not use).
* ``node_name``: Name of the node which is currently executing
  this job, or :samp:`NULL`.
* ``node_timeout``: When this time stamp has passed, then the
  executing node is assumed to be dead, and the record can be released
  and reassigned to another node.
* ``description``: Human readable description.  Not used by
  Cron.

The client is allowed to execute the following operations:

* Create new jobs.
* Update the schedule.  This operation may clear the
  ``next_run`` column so the scheduler reevaluates the new
  schedule without waiting for the previous schedule to fire
  next time.  This is strictly necessary for ":samp:`@once`"
  schedules.
* Enable/disable jobs by modifying the ``enabled`` flag.  This
  does not cancel any running process, it only affects future
  scheduling.
* Update other columns such as ``command``, ``translate_param``,
  ``notification``, ``description``.
* Delete jobs which are currently not running, i.e. :samp:`node_name
  IS NULL`.

Modifying jobs which are currently running should be avoided if
possible; Workshop then tries to continue without affecting the
current execution, and will attempt to apply the new settings after
finishing.

The `cronresults` table
-----------------------

* ``id``: The primary key.
* ``cronjob_id``: A reference to the `cronjobs` record.
* ``node_name``: Name of the node which executed this job.
* ``start_time``: A time stamp when execution started.
* ``finish_time``: A time stamp when execution finished.
* ``exit_status``: The process exit code or the HTTP response
  status.  A value of `-1` indicates an internal error.
* ``log``: Text written by the process to `stdout`/`stderr` or
  the HTTP response body.

The client is allowed to execute the following operations:

* Delete records.


State Directories
-----------------

- :file:`workshop/workshop/NAME/enabled` (boolean): enable or disable
  the (classic) Workshop partition with the specified name.

- :file:`workshop/cron/NAME/enabled` (boolean): enable or disable the
  Cron partition with the specified name.

For a description of this feature, read https://state.readthedocs.io/


Security
========

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
----

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
