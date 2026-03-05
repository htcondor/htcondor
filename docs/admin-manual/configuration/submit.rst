:index:`Job Submission Options<single: Configuration; Job Submission Options>`

.. _submit_config_options:

Job Submission Configuration Options
====================================

:macro-def:`DEFAULT_UNIVERSE`
    The universe under which a job is executed may be specified in the
    submit description file. If it is not specified in the submit
    description file, then this variable specifies the universe (when
    defined). If the universe is not specified in the submit description
    file, and if this variable is not defined, then the default universe
    for a job will be the vanilla universe.

:macro-def:`JOB_DEFAULT_NOTIFICATION`
    The default that sets email notification for jobs. This variable
    defaults to ``NEVER``, such that HTCondor will not send email about
    events for jobs. Possible values are ``NEVER``, ``ERROR``,
    ``ALWAYS``, or ``COMPLETE``. If ``ALWAYS``, the owner will be
    notified whenever the
    job completes. If ``COMPLETE``, the owner will be notified when the
    job terminates. If ``ERROR``, the owner will only be notified if the
    job terminates abnormally, or if the job is placed on hold because
    of a failure, and not by user request. If ``NEVER``, the owner will
    not receive email.

:macro-def:`JOB_DEFAULT_LEASE_DURATION`
    The default value for the
    :subcom:`job_lease_duration[and JOB_DEFAULT_LEASE_DURATION]`
    submit command when the submit file does not specify a value. The
    default value is 2400, which is 40 minutes.

:macro-def:`JOB_DEFAULT_REQUESTMEMORY`
    The amount of memory in MiB to acquire for a job, if the job does
    not specify how much it needs using the
    :subcom:`request_memory[and JOB_DEFAULT_REQUESTMEMORY]`
    submit command. If this variable is not defined, then the default is
    defined by the expression :ad-expr:`128`


:macro-def:`JOB_DEFAULT_REQUESTDISK`
    The amount of disk in KiB to acquire for a job, if the job does not
    specify how much it needs using the
    :subcom:`request_disk[and JOB_DEFAULT_REQUESTDISK]`
    submit command. If the job defines the value, then that value takes
    precedence. If not set, then the default is the maximum of 1 GB
    and 125% of the transfer input size, which is the expression
    :ad-expr:`MAX({1024, (TransferInputSizeMB+1) * 1.25}) * 1024`

:macro-def:`JOB_DEFAULT_REQUESTCPUS`
    The number of CPUs to acquire for a job, if the job does not specify
    how many it needs using the :subcom:`request_cpus[and JOB_DEFAULT_REQUESTCPUS]`
    submit command. If the job defines the value, then that value takes
    precedence. If not set, then then the default is 1.

:macro-def:`DEFAULT_JOB_MAX_RETRIES`
    The default value for the maximum number of job retries, if the
    :tool:`condor_submit` retry feature is used. (Note that this value is
    only relevant if either :subcom:`retry_until[and DEFAULT_JOB_MAX_RETRIES]`
    or :subcom:`success_exit_code[and DEFAULT_JOB_MAX_RETRIES]`
    is defined in the submit file, and :subcom:`max_retries[and DEFAULT_JOB_MAX_RETRIES]`
    is not.) (See the :doc:`/man-pages/condor_submit` man page.) The default value
    if not defined is 2.

If you want :tool:`condor_submit` to automatically append an expression to
the ``Requirements`` expression or ``Rank`` expression of jobs at your
site use the following macros:

:macro-def:`APPEND_REQ_VANILLA`
    Expression to be appended to vanilla job requirements.

:macro-def:`APPEND_REQUIREMENTS`
    Expression to be appended to any type of universe jobs. However, if
    :macro:`APPEND_REQ_VANILLA` is defined, then
    ignore the :macro:`APPEND_REQUIREMENTS` for that universe.

:macro-def:`APPEND_RANK`
    Expression to be appended to job rank.
    :macro:`APPEND_RANK_VANILLA` will override this setting if defined.

:macro-def:`APPEND_RANK_VANILLA`
    Expression to append to vanilla job rank.

In addition, you may provide default ``Rank`` expressions if your users
do not specify their own with:

:macro-def:`DEFAULT_RANK`
    Default rank expression for any job that does not specify its own
    rank expression in the submit description file. There is no default
    value, such that when undefined, the value used will be 0.0.

:macro-def:`DEFAULT_RANK_VANILLA`
    Default rank for vanilla universe jobs. There is no default value,
    such that when undefined, the value used will be 0.0. When both
    :macro:`DEFAULT_RANK` and :macro:`DEFAULT_RANK_VANILLA` are defined, the value
    for :macro:`DEFAULT_RANK_VANILLA` is used for vanilla universe jobs.

:macro-def:`SUBMIT_GENERATE_CUSTOM_RESOURCE_REQUIREMENTS`
    If ``True``, :tool:`condor_submit` will treat any attribute in the job
    ClassAd that begins with ``Request`` as a request for a custom resource
    and will ad a clause to the Requirements expression ensuring that
    on slots that have that resource will match the job.
    The default value is ``True``.

:macro-def:`SUBMIT_GENERATE_CONDOR_C_REQUIREMENTS`
    If ``True``, :tool:`condor_submit` will add clauses to the job's
    Requirements expression for **condor** grid universe jobs like it
    does for vanilla universe jobs.
    The default value is ``True``.

:macro-def:`SUBMIT_SKIP_FILECHECKS`
    If ``True``, :tool:`condor_submit` behaves as if the **-disable**
    command-line option is used. This tells :tool:`condor_submit` to disable
    file permission checks when submitting a job for read permissions on
    all input files, such as those defined by commands
    :subcom:`input[and SUBMIT_SKIP_FILECHECKS]`
    :subcom:`transfer_input_files[and SUBMIT_SKIP_FILECHECKS]`
    as well as write permission to output files, such as a log file
    defined by :subcom:`log[and SUBMIT_SKIP_FILECHECKS]`
    files defined with :subcom:`output[and SUBMIT_SKIP_FILECHECKS]`
    :subcom:`transfer_output_files[and SUBMIT_SKIP_FILECHECKS]`
    This can significantly decrease the amount of time required to
    submit a large group of jobs.  The default value is ``True``.

:macro-def:`WARN_ON_UNUSED_SUBMIT_FILE_MACROS`
    A boolean variable that defaults to ``True``. When ``True``,
    :tool:`condor_submit` performs checks on the job's submit description
    file contents for commands that define a macro, but do not use the
    macro within the file. A warning is issued, but job submission
    continues. A definition of a new macro occurs when the lhs of a
    command is not a known submit command. This check may help spot
    spelling errors of known submit commands.

:macro-def:`SUBMIT_DEFAULT_SHOULD_TRANSFER_FILES`
    Provides a default value for the submit command
    :subcom:`should_transfer_files[and SUBMIT_DEFAULT_SHOULD_TRANSFER_FILES]`
    if the submit file does not supply a value and when the value is not
    forced by some other command in the submit file, such as the
    universe. Valid values are YES, TRUE, ALWAYS, NO, FALSE, NEVER and
    IF_NEEDED. If the value is not one of these, then IF_NEEDED will
    be used.

:macro-def:`SUBMIT_REQUEST_MISSING_UNITS`
    If set to the string ``error``, it is an error to submit a job with a 
    :subcom:`request_memory` or :subcom:`request_disk` with a unitless
    value.  If set to ``warn``, a warning is printed to the screen, but
    submit continues. Default value is unset (neither warn or error).
    :jira:`1837`
    
:macro-def:`SUBMIT_SEND_RESCHEDULE`
    A boolean expression that when False, prevents :tool:`condor_submit` from
    automatically sending a :tool:`condor_reschedule` command as it
    completes. The :tool:`condor_reschedule` command causes the
    *condor_schedd* daemon to start searching for machines with which
    to match the submitted jobs. When True, this step always occurs. In
    the case that the machine where the job(s) are submitted is managing
    a huge number of jobs (thousands or tens of thousands), this step
    would hurt performance in such a way that it became an obstacle to
    scalability. The default value is True.

:macro-def:`SUBMIT_CONTAINER_NEVER_XFER_ABSOLUTE_CMD`
    A boolean that defaults to false.  When true, which was the default
    before 24.0, a container or docker universe job whose Executable
    was an absolute path was assumed to be located within the container
    image, and thus never transfered.  When false, we assume it on the AP
    and thus transfered, when file transfer is enabled.

:macro-def:`SUBMIT_ATTRS`
    A comma-separated and/or space-separated list of ClassAd attribute
    names for which the attribute and value will be inserted into all
    the job ClassAds that :tool:`condor_submit` creates. In this way, it is
    like the "+" syntax in a submit description file. Attributes defined
    in the submit description file with "+" will override attributes
    defined in the configuration file with :macro:`SUBMIT_ATTRS`. Note that
    adding an attribute to a job's ClassAd will not function as a method
    for specifying default values of submit description file commands
    forgotten in a job's submit description file. The command in the
    submit description file results in actions by :tool:`condor_submit`,
    while the use of :macro:`SUBMIT_ATTRS` adds a job ClassAd attribute at a
    later point in time.

:macro-def:`SUBMIT_ALLOW_GETENV`
    A boolean attribute which defaults to true. If set to false, the
    submit command "getenv = true" is an error.  Any restricted
    form of "getenv = some_env_var_name" is still allowed. 

:macro-def:`LOG_ON_NFS_IS_ERROR`
    A boolean value that controls whether :tool:`condor_submit` prohibits job
    submit description files with job event log files on NFS. If
    :macro:`LOG_ON_NFS_IS_ERROR` is set to ``True``, such submit files will
    be rejected. If :macro:`LOG_ON_NFS_IS_ERROR` is set to ``False``, the job
    will be submitted. If not defined, :macro:`LOG_ON_NFS_IS_ERROR` defaults
    to ``False``.

:macro-def:`SUBMIT_MAX_PROCS_IN_CLUSTER`
    An integer value that limits the maximum number of jobs that would
    be assigned within a single cluster. Job submissions that would
    exceed the defined value fail, issuing an error message, and with no
    jobs submitted. The default value is 0, which does not limit the
    number of jobs assigned a single cluster number.

:macro-def:`ENABLE_DEPRECATION_WARNINGS`
    A boolean value that defaults to ``False``. When ``True``,
    :tool:`condor_submit` issues warnings when a job requests features that
    are no longer supported.

:macro-def:`INTERACTIVE_SUBMIT_FILE`
    The path and file name of a submit description file that
    :tool:`condor_submit` will use in the specification of an interactive
    job. The default is ``$(RELEASE_DIR)``/libexec/interactive.sub when
    not defined.

:macro-def:`CRED_MIN_TIME_LEFT`
    When a job uses an X509 user proxy, condor_submit will refuse to
    submit a job whose x509 expiration time is less than this many
    seconds in the future. The default is to only refuse jobs whose
    expiration time has already passed.

:macro-def:`CONTAINER_SHARED_FS`
    This is a list of strings that name directories which are shared
    on the execute machines and may contain container images under them.
    The default value is /cvmfs.  When a container universe job lists
    a *condor_image* that is under one of these directories, HTCondor
    knows not to try to transfer the file to the execution point.
