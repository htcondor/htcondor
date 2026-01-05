:index:`Job Router Options<single: Configuration; Job Router Options>`

.. _job_router_config_options:

Job Router Configuration Options
================================

These macros affect the *condor_job_router* daemon.


:macro-def:`JOB_ROUTER_ROUTE_NAMES`
    An ordered list of the names of enabled routes.  In version 8.9.7 or later,
    routes whose names are listed here should each have a :macro:`JOB_ROUTER_ROUTE_<NAME>`
    configuration variable that specifies the route.

    Routes will be matched to jobs in the order their names are declared in this list.  Routes not
    declared in this list will be disabled.  

:macro-def:`JOB_ROUTER_ROUTE_<NAME>`
    Specification of a single route in transform syntax.  ``<NAME>`` should be one of the
    route names specified in :macro:`JOB_ROUTER_ROUTE_NAMES`. The transform syntax is specified
    in the :ref:`classads/transforms:ClassAd Transforms` section of this manual.

:macro-def:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES`
    An ordered list of the names of transforms that should be applied when a job is being
    routed before the route transform is applied.  Each transform name listed here should
    have a corresponding :macro:`JOB_ROUTER_TRANSFORM_<NAME>` configuration variable.

:macro-def:`JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES`
    An ordered list of the names of transforms that should be applied when a job is being
    routed after the route transform is applied.  Each transform name listed here should
    have a corresponding :macro:`JOB_ROUTER_TRANSFORM_<NAME>` configuration variable.

:macro-def:`JOB_ROUTER_TRANSFORM_<NAME>`
    Specification of a single pre-route or post-route transform.  ``<NAME>`` should be one of the
    route names specified in :macro:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES` or in :macro:`JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES`.
    The transform syntax is specified in the :ref:`classads/transforms:ClassAd Transforms` section of this manual.

:macro-def:`JOB_ROUTER_ENTRIES_REFRESH`
    The number of seconds between updates to the routing table. The
    default value is 0, meaning no periodic updates occur. With the
    default value of 0, the routing table can be modified when a
    :tool:`condor_reconfig` command is invoked or when the
    *condor_job_router* daemon restarts.

:macro-def:`JOB_ROUTER_LOCK`
    This specifies the name of a lock file that is used to ensure that
    multiple instances of condor_job_router never run with the same
    :macro:`JOB_ROUTER_NAME`. Multiple instances running with the same name
    could lead to mismanagement of routed jobs. The default value is
    $(LOCK)/$(JOB_ROUTER_NAME)Lock.

:macro-def:`JOB_ROUTER_SOURCE_JOB_CONSTRAINT`
    Specifies a global ``Requirements`` expression that must be true for
    all newly routed jobs, in addition to any ``Requirements`` specified
    within a routing table entry. In addition to the configurable
    constraints, the *condor_job_router* also has some hard-coded
    constraints. It avoids recursively routing jobs by requiring that
    the job's attribute ``RoutedBy`` does not match :macro:`JOB_ROUTER_NAME`.
    When not running as root, it also avoids routing jobs belonging to other users.

:macro-def:`JOB_ROUTER_MAX_JOBS`
    An integer value representing the maximum number of jobs that may be
    routed, summed over all routes. The default value is -1, which means
    an unlimited number of jobs may be routed.

:macro-def:`JOB_ROUTER_DEFAULT_MAX_JOBS_PER_ROUTE`
    An integer value representing the maximum number of jobs that may be
    routed to a single route when the route does not specify a ``MaxJobs``
    value. The default value is 100.

:macro-def:`JOB_ROUTER_DEFAULT_MAX_IDLE_JOBS_PER_ROUTE`
    An integer value representing the maximum number of jobs in a single
    route that may be in the idle state.  When the number of jobs routed
    to that site exceeds this number, no more jobs will be routed to it. 
    A route may specify ``MaxIdleJobs`` to override this number.
    The default value is 50.

:macro-def:`MAX_JOB_MIRROR_UPDATE_LAG`
    An integer value that administrators will rarely consider changing,
    representing the maximum number of seconds the *condor_job_router*
    daemon waits, before it decides that routed copies have gone awry,
    due to the failure of events to appear in the *condor_schedd* 's
    job queue log file. The default value is 600. As the
    *condor_job_router* daemon uses the *condor_schedd* 's job queue
    log file entries for synchronization of routed copies, when an
    expected log file event fails to appear after this wait period, the
    *condor_job_router* daemon acts presuming the expected event will
    never occur.

:macro-def:`JOB_ROUTER_POLLING_PERIOD`
    An integer value representing the number of seconds between cycles
    in the *condor_job_router* daemon's task loop. The default is 10
    seconds. A small value makes the *condor_job_router* daemon quick
    to see new candidate jobs for routing. A large value makes the
    *condor_job_router* daemon generate less overhead at the cost of
    being slower to see new candidates for routing. For very large job
    queues where a few minutes of routing latency is no problem,
    increasing this value to a few hundred seconds would be reasonable.

:macro-def:`JOB_ROUTER_NAME`
    A unique identifier utilized to name multiple instances of the
    *condor_job_router* daemon on the same machine. Each instance must
    have a different name, or all but the first to start up will refuse
    to run. The default is ``"jobrouter"``.

    Changing this value when routed jobs already exist is not currently
    gracefully handled. However, it can be done if one also uses
    :tool:`condor_qedit` to change the value of ``ManagedManager`` and
    ``RoutedBy`` from the old name to the new name. The following
    commands may be helpful:

    .. code-block:: console

        $ condor_qedit -constraint \
        'RoutedToJobId =!= undefined && ManagedManager == "insert_old_name"' \
          ManagedManager '"insert_new_name"'
        $ condor_qedit -constraint \
        'RoutedBy == "insert_old_name"' RoutedBy '"insert_new_name"'

:macro-def:`JOB_ROUTER_RELEASE_ON_HOLD`
    A boolean value that defaults to ``True``. It controls how the
    *condor_job_router* handles the routed copy when it goes on hold.
    When ``True``, the *condor_job_router* leaves the original job
    ClassAd in the same state as when claimed. When ``False``, the
    *condor_job_router* does not attempt to reset the original job
    ClassAd to a pre-claimed state upon yielding control of the job.

:macro-def:`JOB_ROUTER_SCHEDD1_ADDRESS_FILE`
    The path to the address file file for the *condor_schedd*
    serving as the source of jobs for routing.  If specified,
    this must point to the file configured as :macro:`SCHEDD_ADDRESS_FILE`
    of the *condor_schedd* identified by :macro:`JOB_ROUTER_SCHEDD1_NAME`.
    When configured, the *condor_job_router* will first look in this
    address file to get the address of the source schedd and will only
    query the collector specified in :macro:`JOB_ROUTER_SCHEDD1_POOL`
    if it does not find an address in that file.

:macro-def:`JOB_ROUTER_SCHEDD2_ADDRESS_FILE`
    The path to the job_queue.log file for the *condor_schedd*
    serving as the destination of jobs for routing.  If specified,
    this must point to the the file configured as :macro:`SCHEDD_ADDRESS_FILE`
    of the *condor_schedd* identified by :macro:`JOB_ROUTER_SCHEDD2_NAME`.
    When configured, the *condor_job_router* will first look in this
    address file to get the address of the destination schedd and will only
    query the collector specified in :macro:`JOB_ROUTER_SCHEDD2_POOL`
    if it does not find an address in that file.

:macro-def:`JOB_ROUTER_SCHEDD1_JOB_QUEUE_LOG`
    The path to the job_queue.log file for the *condor_schedd*
    serving as the source of jobs for routing.  If specified,
    this must point to the job_queue.log file of the *condor_schedd*
    identified by :macro:`JOB_ROUTER_SCHEDD1_NAME`.

:macro-def:`JOB_ROUTER_SCHEDD2_JOB_QUEUE_LOG`
    The path to the job_queue.log file for the *condor_schedd*
    serving as the destination of jobs for routing.  If specified,
    this must point to the job_queue.log file of the *condor_schedd*
    identified by :macro:`JOB_ROUTER_SCHEDD2_NAME`.

:macro-def:`JOB_ROUTER_SCHEDD1_NAME`
    The advertised daemon name of the *condor_schedd* serving as the
    source of jobs for routing. If not specified, this defaults to the
    local *condor_schedd*. If the named *condor_schedd* is not
    advertised in the local pool, :macro:`JOB_ROUTER_SCHEDD1_POOL` will
    also need to be set.

:macro-def:`JOB_ROUTER_SCHEDD2_NAME`
    The advertised daemon name of the *condor_schedd* to which the
    routed copy of the jobs are submitted. If not specified, this
    defaults to the local *condor_schedd*. If the named *condor_schedd* is not
    advertised in the local pool, :macro:`JOB_ROUTER_SCHEDD2_POOL` will
    also need to be set. Note that when *condor_job_router* is running
    as root and is submitting routed jobs to a different *condor_schedd*
    than the source *condor_schedd*, it is required that *condor_job_router*
    have permission to impersonate the job owners of the routed jobs. It
    is therefore usually necessary to configure
    :macro:`QUEUE_SUPER_USER_MAY_IMPERSONATE` in the configuration
    of the target *condor_schedd*.

:macro-def:`JOB_ROUTER_SCHEDD1_POOL`
    The Condor pool (*condor_collector* address) of the
    *condor_schedd* serving as the source of jobs for routing. If not
    specified, defaults to the local pool.

:macro-def:`JOB_ROUTER_SCHEDD2_POOL`
    The Condor pool (*condor_collector* address) of the
    *condor_schedd* to which the routed copy of the jobs are submitted.
    If not specified, defaults to the local pool.

:macro-def:`JOB_ROUTER_ROUND_ROBIN_SELECTION`
    A boolean value that controls which route is chosen for a candidate
    job that matches multiple routes. When set to ``False``, the
    default, the first matching route is always selected. When set to
    ``True``, the Job Router attempts to distribute jobs across all
    matching routes, round robin style.

:macro-def:`JOB_ROUTER_CREATE_IDTOKEN_NAMES`
    An list of the names of IDTOKENs that the JobRouter should create and refresh.
    IDTOKENS whose names are listed here should each have a :macro:`JOB_ROUTER_CREATE_IDTOKEN_<NAME>`
    configuration variable that specifies the filename, ownership and properties of the IDTOKEN.

:macro-def:`JOB_ROUTER_IDTOKEN_REFRESH`
    An integer value of seconds that controls the rate at which the JobRouter will refresh
    the IDTOKENS listed by the :macro:`JOB_ROUTER_CREATE_IDTOKEN_NAMES` configuration variable.

:macro-def:`JOB_ROUTER_CREATE_IDTOKEN_<NAME>`
    Specification of a single IDTOKEN that will be created an refreshed by the JobRouter.
    ``<NAME>`` should be one of the IDTOKEN names specified in :macro:`JOB_ROUTER_CREATE_IDTOKEN_NAMES`.
    The filename, ownership and properties of the IDTOKEN are defined by the following attributes.
    Each attribute value must be a classad expression that evaluates to a string, except ``lifetime``
    which must evaluate to an integer.

:macro-def:`kid`
    The ID of the token signing key to use, equivalent to the ``-key`` argument of :tool:`condor_token_create`
    and the ``kid`` attribute of :tool:`condor_token_list`.  Defaults to "POOL"

:macro-def:`sub`
    The subject or user identity, equivalent to the ``-identity`` argument of :tool:`condor_token_create`
    and the ``sub`` attribute of :tool:`condor_token_list`. Defaults the token name.

:macro-def:`scope`
    List of allowed authorizations, equivalent to the ``-authz`` argument of :tool:`condor_token_create`
    and the ``scope`` attribute of :tool:`condor_token_list`. 

:macro-def:`lifetime`
    Time in seconds that the IDTOKEN is valid after creation, equivalent to the ``-lifetime`` argument of :tool:`condor_token_create`.
    The ``exp`` attribute of :tool:`condor_token_list` is the creation time of the token plus this value.

:macro-def:`file`
    The filename of the IDTOKEN file, equivalent to the ``-token`` argument of :tool:`condor_token_create`.
    Defaults to the token name.

:macro-def:`dir`
    The directory that the IDTOKEN file will be created and refreshed into. Defaults to ``$(SEC_TOKEN_DIRECTORY)``.

:macro-def:`owner`
    If specified, the IDTOKEN file will be owned by this user.  If not specified, the IDTOKEN file will be owned
    by the owner of *condor_job_router* process.  This attribute is optional if the *condor_job_router* is running as an ordinary user
    but required if it is running as a Windows service or as the ``root`` or ``condor`` user.  The owner specified here
    should be the same as the :ad-attr:`Owner` attribute of the jobs that this IDTOKEN is intended to be sent to.

:macro-def:`JOB_ROUTER_SEND_ROUTE_IDTOKENS`
    List of the names of the IDTOKENS to add to the input file transfer list of each routed job. This list should be one or
    more of the IDTOKEN names specified by the :macro:`JOB_ROUTER_CREATE_IDTOKEN_NAMES`.
    If the route has a ``SendIDTokens`` definition, this configuration variable is not used for that route.
