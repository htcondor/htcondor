:index:`GridManager Options<single: Configuration; GridManager Options>`

.. _gridmanager_config_options:

GridManager Configuration Options
=================================

These macros affect the *condor_gridmanager*.

:macro-def:`GRIDMANAGER_LOG`
    Defines the path and file name for the log of the
    *condor_gridmanager*. The owner of the file is the condor user.

:macro-def:`GRIDMANAGER_CHECKPROXY_INTERVAL`
    The number of seconds between checks for an updated X509 proxy
    credential. The default is 10 minutes (600 seconds).

:macro-def:`GRIDMANAGER_PROXY_REFRESH_TIME`
    For remote schedulers that allow for X.509 proxy refresh,
    the *condor_gridmanager* will not forward a
    refreshed proxy until the lifetime left for the proxy on the remote
    machine falls below this value. The value is in seconds and the
    default is 21600 (6 hours).

:macro-def:`GRIDMANAGER_MINIMUM_PROXY_TIME`
    The minimum number of seconds before expiration of the X509 proxy
    credential for the gridmanager to continue operation. If seconds
    until expiration is less than this number, the gridmanager will
    shutdown and wait for a refreshed proxy credential. The default is 3
    minutes (180 seconds).

:macro-def:`HOLD_JOB_IF_CREDENTIAL_EXPIRES`
    True or False. Defaults to True. If True, and for grid universe jobs
    only, HTCondor-G will place a job on hold
    :macro:`GRIDMANAGER_MINIMUM_PROXY_TIME` seconds before the proxy expires.
    If False, the job will stay in the last known state, and HTCondor-G
    will periodically check to see if the job's proxy has been
    refreshed, at which point management of the job will resume.

:macro-def:`GRIDMANAGER_SELECTION_EXPR`
    By default, the gridmanager operates on a per-:ad-attr:`Owner` basis.  That
    is, the *condor_schedd* starts a distinct *condor_gridmanager* for each
    grid universe job with a distinct :ad-attr:`Owner`.  For additional isolation
    and/or scalability, you may set this macro to a ClassAd expression.
    It will be evaluated against each grid universe job, and jobs with
    the same evaluated result will go to the same gridmanager.  For instance,
    if you want to isolate job going to different remote sites from each
    other, the following expression works:

    .. code-block:: condor-config

          GRIDMANAGER_SELECTION_EXPR = GridResource

:macro-def:`GRIDMANAGER_LOG_APPEND_SELECTION_EXPR`
    A boolean value that defaults to ``False``. When ``True``, the
    evaluated value of :macro:`GRIDMANAGER_SELECTION_EXPR` (if set) is
    appended to the value of :macro:`GRIDMANAGER_LOG` for each
    *condor_gridmanager* instance.
    The value is sanitized to remove characters that have special
    meaning to the shell.
    This allows each *condor_gridmanager* instance that runs concurrently
    to write to a separate daemon log.

:macro-def:`GRIDMANAGER_CONTACT_SCHEDD_DELAY`
    The minimum number of seconds between connections to the
    *condor_schedd*. The default is 5 seconds.

:macro-def:`GRIDMANAGER_JOB_PROBE_INTERVAL`
    The number of seconds between active probes for the status of a
    submitted job. The default is 1 minute (60 seconds). Intervals
    specific to grid types can be set by appending the name of the grid
    type to the configuration variable name, as the example

    .. code-block:: condor-config

          GRIDMANAGER_JOB_PROBE_INTERVAL_ARC = 300


:macro-def:`GRIDMANAGER_JOB_PROBE_RATE`
    The maximum number of job status probes per second that will be
    issued to a given remote resource. The time between status probes
    for individual jobs may be lengthened beyond
    :macro:`GRIDMANAGER_JOB_PROBE_INTERVAL` to enforce this rate.
    The default is 5 probes per second. Rates specific to grid types can
    be set by appending the name of the grid type to the configuration
    variable name, as the example

    .. code-block:: condor-config

          GRIDMANAGER_JOB_PROBE_RATE_ARC = 15


:macro-def:`GRIDMANAGER_RESOURCE_PROBE_INTERVAL`
    When a resource appears to be down, how often (in seconds) the
    *condor_gridmanager* should ping it to test if it is up again.
    The default is 5 minutes (300 seconds).

:macro-def:`GRIDMANAGER_EMPTY_RESOURCE_DELAY`
    The number of seconds that the *condor_gridmanager* retains
    information about a grid resource, once the *condor_gridmanager*
    has no active jobs on that resource. An active job is a grid
    universe job that is in the queue, for which :ad-attr:`JobStatus` is
    anything other than Held. Defaults to 300 seconds.

:macro-def:`GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE`
    An integer value that limits the number of jobs that a
    *condor_gridmanager* daemon will submit to a resource. A
    comma-separated list of pairs that follows this integer limit will
    specify limits for specific remote resources. Each pair is a host
    name and the job limit for that host. Consider the example:

    .. code-block:: condor-config

          GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE = 200, foo.edu, 50, bar.com, 100


    In this example, all resources have a job limit of 200, except
    foo.edu, which has a limit of 50, and bar.com, which has a limit of
    100.

    Limits specific to grid types can be set by appending the name of
    the grid type to the configuration variable name, as the example

    .. code-block:: condor-config

          GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE_PBS = 300


    In this example, the job limit for all PBS resources is 300.
    Defaults to 1000.

:macro-def:`GAHP_DEBUG_HIDE_SENSITIVE_DATA`
    A boolean value that determines when sensitive data such as security
    keys and passwords are hidden, when communication to or from a GAHP
    server is written to a daemon log. The default is ``True``, hiding
    sensitive data.

:macro-def:`GRIDMANAGER_GAHP_CALL_TIMEOUT`
    The number of seconds after which a pending GAHP command should time
    out. The default is 5 minutes (300 seconds).

:macro-def:`GRIDMANAGER_GAHP_RESPONSE_TIMEOUT`
    The *condor_gridmanager* will assume a GAHP is hung if this many
    seconds pass without a response. The default is 20.

:macro-def:`GRIDMANAGER_MAX_PENDING_REQUESTS`
    The maximum number of GAHP commands that can be pending at any time.
    The default is 50.

:macro-def:`GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT`
    The number of times to retry a command that failed due to a timeout
    or a failed connection. The default is 3.

:macro-def:`EC2_RESOURCE_TIMEOUT`
    The number of seconds after which if an EC2 grid universe job fails
    to ping the EC2 service, the job will be put on hold. Defaults to
    -1, which implements an infinite length, such that a failure to ping
    the service will never put the job on hold.

:macro-def:`EC2_GAHP_RATE_LIMIT`
    The minimum interval, in whole milliseconds, between requests to the
    same EC2 service with the same credentials. Defaults to 100.

:macro-def:`BATCH_GAHP_CHECK_STATUS_ATTEMPTS`
    The number of times a failed status command issued to the
    *blahpd* should be retried. These retries allow the
    *condor_gridmanager* to tolerate short-lived failures of the
    underlying batch system. The default value is 5.

:macro-def:`C_GAHP_LOG`
    The complete path and file name of the HTCondor GAHP server's log.
    The default value is ``/tmp/CGAHPLog.$(USERNAME)``.

:macro-def:`MAX_C_GAHP_LOG`
    The maximum size of the :macro:`C_GAHP_LOG`.

:macro-def:`C_GAHP_WORKER_THREAD_LOG`
    The complete path and file name of the HTCondor GAHP worker process'
    log. The default value is ``/temp/CGAHPWorkerLog.$(USERNAME)``.

:macro-def:`C_GAHP_CONTACT_SCHEDD_DELAY`
    The number of seconds that the *condor_C-gahp* daemon waits between
    consecutive connections to the remote *condor_schedd* in order to
    send batched sets of commands to be executed on that remote
    *condor_schedd* daemon. The default value is 5.

:macro-def:`C_GAHP_MAX_FILE_REQUESTS`
    Limits the number of file transfer commands of each type (input,
    output, proxy refresh) that are performed before other (potentially
    higher-priority) commands are read and performed.
    The default value is 10.

:macro-def:`BLAHPD_LOCATION`
    The complete path to the directory containing the *blahp* software,
    which is required for grid-type batch jobs.
    The default value is ``$(RELEASE_DIR)``.

:macro-def:`GAHP_SSL_CADIR`
    The path to a directory that may contain the certificates (each in
    its own file) for multiple trusted CAs to be used by GAHP servers
    when authenticating with remote services.

:macro-def:`GAHP_SSL_CAFILE`
    The path and file name of a file containing one or more trusted CA's
    certificates to be used by GAHP servers when authenticating with
    remote services.

:macro-def:`CONDOR_GAHP`
    The complete path and file name of the HTCondor GAHP executable. The
    default value is ``$(SBIN)``/condor_c-gahp.

:macro-def:`EC2_GAHP`
    The complete path and file name of the EC2 GAHP executable. The
    default value is ``$(SBIN)``/ec2_gahp.

:macro-def:`BATCH_GAHP`
    The complete path and file name of the batch GAHP executable, to be
    used for Slurm, PBS, LSF, SGE, and similar batch systems. The default
    location is ``$(BIN)``/blahpd.

:macro-def:`ARC_GAHP`
    The complete path and file name of the ARC GAHP executable.
    The default value is ``$(SBIN)``/arc_gahp.

:macro-def:`ARC_GAHP_COMMAND_LIMIT`
    On systems where libcurl uses NSS for security, start a new
    *arc_gahp* process when the existing one has handled the given
    number of commands.
    The default is 1000.

:macro-def:`ARC_GAHP_USE_THREADS`
    Controls whether the *arc_gahp* should run multiple HTTPS requests
    in parallel in different threads.
    The default is ``False``.

:macro-def:`GCE_GAHP`
    The complete path and file name of the GCE GAHP executable. The
    default value is ``$(SBIN)``/gce_gahp.

:macro-def:`AZURE_GAHP`
    The complete path and file name of the Azure GAHP executable. The
    default value is ``$(SBIN)``/AzureGAHPServer.py on Windows and
    ``$(SBIN)``/AzureGAHPServer on other platforms.

:macro-def:`BOINC_GAHP`
    The complete path and file name of the BOINC GAHP executable. The
    default value is ``$(SBIN)``/boinc_gahp.
