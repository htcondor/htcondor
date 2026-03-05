:index:`Procd Daemon Options<single: Configuration; Procd Daemon Options>`

.. _procd_config_options:

Procd Daemon Configuration Options
==================================

:macro-def:`USE_PROCD`
    This boolean variable determines whether the :tool:`condor_procd` will be
    used for managing process families. If the :tool:`condor_procd` is not
    used, each daemon will run the process family tracking logic on its
    own. Use of the :tool:`condor_procd` results in improved scalability
    because only one instance of this logic is required. The
    :tool:`condor_procd` is required when using group ID-based process
    tracking.
    In this case, the :macro:`USE_PROCD` setting will be ignored and a
    :tool:`condor_procd` will always be used. By default, the
    :tool:`condor_master` will start a :tool:`condor_procd` that all other daemons
    that need process family tracking will use. A daemon that uses the
    :tool:`condor_procd` will start a :tool:`condor_procd` for use by itself and
    all of its child daemons.

:macro-def:`PROCD_MAX_SNAPSHOT_INTERVAL`
    This setting determines the maximum time that the :tool:`condor_procd`
    will wait between probes of the system for information about the
    process families it is tracking.

:macro-def:`PROCD_LOG`
    Specifies a log file for the :tool:`condor_procd` to use. Note that by
    design, the :tool:`condor_procd` does not include most of the other logic
    that is shared amongst the various HTCondor daemons. This means that
    the :tool:`condor_procd` does not include the normal HTCondor logging
    subsystem, and thus multiple debug levels are not supported.
    :macro:`PROCD_LOG` defaults to ``$(LOG)/ProcLog``. Note that enabling
    ``D_PROCFAMILY`` in the debug level for any other daemon will cause
    it to log all interactions with the :tool:`condor_procd`.

:macro-def:`MAX_PROCD_LOG`
    Controls the maximum length in bytes to which the :tool:`condor_procd`
    log will be allowed to grow. The log file will grow to the specified
    length, then be saved to a file with the suffix ``.old``. The
    ``.old`` file is overwritten each time the log is saved, thus the
    maximum space devoted to logging will be twice the maximum length of
    this log file. A value of 0 specifies that the file may grow without
    bounds. The default is 10 MiB.

:macro-def:`PROCD_ADDRESS`
    This specifies the address that the :tool:`condor_procd` will use to
    receive requests from other HTCondor daemons. On Unix, this should
    point to a file system location that can be used for a named pipe.
    On Windows, named pipes are also used but they do not exist in the
    file system. The default setting is $(RUN)/procd_pipe on Unix
    and \\\\.\\pipe\\procd_pipe on Windows.

:macro-def:`USE_GID_PROCESS_TRACKING`
    A boolean value that defaults to ``False``. When ``True``, a job's
    initial process is assigned a dedicated GID which is further used by
    the :tool:`condor_procd` to reliably track all processes associated with
    a job. When ``True``, values for :macro:`MIN_TRACKING_GID` and
    :macro:`MAX_TRACKING_GID` must also be set, or HTCondor will abort,
    logging an error message.

:macro-def:`MIN_TRACKING_GID`
    An integer value, that together with :macro:`MAX_TRACKING_GID` specify a
    range of GIDs to be assigned on a per slot basis for use by the
    :tool:`condor_procd` in tracking processes associated with a job.

:macro-def:`MAX_TRACKING_GID`
    An integer value, that together with :macro:`MIN_TRACKING_GID` specify a
    range of GIDs to be assigned on a per slot basis for use by the
    :tool:`condor_procd` in tracking processes associated with a job.

:macro-def:`BASE_CGROUP`
    The path to the directory used as the virtual file system for the
    implementation of Linux kernel cgroups. This variable defaults to
    the string ``htcondor``, and is only used on Linux systems. To
    disable cgroup tracking, define this to an empty string. See
    :ref:`admin-manual/ep-policy-configuration:cgroup based process
    tracking` for a description of cgroup-based process tracking.
    An administrator can configure distinct cgroup roots for 
    different slot types within the same startd by prefixing
    the *BASE_CGROUP* macro with the slot type. e.g. setting
    SLOT_TYPE_1.BASE_CGROUP = hiprio_cgroup and SLOT_TYPE_2.BASE_CGROUP = low_prio

:macro-def:`CREATE_CGROUP_WITHOUT_ROOT`
    Defaults to false.  When true, on a Linux cgroup v2 system, a
    condor system without root privilege (such as a glidein)
    will attempt to create cgroups for jobs.  The condor_master
    must have been started under a writable cgroup for this to work.
