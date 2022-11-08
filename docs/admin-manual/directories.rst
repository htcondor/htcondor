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
    for details.  If you want to write your logs to a shared filesystem,
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

Directories use by the Submit Role
==================================

 ``SPOOL``
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
===================================

 ``EXECUTE``
    The :macro:`EXECUTE` directory is the parent directory of the
    current working directory for any HTCondor job that runs on a given
    execute-role machine.  HTCondor copies the executable and input files
    for a job to its subdirectory; the job's standard output and standard
    error streams are also logged here.  Jobs will also almost always
    generate their output here as well, so the ``EXECUTE`` directory should
    provide a plenty of space.  ``EXECUTE`` should not be placed under /tmp
    or /var/tmp if possible, as HTCondor loses the ability to make /tmp and
    /var/tmp private to the job.  While not a requirement, ideally ``EXECUTE``
    should be on a distinct filesystem, so that it is impossible for a rogue job
    to fill up non-HTCondor related partitions.

    Usually, the per-job scratch execute directory is created by the startd
    as a directory under ``EXECUTE``.  However, on Linux machines where HTCondor
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

.. warning::
   The per job filesystem feature is a work in progress and not currently supported.

