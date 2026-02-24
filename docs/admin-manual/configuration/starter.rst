:index:`Starter Daemon Options<single: Configuration; Starter Daemon Options>`

.. _starter_config_options:

Starter Daemon Configuration Options
====================================

These settings affect the *condor_starter*.

:macro-def:`ASSUME_COMPATIBLE_MULTIFILE_PLUGINS`
    All multi-file file transfer plug-ins read from ClassAd-formatted input
    file, and are expected to both parse any valid ClassAd and ignore any
    attributes it doesn't recognize.  To allow plug-ins which don't meet this
    expectation to continue to work while they're being fixed, you may set
    this macro to false.

:macro-def:`DISABLE_SETUID`
    HTCondor can prevent jobs from running setuid executables
    on Linux by setting the no-new-privileges flag.  This can be
    enabled (i.e. to disallow setuid binaries) by setting :macro:`DISABLE_SETUID`
    to true.

:macro-def:`JOB_RENICE_INCREMENT`
    When the *condor_starter* spawns an HTCondor job, it can do so with
    a nice-level. A nice-level is a Unix mechanism that allows users to
    assign their own processes a lower priority, such that these
    processes do not interfere with interactive use of the machine. For
    machines with lots of real memory and swap space, such that the only
    scarce resource is CPU time, use this macro in conjunction with a
    policy that allows HTCondor to always start jobs on the machines.
    HTCondor jobs would always run, but interactive response on the
    machines would never suffer. A user most likely will not notice
    HTCondor is running jobs. See
    :doc:`/admin-manual/ep-policy-configuration` for more details on setting up a
    policy for starting and stopping jobs on a given machine.

    The ClassAd expression is evaluated in the context of the job ad to
    an integer value, which is set by the *condor_starter* daemon for
    each job just before the job runs. The range of allowable values are
    integers in the range of 0 to 19 (inclusive), with a value of 19
    being the lowest priority. If the integer value is outside this
    range, then on a Unix machine, a value greater than 19 is
    auto-decreased to 19; a value less than 0 is treated as 0. For
    values outside this range, a Windows machine ignores the value and
    uses the default instead. The default value is 0, on Unix, and the
    idle priority class on a Windows machine.

:macro-def:`STARTER_LOCAL_LOGGING`
    This macro determines whether the starter should do local logging to
    its own log file, or send debug information back to the
    *condor_shadow* where it will end up in the ShadowLog. It defaults
    to ``True``.

:macro-def:`STARTER_LOG_NAME_APPEND`
    A fixed value that sets the file name extension of the local log
    file used by the *condor_starter* daemon. Permitted values are
    ``true``, ``false``, ``slot``, ``cluster`` and ``jobid``. A value of
    ``false`` will suppress the use of a file extension. A value of
    ``true`` gives the default behavior of using the slot name, unless
    there is only a single slot. A value of ``slot`` uses the slot name.
    A value of ``cluster`` uses the job's :ad-attr:`ClusterId` ClassAd
    attribute. A value of ``jobid`` uses the job's :ad-attr:`ClusterId` and
    :ad-attr:`ProcId` ClassAd
    attributes. If ``cluster`` or ``jobid`` are specified, the resulting
    log files will persist until deleted by the user, so these two
    options should only be used to assist in debugging, not as permanent
    options.

:macro-def:`STARTER_DEBUG`
    This setting (and other settings related to debug logging in the
    starter) is described above in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`STARTER_NUM_THREADS_ENV_VARS`
    A string containing a list of job environment variables to set equal to
    the number of cores allocated into the slot.  Many commonly used computing
    libraries and programs will look at the value of environment
    variables, such as ``OMP_NUM_THREADS``, to control how many CPU cores to use.  
    Defaults to
    CUBACORES, GOMAXPROCS, JULIA_NUM_THREADS, MKL_NUM_THREADS,
    NUMEXPR_NUM_THREADS, OMP_NUM_THREADS, OMP_THREAD_LIMIT,
    OPENBLAS_NUM_THREADS, PYTHON_CPU_COUNT, ROOT_MAX_THREADS, TF_LOOP_PARALLEL_ITERATIONS,
    TF_NUM_THREADS.

:macro-def:`STARTER_FILE_XFER_STALL_TIMEOUT`
    This value defaults to 3600 (seconds).  It controls the amount of
    time a file transfer can stall before the starter evicts the job.
    A stall can happen when the sandbox is on an NFS server that it down,
    or the network has broken.

:macro-def:`STARTER_UPDATE_INTERVAL`
    An integer value representing the number of seconds between ClassAd
    updates that the *condor_starter* daemon sends to the
    *condor_shadow* and *condor_startd* daemons. Defaults to 300 (5
    minutes).

:macro-def:`STARTER_INITIAL_UPDATE_INTERVAL`
    An integer value representing the number of seconds before the
    first ClassAd update from the *condor_starter* to the *condor_shadow*
    and *condor_startd*.  Defaults to 2 seconds.  On extremely
    large systems which frequently launch all starters at the same time,
    setting this to a random delay may help spread out starter updates
    over time.

:macro-def:`STARTER_UPDATE_INTERVAL_TIMESLICE`
    A floating point value, specifying the highest fraction of time that
    the *condor_starter* daemon should spend collecting monitoring
    information about the job, such as disk usage. The default value is
    0.1. If monitoring, such as checking disk usage takes a long time,
    the *condor_starter* will monitor less frequently than specified by
    :macro:`STARTER_UPDATE_INTERVAL`.

:macro-def:`STARTER_UPDATE_INTERVAL_MAX`
    An integer value representing an upper bound on the number of
    seconds between updates controlled by :macro:`STARTER_UPDATE_INTERVAL` and
    :macro:`STARTER_UPDATE_INTERVAL_TIMESLICE`.  It is recommended to leave this parameter
    at its default value, which is calculated
    as :macro:`STARTER_UPDATE_INTERVAL` * ( 1 / :macro:`STARTER_UPDATE_INTERVAL_TIMESLICE` )

:macro-def:`USER_JOB_WRAPPER`
    The full path and file name of an executable or script. If
    specified, HTCondor never directly executes a job, but instead
    invokes this executable, allowing an administrator to specify the
    executable (wrapper script) that will handle the execution of all
    user jobs. The command-line arguments passed to this program will
    include the full path to the actual user job which should be
    executed, followed by all the command-line parameters to pass to the
    user job. This wrapper script must ultimately replace its image with
    the user job; thus, it must exec() the user job, not fork() it.

    For Bourne type shells (*sh*, *bash*, *ksh*), the last line should
    be:

    .. code-block:: bash

        exec "$@"

    On Windows, the end should look like:

    .. code-block:: bat

        REM set some environment variables
        set LICENSE_SERVER=192.0.2.202:5012
        set MY_PARAMS=2

        REM Run the actual job now
        %*

    This syntax is precise, to correctly handle program arguments which
    contain white space characters.

    For Windows machines, the wrapper will either be a batch script with
    a file extension of ``.bat`` or ``.cmd``, or an executable with a
    file extension of ``.exe`` or ``.com``.

    If the wrapper script encounters an error as it runs, and it is
    unable to run the user job, it is important that the wrapper script
    indicate this to the HTCondor system so that HTCondor does not
    assign the exit code of the wrapper script to the job. To do this,
    the wrapper script should write a useful error message to the file
    named in the environment variable ``_CONDOR_WRAPPER_ERROR_FILE``,
    and then the wrapper script should exit with a non-zero value. If
    this file is created by the wrapper script, HTCondor assumes that
    the wrapper script has failed, and HTCondor will place the job back
    in the queue marking it as Idle, such that the job will again be
    run. The *condor_starter* will also copy the contents of this error
    file to the *condor_starter* log, so the administrator can debug
    the problem.

    When a wrapper script is in use, the executable of a job submission
    may be specified by a relative path, as long as the submit
    description file also contains:

    .. code-block:: condor-submit

                +PreserveRelativeExecutable = True

    For example,

    .. code-block:: condor-submit

                # Let this executable be resolved by user's path in the wrapper
                cmd = sleep
                +PreserveRelativeExecutable = True

    Without this extra attribute:

    .. code-block:: condor-submit

                # A typical fully-qualified executable path
                cmd = /bin/sleep

:macro-def:`CGROUP_MEMORY_LIMIT_POLICY`
    A string with possible values of ``hard``, ``custom`` and ``none``.
    The default value is ``hard``. If set to ``hard``, when the job tries
    to use more memory than the slot size, it will be put on hold with
    an appropriate message.  Also, the cgroup soft limit will set to
    90% of the hard limit to encourage the kernel to lower 
    cacheable memory the job is using.  If set to ``none``, no limit will be enforced, 
    but the memory usage of the job will be accurately measured by a cgroup.
    When set to custom, the additional knob CGROUP_HARD_MEMORY_LIMIT_EXPR
    must be set, which is a classad expression evaluated
    in the context of the machine and the job, respectively, to determine the hard limits.

:macro-def:`CGROUP_HARD_MEMORY_LIMIT_EXPR`
    See above.

:macro-def:`CGROUP_LOW_MEMORY_LIMIT`
    A classad expression, evaluated in the context of the slot and job ad.
    When it evaluated to a number, that number is written to the job's
    cgroup memory.low limit.  This is only implemented on Linux systems
    where HTCondor controls the jobs' cgroups.  When the job exceeds this 
    limit, the kernel will aggressively evict read-only pages (often disk cache)
    from the job's use.  For example, an admin could set this to 
    Memory * 0.5, in order to prevent the system from using otherwise available
    memory for caching on behalf of the job.

:macro-def:`CGROUP_IGNORE_CACHE_MEMORY`
    A boolean value which defaults to true.  When true, cached memory pages
    (like the disk cache) do not count to the job's reported memory usage.

:macro-def:`CGROUP_POLLING_INTERVAL`
    An integer that defaults to 5 (seconds) that controls how frequently a cgroup
    system polls for resource usage.

:macro-def:`DISABLE_SWAP_FOR_JOB`
    A boolean that defaults to true.  When true, and cgroups are in effect, the
    *condor_starter* will set the memws to the same value as the hard memory limit.
    This will prevent the job from using any swap space.  If it needs more memory than
    the hard limit, it will be put on hold.  When false, the job is allowed to use any
    swap space configured by the operating system.

:macro-def:`STARTER_ALWAYS_HOLD_ON_OOM`
    A boolean that defaults to true.  When false, if a job exits With
    an Out Of Memory signal from the kernel, instead of always putting
    the job on hold, HTCondor will check the last memory usage of the
    job, and if less than 90% of the limit, it will assume the Out Of
    Memory was because the system as a whole was out of memory, and the
    job was merely the victim, not the cause of the problem.

:macro-def:`STARTER_HIDE_GPU_DEVICES`
    A Linux-specific boolean that defaults to true.  When true, if started as root,
    HTCondor will use the "devices" cgroup to prevent the job from accessing
    any NVidia GPUs not assigned to it by HTCondor.  The device files will still exist
    in ``/dev``, but any attempt to access them will fail, regardless of their file
    permissions.  The ``nvidia-smi`` command will not report them as being available.
    Setting this macro to false returns to the previous functionality (of allowing jobs
    to access NVidia GPUs not assigned to them).

:macro-def:`USE_VISIBLE_DESKTOP`
    This boolean variable is only meaningful on Windows machines. If
    ``True``, HTCondor will allow the job to create windows on the
    desktop of the execute machine and interact with the job. This is
    particularly useful for debugging why an application will not run
    under HTCondor. If ``False``, HTCondor uses the default behavior of
    creating a new, non-visible desktop to run the job on. See
    the :doc:`/platform-specific/microsoft-windows` section for details
    on how HTCondor interacts with the desktop.

:macro-def:`STARTER_JOB_ENVIRONMENT`
    This macro sets the default environment inherited by jobs. The
    syntax is the same as the syntax for environment settings in the job
    submit file (see :doc:`/man-pages/condor_submit`). If the same
    environment variable is assigned by this macro and by the user in
    the submit file, the user's setting takes precedence.

:macro-def:`JOB_INHERITS_STARTER_ENVIRONMENT`
    A matchlist or boolean value that defaults to ``False``. When set to
    a matchlist it causes jobs to inherit all environment variables from the
    *condor_starter* that are selected by the match list and not already defined
    in the job ClassAd or by the :macro:`STARTER_JOB_ENVIRONMENT` configuration variable.

    A matchlist is a comma, semicolon or space separated list of environment variable names
    and name patterns that match or reject names.
    Matchlist members are matched case-insensitively to each name
    in the environment and those that match are imported. Matchlist members can contain ``*`` as wildcard
    character which matches anything at that position.  Members can have two ``*`` characters if one of them
    is at the end. Members can be prefixed with ``!``
    to force a matching environment variable to not be imported.  The order of members in the Matchlist
    has no effect on the result.  For backward compatibility a single value of ``True`` behaves as if the value
    was set to ``*``.  Prior to HTCondor version 10.1.0 all values other than ``True`` are treated as ``False``.

:macro-def:`STARTER_UPLOAD_TIMEOUT`
    An integer value that specifies the network communication timeout to
    use when transferring files back to the access point. The default
    value is set by the *condor_shadow* daemon to 300. Increase this
    value if the disk on the access point cannot keep up with large
    bursts of activity, such as many jobs all completing at the same
    time.

:macro-def:`ASSIGN_CPU_AFFINITY`
    A boolean expression that defaults to ``False``. When it evaluates
    to ``True``, each job under this *condor_startd* is confined to
    using only as many cores as the configured number of slots. When
    using partitionable slots, each job will be bound to as many cores
    as requested by specifying **request_cpus**. When ``True``, this
    configuration variable overrides any specification of
    :macro:`ENFORCE_CPU_AFFINITY`. The expression is evaluated in the context
    of the Job ClassAd.

:macro-def:`ENFORCE_CPU_AFFINITY`
    This configuration variable is replaced by :macro:`ASSIGN_CPU_AFFINITY`.
    Do not enable this configuration variable unless using glidein or
    another unusual setup.

    A boolean value that defaults to ``False``. When ``False``, the CPU
    affinity of processes in a job is not enforced. When ``True``, the
    processes in an HTCondor job maintain their affinity to a CPU. This
    means that this job will only run on that particular CPU, even if
    other CPU cores are idle.

    If ``True`` and :macro:`SLOT<N>_CPU_AFFINITY` is not set, the CPU that
    the job is locked to is the same as ``SlotID - 1``. Note that slots
    are numbered beginning with the value 1, while CPU cores are
    numbered beginning with the value 0.

    When ``True``, more fine grained affinities may be specified with
    :macro:`SLOT<N>_CPU_AFFINITY`.

:macro-def:`SLOT<N>_CPU_AFFINITY`
    This configuration variable is replaced by :macro:`ASSIGN_CPU_AFFINITY`.
    Do not enable this configuration variable unless using glidein or
    another unusual setup.

    A comma separated list of cores to which an HTCondor job running on
    a specific slot given by the value of ``<N>`` show affinity. Note
    that slots are numbered beginning with the value 1, while CPU cores
    are numbered beginning with the value 0. This affinity list only
    takes effect when ``ENFORCE_CPU_AFFINITY = True``.

:macro-def:`ENABLE_URL_TRANSFERS`
    A boolean value that when ``True`` causes the *condor_starter* for
    a job to invoke all plug-ins defined by :macro:`FILETRANSFER_PLUGINS` to
    determine their capabilities for handling protocols to be used in
    file transfer specified with a URL. When ``False``, a URL transfer
    specified in a job's submit description file will cause an error
    issued by :tool:`condor_submit`. The default value is ``True``.

:macro-def:`FILETRANSFER_PLUGINS`
    A comma separated list of full and absolute path and executable
    names for plug-ins that will accomplish the task of doing file
    transfer when a job requests the transfer of an input file by
    specifying a URL. See
    :ref:`admin-manual/file-and-cred-transfer:Custom File Transfer Plug-Ins`
    for a description of the functionality required of a plug-in.

:macro-def:`FILETRANSFER_PLUGIN_CLASSAD_TIMEOUT`
    An integer number of seconds (defaulting to 20 seconds) after which
    the starter will kill a file transfer plug-in if it takes too long
    to return its ClassAd describing its capabilities.  This may 
    need to be set higher than the default if the plugin is stored 
    on a slow or shared filesystem, or if it does a significant amount 
    of work to generate its ClassAd.

:macro-def:`<PLUGIN>_TEST_URL`
    This configuration takes a URL to be tested against the specified
    ``<PLUGIN>``. If this test fails, then that plugin is removed from
    the *condor_starter* classad attribute :ad-attr:`HasFileTransferPluginMethods`.
    This attribute determines what plugin capabilities the *condor_starter*
    can utilize.

:macro-def:`RUN_FILETRANSFER_PLUGINS_WITH_ROOT`
    A boolean value that affects only Unix platforms and defaults to
    ``False``, causing file transfer plug-ins invoked for a job to run
    with both the real and the effective UID set to user that the job
    runs as. The user that the job runs as may be the job owner, nobody,
    or the slot user. The group is set to primary group of the user that
    the job runs as, and all supplemental groups are dropped. The
    default gives the behavior exhibited prior to the existence of this
    configuration variable. When set to ``True``, file transfer plug-ins
    are invoked with a real UID of 0 (root), provided the HTCondor
    daemons also run as root. The effective UID is set to the user that
    the job runs as.

    This configuration variable can permit plug-ins to do privileged
    operations, such as access a credential protected by file system
    permissions. The default value is recommended unless privileged
    operations are required.

:macro-def:`MAX_FILE_TRANSFER_PLUGIN_LIFETIME`:
    An integer number of seconds (defaulting to twenty hours) after which
    the starter will kill a file transfer plug-in for taking too long.
    Currently, this causes the job to go on hold with ``ETIME`` (62) as
    the hold reason subcode.

:macro-def:`ENABLE_CHIRP`
    A boolean value that defaults to ``True``. An administrator would
    set the value to ``False`` to disable Chirp remote file access from
    execute machines.

:macro-def:`ENABLE_CHIRP_UPDATES`
    A boolean value that defaults to ``True``. If :macro:`ENABLE_CHIRP`
    is ``True``, and :macro:`ENABLE_CHIRP_UPDATES` is ``False``, then the
    user job can only read job attributes from the submit side; it
    cannot change them or write to the job event log. If :macro:`ENABLE_CHIRP`
    is ``False``, the setting of this variable does not matter, as no
    Chirp updates are allowed in that case.

:macro-def:`ENABLE_CHIRP_IO`
    A boolean value that defaults to ``True``. If ``False``, the file
    I/O :tool:`condor_chirp` commands are prohibited.

:macro-def:`ENABLE_CHIRP_DELAYED`
    A boolean value that defaults to ``True``. If ``False``, the
    :tool:`condor_chirp` commands **get_job_attr_delayed** and
    **set_job_attr_delayed** are prohibited.

:macro-def:`CHIRP_DELAYED_UPDATE_PREFIX`
    This is a string-valued and case-insensitive parameter with the
    default value of ``"Chirp*"``. The string is a list separated by
    spaces and/or commas. Each attribute passed to the either of the
    :tool:`condor_chirp` commands **set_job_attr_delayed** or
    **get_job_attr_delayed** must match against at least one element
    in the list. An attribute which does not match any list element
    fails. A list element may contain a wildcard character
    (``"Chirp*"``), which marks where any number of characters matches.
    Thus, the default is to allow reads from and writes to only
    attributes which start with ``"Chirp"``.

    Because this parameter must be set to the same value on both the
    submit and execute nodes, it is advised that this parameter not be
    changed from its built-in default.

:macro-def:`CHIRP_DELAYED_UPDATE_MAX_ATTRS`
    This integer-valued parameter, which defaults to 100, represents the
    maximum number of pending delayed chirp updates buffered by the
    *condor_starter*. If the number of unique attributes updated by the
    :tool:`condor_chirp` command **set_job_attr_delayed** exceeds this
    parameter, it is possible for these updates to be ignored.

:macro-def:`CONDOR_SSH_TO_JOB_FAKE_PASSWD_ENTRY`
    A boolean valued parameter which defaults to true.  When true,
    it sets the environment variable LD_PRELOAD to point to the
    htcondor-provided libgetpwnam.so for the sshd run by 
    :tool:`condor_ssh_to_job`.  This results in the shell being
    set to /bin/sh and the home directory to the scratch directory
    for processes launched by :tool:`condor_ssh_to_job`.

:macro-def:`USE_PSS`
    A boolean value, that when ``True`` causes the *condor_starter* to
    measure the PSS (Proportional Set Size) of each HTCondor job. The
    default value is ``False``. When running many short lived jobs,
    performance problems in the :tool:`condor_procd` have been observed, and
    a setting of ``False`` may relieve these problems.

:macro-def:`MEMORY_USAGE_METRIC`
    A ClassAd expression that produces an initial value for the job
    ClassAd attribute :ad-attr:`MemoryUsage` in jobs that are not vm universe.

:macro-def:`MEMORY_USAGE_METRIC_VM`
    A ClassAd expression that produces an initial value for the job
    ClassAd attribute :ad-attr:`MemoryUsage` in vm universe jobs.

:macro-def:`STARTER_RLIMIT_AS`
    An integer ClassAd expression, expressed in MiB, evaluated by the
    *condor_starter* to set the ``RLIMIT_AS`` parameter of the
    setrlimit() system call. This limits the virtual memory size of each
    process in the user job. The expression is evaluated in the context
    of both the machine and job ClassAds, where the machine ClassAd is
    the ``MY.`` ClassAd, and the job ClassAd is the ``TARGET.`` ClassAd.
    There is no default value for this variable. Since values larger
    than 2047 have no real meaning on 32-bit platforms, values larger
    than 2047 result in no limit set on 32-bit platforms.

:macro-def:`USE_PID_NAMESPACES`
    A boolean value that, when ``True``, enables the use of per job PID
    namespaces for HTCondor jobs run on Linux kernels. Defaults to
    ``False``.

:macro-def:`PER_JOB_NAMESPACES`
    A boolean value that defaults to ``True``. Relevant only for Linux
    platforms using file system namespaces. A value of
    ``False`` ensures that there will be no private mount points,
    because auto mounts done by *autofs* would use the wrong name for
    private file system mounts. A ``True`` value is useful when private
    file system mounts are permitted and *autofs* (for NFS) is not used.

:macro-def:`DYNAMIC_RUN_ACCOUNT_LOCAL_GROUP`
    For Windows platforms, a value that sets the local group to a group
    other than the default ``Users`` for the ``condor-slot<X>`` run
    account. Do not place the local group name within quotation marks.

:macro-def:`JOB_EXECDIR_PERMISSIONS`
    Control the permissions on the job's scratch directory. Defaults to
    ``user`` which sets permissions to 0700. Possible values are
    ``user``, ``group``, and ``world``. If set to ``group``, then the
    directory is group-accessible, with permissions set to 0750. If set
    to ``world``, then the directory is created with permissions set to
    0755.

:macro-def:`CONDOR_SSHD`
    A string value defaulting to /usr/sbin/sshd which is used by
    the example parallel universe scripts to find a working sshd.

:macro-def:`CONDOR_SSH_KEYGEN`
    A string value defaulting to /usr/bin/ssh_keygen which is used by
    the example parallel universe scripts to find a working ssh_keygen
    program.

:macro-def:`STARTER_STATS_LOG`
    The full path and file name of a file that stores TCP statistics for
    starter file transfers. (Note that the starter logs TCP statistics
    to this file by default. Adding ``D_STATS`` to the :macro:`STARTER_DEBUG`
    value will cause TCP statistics to be logged to the normal starter
    log file (``$(STARTER_LOG)``).) If not defined,
    :macro:`STARTER_STATS_LOG` defaults to ``$(LOG)/XferStatsLog``. Setting
    :macro:`STARTER_STATS_LOG` to ``/dev/null`` disables logging of starter
    TCP file transfer statistics.

:macro-def:`MAX_STARTER_STATS_LOG`
    Controls the maximum size in bytes or amount of time that the
    starter TCP statistics log will be allowed to grow. If not defined,
    :macro:`MAX_STARTER_STATS_LOG` defaults to ``$(MAX_DEFAULT_LOG)``, which
    currently defaults to 10 MiB in size. Values are specified with the
    same syntax as :macro:`MAX_DEFAULT_LOG`.

:macro-def:`SINGULARITY`
    The path to the Singularity binary. The default value is
    ``/usr/bin/singularity``.

:macro-def:`SINGULARITY_JOB`
    A boolean value specifying whether this startd should run jobs under
    Singularity.  This can be an expression evaluated in the context of the slot
    ad and the job ad, where the slot ad is the "MY.", and the job ad is the
    "TARGET.". The default value is ``False``.

:macro-def:`SINGULARITY_IMAGE_EXPR`
    The path to the Singularity container image file.  This can be an
    expression evaluated in the context of the slot ad and the job ad, where the
    slot ad is the "MY.", and the job ad is the "TARGET.".  The default value
    is ``"SingularityImage"``.

:macro-def:`SINGULARITY_TARGET_DIR`
    A directory within the Singularity image to which
    ``$_CONDOR_SCRATCH_DIR`` on the host should be mapped. The default
    value is ``""``.

:macro-def:`SINGULARITY_USE_LAUNCHER`
    A boolean which defaults to false.  When true, singularity or apptainer
    images must have a /bin/sh in them, and this is used to launch
    the job proper after dropping a file indicating that the shell wrapper
    has successfully run inside the container.  When HTCondor sees this file
    exists, it knows the container runtime has successfully launched the image.
    If the job exits without this file, HTCondor assumes there is some problem
    with the runtime, and retries the job.

:macro-def:`SINGULARITY_BIND_EXPR`
    A string value containing a list of bind mount specifications to be passed
    to Singularity.  This can be an expression evaluated in the context of the
    slot ad and the job ad, where the slot ad is the "MY.", and the job ad is
    the "TARGET.". The default value is ``"SingularityBind"``.

:macro-def:`SINGULARITY_IGNORE_MISSING_BIND_TARGET`
    A boolean value defaulting to false.  If true, and the singularity
    image is a directory, and the target of a bind mount doesn't exist in
    the target, then skip this bind mount.

:macro-def:`SINGULARITY_USE_PID_NAMESPACES`
    Controls if jobs using Singularity should run in a private PID namespace, with a default value of ``Auto``.
    If set to ``Auto``, then PID namespaces will be used if it is possible to do so, else not used.
    If set to ``True``, then a PID namespaces must be used; if the installed Singularity cannot
    activate PID namespaces (perhaps due to insufficient permissions), then the slot
    attribute :ad-attr:`HasSingularity` will be set to False so that jobs needing Singularity will match.
    If set to ``False``, then PID namespaces must not be used.

:macro-def:`SINGULARITY_EXTRA_ARGUMENTS`
    A string value or classad expression containing a list of extra arguments to be appended
    to the Singularity command line. This can be an expression evaluated in the context of the
    slot ad and the job ad, where the slot ad is the "MY.", and the job ad is the "TARGET.".
:macro-def:`SINGULARITY_RUN_TEST_BEFORE_JOB`
    A boolean value which defaults to true.  When true, before running a singularity
    or apptainer contained job, the HTCondor starter will run apptainer test your_image.
    Only if that succeeds will HTCondor then run your job proper.

:macro-def:`SINGULARITY_VERBOSITY`
    A string value that defaults to -q.  This string is placed immediately after the
    singularity or apptainer command, intended to control debugging verbosity, but
    could be used for any global option for all singularity or apptainer commands.
    Debugging singularity or apptainer problems may be aided by setting this to -v
    or -d.

:macro-def:`SINGULARITY_ADD_ROCM_FLAG`
    A boolean value that defaults to true.  When true, HTCONDOR will pass --rocm 
    flag to singularity, in order to support AMD gpus.  This should not cause problems
    on machines without AMD gpus.

:macro-def:`USE_DEFAULT_CONTAINER`
    A boolean value or classad expression evaluating to boolean in the context of the Slot
    ad (the MY.) and the job ad (the TARGET.).  When true, a vanilla universe job that
    does not request a container will be put into the container image specified by
    the parameter :macro:`DEFAULT_CONTAINER_IMAGE`

:macro-def:`DEFAULT_CONTAINER_IMAGE`
    A string value that when :macro:`USE_DEFAULT_CONTAINER` is true, contains the container
    image to use, either starting with docker:, ending in .sif for a sif file, or otherwise
    an exploded directory for singularity/apptainer to run.

:macro-def:`REACTIVATE_ON_RESTART`
    A boolean value.  If true, self-checkpointing jobs will not restart after
    uploading a checkpoint if the slot would not otherwise be reactivatable.
