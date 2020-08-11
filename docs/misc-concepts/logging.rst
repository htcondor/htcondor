Logging in HTCondor
===================

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
    **log** :index:`log<single: log; submit commands>`. The log is created
    and remains on the submit machine. Contents of the log are detailed
    in the :ref:`users-manual/managing-a-job:in the job event log file` section.
    Examples of events are that the job is running, that the job is placed on
    hold, or that the job completed.

daemon logs
    Each daemon configured to have a log writes events relevant to that
    daemon. Each event written consists of a timestamp and message. The
    name of the log file is set by the value of configuration variable
    ``<SUBSYS>_LOG`` :index:`<SUBSYS>_LOG`, where ``<SUBSYS>`` is
    replaced by the name of the daemon. The log is not permitted to grow
    without bound; log rotation takes place after a configurable maximum
    size or length of time is encountered. This maximum is specified by
    configuration variable ``MAX_<SUBSYS>_LOG``
    :index:`MAX_<SUBSYS>_LOG`.

    Which events are logged for a particular daemon are determined by
    the value of configuration variable ``<SUBSYS>_DEBUG``
    :index:`<SUBSYS>_DEBUG`. The possible values for
    ``<SUBSYS>_DEBUG`` categorize events, such that it is possible to
    control the level and quantity of events written to the daemon's
    log.

    Configuration variables that affect daemon logs are

     ``MAX_NUM_<SUBSYS>_LOG`` :index:`MAX_NUM_<SUBSYS>_LOG`
     ``TRUNC_<SUBSYS>_LOG_ON_OPEN``:index:`TRUNC_<SUBSYS>_LOG_ON_OPEN`
     ``<SUBSYS>_LOG_KEEP_OPEN`` :index:`<SUBSYS>_LOG_KEEP_OPEN`
     ``<SUBSYS>_LOCK`` :index:`<SUBSYS>_LOCK`
     ``FILE_LOCK_VIA_MUTEX`` :index:`FILE_LOCK_VIA_MUTEX`
     ``TOUCH_LOG_INTERVAL`` :index:`TOUCH_LOG_INTERVAL`
     ``LOGS_USE_TIMESTAMP`` :index:`LOGS_USE_TIMESTAMP`
     ``LOG_TO_SYSLOG`` :index:`LOG_TO_SYSLOG`

    Daemon logs are often investigated to accomplish administrative
    debugging. *condor_config_val* can be used to determine the
    location and file name of the daemon log. For example, to display
    the location of the log for the *condor_collector* daemon, use

    .. code-block:: console

          $ condor_config_val COLLECTOR_LOG

job queue log
    The job queue log is a transactional representation of the current
    job queue. If the *condor_schedd* crashes, the job queue can be
    rebuilt using this log. The file name is set by configuration
    variable ``JOB_QUEUE_LOG`` :index:`JOB_QUEUE_LOG`, and
    defaults to ``$(SPOOL)/job_queue.log``.

    Within the log, each transaction is identified with an integer value
    and followed where appropriate with other values relevant to the
    transaction. To reduce the size of the log and remove any
    transactions that are no longer relevant, a copy of the log is kept
    by renaming the log at each time interval defined by configuration
    variable ``QUEUE_CLEAN_INTERVAL``, and then a new log is written
    with only current and relevant transactions.

    Configuration variables that affect the job queue log are

     ``SCHEDD_BACKUP_SPOOL`` :index:`SCHEDD_BACKUP_SPOOL`
     ``QUEUE_CLEAN_INTERVAL`` :index:`QUEUE_CLEAN_INTERVAL`
     ``MAX_JOB_QUEUE_LOG_ROTATIONS`` :index:`MAX_JOB_QUEUE_LOG_ROTATIONS`

*condor_schedd* audit log
    The optional *condor_schedd* audit log records user-initiated
    events that modify the job queue, such as invocations of
    *condor_submit*, *condor_rm*, *condor_hold* and
    *condor_release*. Each event has a time stamp and a message that
    describes details of the event.

    This log exists to help administrators track the activities of pool
    users.

    The file name is set by configuration variable ``SCHEDD_AUDIT_LOG``
    :index:`SCHEDD_AUDIT_LOG`.

    Configuration variables that affect the audit log are

     ``MAX_SCHEDD_AUDIT_LOG`` :index:`MAX_SCHEDD_AUDIT_LOG`
     ``MAX_NUM_SCHEDD_AUDIT_LOG`` :index:`MAX_NUM_SCHEDD_AUDIT_LOG`

*condor_shared_port* audit log
    The optional *condor_shared_port* audit log records connections
    made through the ``DAEMON_SOCKET_DIR``
    :index:`DAEMON_SOCKET_DIR`. Each record includes the source
    address, the socket file name, and the target process's PID, UID,
    GID, executable path, and command line.

    This log exists to help administrators track the activities of pool
    users.

    The file name is set by configuration variable
    ``SHARED_PORT_AUDIT_LOG`` :index:`SHARED_PORT_AUDIT_LOG`.

    Configuration variables that affect the audit log are

     ``MAX_SHARED_PORT_AUDIT_LOG``:index:`MAX_SHARED_PORT_AUDIT_LOG`
     ``MAX_NUM_SHARED_PORT_AUDIT_LOG`` :index:`MAX_NUM_SHARED_PORT_AUDIT_LOG`

event log
    The event log is an optional, chronological list of events that
    occur for all jobs and all users. The events logged are the same as
    those that would go into a job event log. The file name is set by
    configuration variable ``EVENT_LOG`` :index:`EVENT_LOG`. The
    log is created only if this configuration variable is set.

    Configuration variables that affect the event log, setting details
    such as the maximum size to which this log may grow and details of
    file rotation and locking are

     ``EVENT_LOG_MAX_SIZE`` :index:`EVENT_LOG_MAX_SIZE`
     ``EVENT_LOG_MAX_ROTATIONS`` :index:`EVENT_LOG_MAX_ROTATIONS`
     ``EVENT_LOG_LOCKING`` :index:`EVENT_LOG_LOCKING`
     ``EVENT_LOG_FSYNC`` :index:`EVENT_LOG_FSYNC`
     ``EVENT_LOG_ROTATION_LOCK`` :index:`EVENT_LOG_ROTATION_LOCK`
     ``EVENT_LOG_JOB_AD_INFORMATION_ATTRS`` :index:`EVENT_LOG_JOB_AD_INFORMATION_ATTRS`
     ``EVENT_LOG_USE_XML`` :index:`EVENT_LOG_USE_XML`

accountant log
    The accountant log is a transactional representation of the
    *condor_negotiator* daemon's database of accounting information,
    which are user priorities. The file name of the accountant log is
    ``$(SPOOL)/Accountantnew.log``. Within the log, users are identified
    by username@uid_domain.

    To reduce the size and remove information that is no longer
    relevant, a copy of the log is made when its size hits the number of
    bytes defined by configuration variable
    ``MAX_ACCOUNTANT_DATABASE_SIZE``, and then a new log is written in a
    more compact form.

    Administrators can change user priorities kept in this log by using
    the command line tool *condor_userprio*.

negotiator match log
    The negotiator match log is a second daemon log from the
    *condor_negotiator* daemon. Events written to this log are those
    with debug level of ``D_MATCH``. The file name is set by
    configuration variable ``NEGOTIATOR_MATCH_LOG``
    :index:`NEGOTIATOR_MATCH_LOG`, and defaults to
    ``$(LOG)/MatchLog``.

history log
    This optional log contains information about all jobs that have been
    completed. It is written by the *condor_schedd* daemon. The file
    name is ``$(SPOOL)/history``.

    Administrators can change view this historical information by using
    the command line tool *condor_history*.

    Configuration variables that affect the history log, setting details
    such as the maximum size to which this log may grow are

     ``ENABLE_HISTORY_ROTATION`` :index:`ENABLE_HISTORY_ROTATION`
     ``MAX_HISTORY_LOG`` :index:`MAX_HISTORY_LOG`
     ``MAX_HISTORY_ROTATIONS`` :index:`MAX_HISTORY_ROTATIONS`
     ``ROTATE_HISTORY_DAILY`` :index:`ROTATE_HISTORY_DAILY`
     ``ROTATE_HISTORY_MONTHLY`` :index:`ROTATE_HISTORY_MONTHLY`

DAGMan Logs
-----------

default node log
    A job event log of all node jobs within a single DAG. It is used to
    enforce the dependencies of the DAG.

    The file name is set by configuration variable
    ``DAGMAN_DEFAULT_NODE_LOG`` :index:`DAGMAN_DEFAULT_NODE_LOG`,
    and the full path name of this file must be unique while any and all
    submitted DAGs and other jobs from the submit host run. The syntax
    used in the definition of this configuration variable is different
    to enable the setting of a unique file name. See
    the :ref:`admin-manual/configuration-macros:configuration file entries for
    dagman` section for the complete definition.

    Configuration variables that affect this log are

     ``DAGMAN_ALWAYS_USE_NODE_LOG`` :index:`DAGMAN_ALWAYS_USE_NODE_LOG`

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

     ``DAGMAN_VERBOSITY`` :index:`DAGMAN_VERBOSITY`
     ``DAGMAN_PENDING_REPORT_INTERVAL`` :index:`DAGMAN_PENDING_REPORT_INTERVAL`

the ``jobstate.log`` file
    This optional, machine-readable log enables automated monitoring of
    DAG. The page :ref:`users-manual/dagman-workflows:a machine-readable
    event history, the jobstate.log file` details this log.

:index:`logging`


