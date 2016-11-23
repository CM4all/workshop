Cron
====

What is Cron?
-------------

Cron is a daemon which reads cron jobs from a database queue
(PostgreSQL) and executes them (like the classic `cron` daemon).
Multiple instances can run in parallel on different hosts.


Installation & Configuration
----------------------------

#. Create a PostgreSQL database for the queue.
#. :samp:`apt-get install cm4all-cron`
#. Execute :file:`/usr/share/cm4all/workshop/sql/cronjobs.sql` in the
   PostgreSQL database to create the `cronjobs` table.
#. Grant the required permissions to the Cron daemon: :samp:`GRANT
   SELECT, UPDATE ON cronjobs TO "cm4all-cron";`
#. The PostgreSQL user which manages jobs can be configured like this:
   :samp:`GRANT INSERT, SELECT, DELETE ON cronjobs TO cron_client;` and
   :samp:`GRANT UPDATE, SELECT ON cronjobs_id_seq TO cron_client`
#. Edit :file:`/etc/cm4all/cron/cron.conf` and set the variable
   :envvar:`database` (`PostgreSQL documentation
   <https://www.postgresql.org/docs/9.6/static/libpq-connect.html#LIBPQ-CONNSTRING>`_)
#. :samp:`systemctl start cm4all-cron`

Concept
-------

The queue (a PostgreSQL table) contains a list of *jobs*.  Every
daemon instance monitors this queue.

Every cron job contains a schedule in classic `cron` syntax and a
command line to be executed by the shell (:file:`/bin/sh`).


Using Cron
----------

Queueing a job
^^^^^^^^^^^^^^

A cron job consists of a row in the PostgreSQL table.  Example::

  INSERT INTO cronjobs(account_id, schedule, command)
  VALUES('foo', '*/15 * * * *', 'echo Hello World');

During job execution, the column `node_name` is set.

Reference
---------

Cron Schedule
^^^^^^^^^^^^^

The :envvar:`schedule` column follows the classic `cron` schedule
syntax (see :manpage:`crontab(5)`), though the special `@` strings are
not yet implemented.

The `cronjobs` table
^^^^^^^^^^^^^^^^

* :envvar:`id`: The primary key.
* :envvar:`account_id`: The user account which owns this job.  This
  gets passed to the translation server to determine the process
  parameters.
* :envvar:`schedule`: A :manpage:`crontab(5)`-like schedule.
* :envvar:`command`: A command to be executed by :file:`/bin/sh`.
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

Cron is a service which executes programs based on data stored in a
database.  That concept is potentially dangerous when the database has
been compromised.

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
