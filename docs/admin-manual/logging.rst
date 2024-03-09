Files, Directories and Logs
===========================

:index:`logging`

HTCondor records many types of information in a variety of logs.
Administration may require locating and using the contents of a log to
debug issues. Listed here are details of the logs, to aid in
identification.

Job and Daemon Logs
-------------------

job event log
    The job event log is an optional, chronological list of events that
    occur as a job runs. The job event log is written on the submit
    machine. The submit description file for the job requests a job
    event log with the submit command
    :subcom:`log[definition]`. The log is created
    on and remains on the access point. Contents of the log are detailed
    in the :ref:`users-manual/managing-a-job:in the job event log file` section.
    Examples of events are that the job is running, that the job is placed on
    hold, or that the job completed.

daemon logs
    Each daemon configured to have a log writes events relevant to that
    daemon. Each event written consists of a timestamp and message. The
    name of the log file is set by the value of configuration variable
    :macro:`<SUBSYS>_LOG`, where :macro:`<SUBSYS>` is
    replaced by the name of the daemon. The log is not permitted to grow
    without bound; log rotation takes place after a configurable maximum
    size or length of time is encountered. This maximum is specified by
    configuration variable :macro:`MAX_<SUBSYS>_LOG`.

    Which events are logged for a particular daemon are determined by
    the value of configuration variable :macro:`<SUBSYS>_DEBUG`. The
    possible values for :macro:`<SUBSYS>_DEBUG` categorize events, such
    that it is possible to control the level and quantity of events
    written to the daemon's log.

    Configuration variables that affect daemon logs are

    +------------------------------------+
    |:macro:`MAX_NUM_<SUBSYS>_LOG`       |
    +------------------------------------+
    | :macro:`<SUBSYS>_LOG_KEEP_OPEN`    |
    +------------------------------------+
    |:macro:`TRUNC_<SUBSYS>_LOG_ON_OPEN` |
    +------------------------------------+
    | :macro:`TOUCH_LOG_INTERVAL`        |
    +------------------------------------+
    | :macro:`<SUBSYS>_LOCK`             |
    +------------------------------------+
    | :macro:`FILE_LOCK_VIA_MUTEX`       |
    +------------------------------------+
    | :macro:`LOGS_USE_TIMESTAMP`        |
    +------------------------------------+
    | :macro:`LOG_TO_SYSLOG`             |
    +------------------------------------+

    Daemon logs are often investigated to accomplish administrative
    debugging. :tool:`condor_config_val` can be used to determine the
    location and file name of the daemon log. For example, to display
    the location of the log for the *condor_collector* daemon, use

    .. code-block:: console

          $ condor_config_val COLLECTOR_LOG

job queue log
    The job queue log is a transactional representation of the current
    job queue. If the *condor_schedd* crashes, the job queue can be
    rebuilt using this log. The file name is set by configuration
    variable :macro:`JOB_QUEUE_LOG`, and defaults to ``$(SPOOL)/job_queue.log``.

    Within the log, each transaction is identified with an integer value
    and followed where appropriate with other values relevant to the
    transaction. To reduce the size of the log and remove any
    transactions that are no longer relevant, a copy of the log is kept
    by renaming the log at each time interval defined by configuration
    variable :macro:`QUEUE_CLEAN_INTERVAL`, and then a new log is written
    with only current and relevant transactions.

    Configuration variables that affect the job queue log are

    +------------------------------+--------------------------------------+
    | :macro:`SCHEDD_BACKUP_SPOOL` | :macro:`MAX_JOB_QUEUE_LOG_ROTATIONS` |
    +------------------------------+--------------------------------------+
    | :macro:`QUEUE_CLEAN_INTERVAL`|                                      |
    +------------------------------+--------------------------------------+

*condor_schedd* audit log
    The optional *condor_schedd* audit log records user-initiated
    events that modify the job queue, such as invocations of
    :tool:`condor_submit`, :tool:`condor_rm`, :tool:`condor_hold` and
    :tool:`condor_release`. Each event has a time stamp and a message that
    describes details of the event.

    This log exists to help administrators track the activities of pool
    users.

    The file name is set by configuration variable :macro:`SCHEDD_AUDIT_LOG`.

    Configuration variables that affect the audit log are

    +-------------------------------+----------------------------------+
    | :macro:`MAX_SCHEDD_AUDIT_LOG` | :macro:`MAX_NUM_SCHEDD_AUDIT_LOG`|
    +-------------------------------+----------------------------------+

*condor_shared_port* audit log
    The optional *condor_shared_port* audit log records connections
    made through the :macro:`DAEMON_SOCKET_DIR`. Each record includes the source
    address, the socket file name, and the target process's PID, UID,
    GID, executable path, and command line.

    This log exists to help administrators track the activities of pool
    users.

    The file name is set by configuration variable :macro:`SHARED_PORT_AUDIT_LOG`.

    Configuration variables that affect the audit log are

    +------------------------------------+----------------------------------------+
    | :macro:`MAX_SHARED_PORT_AUDIT_LOG` | :macro:`MAX_NUM_SHARED_PORT_AUDIT_LOG` |
    +------------------------------------+----------------------------------------+

event log
    The event log is an optional, chronological list of events that
    occur for all jobs and all users. The events logged are the same as
    those that would go into a job event log. The file name is set by
    configuration variable :macro:`EVENT_LOG`. The
    log is created only if this configuration variable is set.

    Configuration variables that affect the event log, setting details
    such as the maximum size to which this log may grow and details of
    file rotation and locking are

    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_MAX_SIZE`        | :macro:`EVENT_LOG_MAX_ROTATIONS`           |
    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_LOCKING`         |  :macro:`EVENT_LOG_ROTATION_LOCK`          |
    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_FSYNC`           | :macro:`EVENT_LOG_JOB_AD_INFORMATION_ATTRS`|
    +------------------------------------+--------------------------------------------+
    | :macro:`EVENT_LOG_USE_XML`         |                                            |
    +------------------------------------+--------------------------------------------+

accountant log
    The accountant log is a transactional representation of the
    *condor_negotiator* daemon's database of accounting information,
    which are user priorities. The file name of the accountant log is
    ``$(SPOOL)/Accountantnew.log``. Within the log, users are identified
    by username@uid_domain.

    To reduce the size and remove information that is no longer
    relevant, a copy of the log is made when its size hits the number of
    bytes defined by configuration variable
    :macro:`MAX_ACCOUNTANT_DATABASE_SIZE`, and then a new log is written in a
    more compact form.

    Administrators can change user priorities kept in this log by using
    the command line tool :tool:`condor_userprio`.

negotiator match log
    The negotiator match log is a second daemon log from the
    *condor_negotiator* daemon. Events written to this log are those
    with debug level of ``D_MATCH``. The file name is set by
    configuration variable :macro:`NEGOTIATOR_MATCH_LOG`, and defaults to
    ``$(LOG)/MatchLog``.

history log
    This optional log contains information about all jobs that have been
    completed. It is written by the *condor_schedd* daemon. The file
    name is ``$(SPOOL)/history``.

    Administrators can change view this historical information by using
    the command line tool :tool:`condor_history`.

    Configuration variables that affect the history log, setting details
    such as the maximum size to which this log may grow are

    +----------------------------------+--------------------------------+
    | :macro:`ENABLE_HISTORY_ROTATION` |                                |
    +----------------------------------+--------------------------------+
    | :macro:`MAX_HISTORY_LOG`         | :macro:`MAX_HISTORY_ROTATIONS` |
    +----------------------------------+--------------------------------+
    | :macro:`ROTATE_HISTORY_MONTHLY`  | :macro:`ROTATE_HISTORY_DAILY`  |
    +----------------------------------+--------------------------------+

DAGMan Logs
-----------

default node log
    A job event log of all node jobs within a single DAG. It is used to
    enforce the dependencies of the DAG.

    The file name is set by configuration variable
    :macro:`DAGMAN_DEFAULT_NODE_LOG`,
    and the full path name of this file must be unique while any and all
    submitted DAGs and other jobs from the submit host run. The syntax
    used in the definition of this configuration variable is different
    to enable the setting of a unique file name. See
    the :ref:`DAGMan Configuration` section for the complete definition.

the ``.dagman.out`` file
    A log created or appended to for each DAG submitted with timestamped
    events and extra information about the configuration applied to the
    DAG. The name of this log is formed by appending ``.dagman.out`` to
    the name of the DAG input file. The file remains after the DAG
    completes.

    This log may be helpful in debugging what has happened in the
    execution of a DAG, as well as help to determine the final state of
    the DAG.

    Configuration variables that affect this log are

    +---------------------------+-----------------------------------------+
    | :macro:`DAGMAN_VERBOSITY` | :macro:`DAGMAN_PENDING_REPORT_INTERVAL` |
    +---------------------------+-----------------------------------------+

the DAGMan job state log
    This optional, machine-readable log enables automated monitoring of
    DAG. The page :ref:`DAGMan Machine Readable History` details this log.


Directories
-----------

HTCondor uses a few different directories, some of which are role-specific.
Do not use these directories for any other purpose, and do not share these
directories between machines.  The directories are listed in here by the
name of the configuration option used to tell HTCondor where they are; you
will not normally need to change these.

Directories used by More than One Role
``````````````````````````````````````

 :macro:`LOG`
    Each HTCondor daemon writes its own log file, and each log file
    is placed in the :macro:`LOG` directory.  You can configure the name
    of each daemon's log by setting :macro:`<SUBSYS>_LOG`,
    although you should never need to do so.  You can also control the sizes
    of the log files or how often they rotate; see
    :ref:`admin-manual/configuration-macros:Daemon Logging Configuration File Entries`
    for details.  If you want to write your logs to a shared filesystem,
    we recommend including ``$(HOSTNAME)`` in the value of :macro:`LOG` rather
    than changing the names of each individual log to not collide.  If you
    set :macro:`LOG` to a shared filesystem, you should set :macro:`LOCK` to a local
    filesystem; see below.

 :macro:`LOCK`
    HTCondor uses a small number of lock files to synchronize access
    to certain files that are shared between multiple daemons.
    Because of problems encountered with file locking and network
    file systems (particularly NFS), these lock files should be
    placed on a local filesystem on each machine.  By default, they
    are placed in the :macro:`LOG` directory.

Directories use by the Submit Role
``````````````````````````````````

 :macro:`SPOOL`
    The :macro:`SPOOL` directory holds two types of files: system
    data and (user) job data.  The former includes the job queue and
    history files.  The latter includes:

    - the files transferred, if any, when a job which set
      ``when_to_transfer_files`` to ``EXIT_OR_EVICT`` is evicted.
    - the input and output files of remotely-submitted jobs.
    - the checkpoint files stored by self-checkpointing jobs.

    Disk usage therefore varies widely based on the job mix, but
    since the schedd will abort if it can't append to the job queue log,
    you want to make sure this directory is on a partition which
    won't run out of space.

    To help ensure this, you may set
    :macro:`JOB_QUEUE_LOG` to separate the job queue log (system data)
    from the (user) job data.  This can also be used to increase performance
    (or reliability) by moving the job queue log to specialized hardware (an
    SSD or a a high-redundancy RAID, for example).

Directories use by the Execute Role
```````````````````````````````````

 :macro:`EXECUTE`
    The :macro:`EXECUTE` directory is the parent directory of the
    current working directory for any HTCondor job that runs on a given
    execute-role machine.  HTCondor copies the executable and input files
    for a job to its subdirectory; the job's standard output and standard
    error streams are also logged here.  Jobs will also almost always
    generate their output here as well, so the :macro:`EXECUTE` directory should
    provide a plenty of space.  :macro:`EXECUTE` should not be placed under /tmp
    or /var/tmp if possible, as HTCondor loses the ability to make /tmp and
    /var/tmp private to the job.  While not a requirement, ideally :macro:`EXECUTE`
    should be on a distinct filesystem, so that it is impossible for a rogue job
    to fill up non-HTCondor related partitions.

    Usually, the per-job scratch execute directory is created by the startd
    as a directory under :macro:`EXECUTE`.  However, on Linux machines where HTCondor
    has root privilege, it can be configured to make an ephemeral, per-job scratch filesystem
    backed either by LVM, if it is configured, or a large existing file on the filesystem.

    There are several advantages to this approach.  The first is that disk space is
    more accurately measured and enforced.  HTCondor can get the disk usage by a single
    system call, instead of traversing what might be a very deep directory hierarchy. There
    may also be performance benefits, as this filesystem never needs to survive a reboot,
    and is thus mounted with mount options that provide the least amount of disk consistence
    in the face of a reboot.  Also, when the job exits, all the files in the filesystem
    can be removed by simply unmounting and destroying the filesystem, which is much
    faster than having condor remove each scratch file in turn.

    To enable this, first set :macro:`STARTD_ENFORCE_DISK_LIMITS` to ``true``.  Then, if LVM is 
    installed and configured, set :macro:`THINPOOL_NAME` to the name of a logical volume.
    ``"condor_lv"`` might be a good choice.  Finally, set :macro:`THINPOOL_VOLUME_GROUP` to 
    the name of the volume group the LVM administrator has created for this purpose.
    ``"condor_vg"`` might be a good name.  If there is no LVM on the system, a single large
    existing file can be used as the backing store, in which case the knob :macro:`THINPOOL_BACKING_FILE`
    should be set to the name of the existing large file on disk that HTCondor
    will use to make filesystems from.

