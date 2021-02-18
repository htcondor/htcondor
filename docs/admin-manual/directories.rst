Directories
-----------

HTCondor uses a few different directories, some of which are role-specific.
Do not use these directories for any other purpose, and do not share these
directories between machines.  The directories are listed in here by the
name of the configuration option used to tell HTCondor where they are; you
will not normally need to change these.

Directories used by More than One Role
======================================

 ``LOG``
    Each HTCondor daemon writes its own log file, and each log file
    is placed in the :macro:`LOG` directory.  You can configure the name
    of each daemon's log by setting :macro:`<SUBSYS>_LOG`,
    although you should never need to do so.  You can also control the sizes
    of the log files or how often they rotate; see
    :ref:`admin-manual/configuration-macros:Daemon Logging Configuration File Entries`
    for details.  If you want to write your logs to a shared filesytem,
    we recommend including ``$(HOSTNAME)`` in the value of ``LOG`` rather
    than changing the names of each individual log to not collide.  If you
    set ``LOG`` to a shared filesystem, you should set ``LOCK`` to a local
    filesystem; see below.

 ``LOCK``
    HTCondor uses a small number of lock files to synchronize access
    to certain files that are shared between multiple daemons.
    Because of problems encountered with file locking and network
    file systems (particularly NFS), these lock files should be
    placed on a local filesystem on each machine.  By default, they
    are placed in the ``LOG`` directory.

Directories use by the Submi Role
=================================

 ``SPOOL``
    The :macro:`SPOOL` directory holds the job queue and history files,
    and the files transferred, if any, when a job which set
    ``when_to_transfer_files`` to ``EXIT_OR_EVICT`` is evicted.  It
    also holds the input and output files of remotely-submitted jobs.
    Disk usage therefore varies widely based on the job mix, but
    since the schedd will abort if it can't append to the job queue log,
    you want to make sure this directory is on a partition which
    won't run out of space.  You may also set
    :macro:`JOB_QUEUE_LOG` to separate the (system) job queue
    log from (user) job data.  This can also be used to increase performance
    (or reliability) by moving the job queue log to specialized hardware (an
    SSD or a a high-redudancy RAID, for example).

Directories use by the Execute Role
===================================

 ``EXECUTE``
    The :macro:`EXECUTE` directory is the parent directory of the
    current working directory for any HTCondor job that runs on a given
    execute-role machine.  HTCondor copies the executable and input files
    for a job to its subdirectory; the job's standard output and standard
    error streams are also logged here.  Jobs will also almost always
    generate their output here as well, so the ``EXECUTE`` directory should
    provide a plenty of space.
