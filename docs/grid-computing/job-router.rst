The HTCondor Job Router
=======================

:index:`Job Router` :index:`condor_job_router daemon`

The HTCondor Job Router is an add-on to the *condor_schedd* that
transforms jobs from one type into another according to a configurable
policy. This process of transforming the jobs is called job routing.

One example of how the Job Router can be used is for the task of sending
excess jobs to one or more remote grid sites. The Job Router can
transform the jobs such as vanilla universe jobs into grid universe jobs
that use any of the grid types supported by HTCondor. The rate at which
jobs are routed can be matched roughly to the rate at which the site is
able to start running them. This makes it possible to balance a large
work flow across multiple grid sites, a local HTCondor pool, and any
flocked HTCondor pools, without having to guess in advance how quickly
jobs will run and complete in each of the different sites.

Job Routing is most appropriate for high throughput work flows, where
there are many more jobs than computers, and the goal is to keep as many
of the computers busy as possible. Job Routing is less suitable when
there are a small number of jobs, and the scheduler needs to choose the
best place for each job, in order to finish them as quickly as possible.
The Job Router does not know which site will run the jobs faster, but it
can decide whether to send more jobs to a site, based on whether jobs
already submitted to that site are sitting idle or not, as well as
whether the site has experienced recent job failures.

Routing Mechanism
-----------------

The *condor_job_router* daemon and configuration determine a policy
for which jobs may be transformed and sent to grid sites. By default, a
job is transformed into a grid universe job by making a copy of the
original job ClassAd, and modifying some attributes in this copy of the
job. The copy is called the routed copy, and it shows up in the job
queue under a new job id.

Until the routed copy finishes or is removed, the original copy of the
job passively mirrors the state of the routed job. During this time, the
original job is not available for matchmaking, because it is tied to the
routed copy. The original job also does not evaluate periodic
expressions, such as ``PeriodicHold``. Periodic expressions are
evaluated for the routed copy. When the routed copy completes, the
original job ClassAd is updated such that it reflects the final status
of the job. If the routed copy is removed, the original job returns to
the normal idle state, and is available for matchmaking or rerouting.
If, instead, the original job is removed or goes on hold, the routed
copy is removed.

Although the default mode routes vanilla universe jobs to grid universe
jobs, the routing rules may be configured to do some other
transformation of the job. It is also possible to edit the job in place
rather than creating a new transformed version of the job.

The *condor_job_router* daemon utilizes a routing table, in which a
ClassAd transform describes each site to where jobs may be sent.

There is also a list of pre-route and post-route transforms that are
applied whenever a job is routed.

The routing table is given as a set of configuration macros.  Each configuration macro
is given in the job transform language. This is the same transform language used by the
*condor_schedd* for job transforms.  This language is similar to the
:tool:`condor_submit` language, but has commands to describe the
transform steps and optional macro values such as ``MaxJobs`` that can control the way
the route is used.

When a route matches a job, and the *condor_job_router* is about to apply
the routing transform, it will first apply all of the pre-route transforms
that match that job, then it will apply the routing transform, then it will
apply all of the post-route transforms that match the job.

In older versions the routing table was given as a list of ClassAds,
and for backwards compatibility this form of configuration is still
supported - It will be converted automatically into a set of job transforms.


Job Submission with Job Routing Capability
------------------------------------------

If Job Routing is set up, then the following items ought to be
considered for jobs to have the necessary prerequisites to be considered
for routing.

-  Jobs appropriate for routing to the grid must not rely on access to a
   shared file system, or other services that are only available on the
   local pool. The job will use HTCondor's file transfer mechanism,
   rather than relying on a shared file system to access input files and
   write output files. In the submit description file, to enable file
   transfer, there will be a set of commands similar to

   .. code-block:: condor-submit

       should_transfer_files = YES
       when_to_transfer_output = ON_EXIT
       transfer_input_files = input1, input2
       transfer_output_files = output1, output2

   Vanilla universe jobs and most types of grid universe jobs differ in
   the set of files transferred back when the job completes. Vanilla
   universe jobs transfer back all files created or modified, while all
   grid universe jobs, except for HTCondor-C, only transfer back the
   :subcom:`output[and job router]` file, as well as
   those explicitly listed with
   :subcom:`transfer_output_files[and job router]`
   Therefore, when routing jobs to grid universes other than HTCondor-C,
   it is important to explicitly specify all output files that must be
   transferred upon job completion.

-  One configuration for routed jobs requires the jobs to identify
   themselves as candidates for Job Routing. This may be accomplished by
   inventing a ClassAd attribute that the configuration utilizes in
   setting the policy for job identification, and the job defines this
   attribute to identify itself. If the invented attribute is called
   ``WantJobRouter``, then the job identifies itself as a job that may
   be routed by placing in the submit description file:

   .. code-block:: condor-submit

       +WantJobRouter = True

   This implementation can be taken further, allowing the job to first
   be rejected within the local pool, before being a candidate for Job
   Routing:

   .. code-block:: condor-submit

       +WantJobRouter = LastRejMatchTime =!= UNDEFINED

-  As appropriate to the potential grid site, create a grid proxy, and
   specify it in the submit description file:

   .. code-block:: condor-submit

       x509userproxy = /tmp/x509up_u275

   This is not necessary if the *condor_job_router* daemon is
   configured to add a grid proxy on behalf of jobs.

Job submission does not change for jobs that may be routed.

.. code-block:: console

      $ condor_submit job1.sub

where ``job1.sub`` might contain:

.. code-block:: condor-submit

    universe = vanilla
    executable = my_executable
    output = job1.stdout
    error = job1.stderr
    log = job1.ulog
    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    +WantJobRouter = LastRejMatchTime =!= UNDEFINED
    x509userproxy = /tmp/x509up_u275
    queue

The status of the job may be observed as with any other HTCondor job,
for example by looking in the job's log file. Before the job completes,
:tool:`condor_q` shows the job's status. Should the job become routed, a
second job will enter the job queue. This is the routed copy of the
original job. The command :tool:`condor_router_q` shows a more specialized
view of routed jobs, as this example shows:

.. code-block:: console

    $ condor_router_q -S
       JOBS ST Route      GridResource
         40  I Site1      site1.edu/jobmanager-condor
         10  I Site2      site2.edu/jobmanager-pbs
          2  R Site3      condor submit.site3.edu condor.site3.edu

:tool:`condor_router_history` summarizes the history of routed jobs, as this
example shows:

.. code-block:: console

    $ condor_router_history
    Routed job history from 2007-06-27 23:38 to 2007-06-28 23:38

    Site            Hours    Jobs    Runs
                          Completed Aborted
    -------------------------------------------------------
    Site1              10       2     0
    Site2               8       2     1
    Site3              40       6     0
    -------------------------------------------------------
    TOTAL              58      10     1

An Example Configuration
------------------------

The following sample configuration sets up potential job routing to
three routes (grid sites). Definitions of the configuration variables
specific to the Job Router are in the 
:ref:`admin-manual/configuration-macros:condor_job_router configuration file
entries` section. One route a local SLURM cluster.
A second route is cluster accessed via ARC CE. The third
site is an HTCondor site accessed by HTCondor-C. The *condor_job_router* daemon
does not know which site will be best for a given job. The policy implemented in
this sample configuration stops sending more jobs to a site, if ten jobs
that have already been sent to that site are idle.

These configuration settings belong in the local configuration file of
the machine where jobs are submitted. Check that the machine can
successfully submit grid jobs before setting up and using the Job
Router. Typically, the single required element that needs to be added
for SSL authentication is an X.509 trusted certification authority
directory, in a place recognized by HTCondor (for example,
``/etc/grid-security/certificates``).

Note that, as of version 8.5.6, the configuration language supports
multi-line values, as shown in the example below (see the
:ref:`admin-manual/introduction-to-configuration:multi-line values` section
for more details).

The list of enabled routes is specified by :macro:`JOB_ROUTER_ROUTE_NAMES`, routes
will be considered in the order given by this configuration variable.

.. code-block:: condor-config

    # define a global constraint, only jobs that match this will be considered for routing
    JOB_ROUTER_SOURCE_JOB_CONSTRAINT = WantJobRouter

    # define a default maximum number of jobs that will be matched to each route
    # and a limit on the number of idle jobs a route may have before we stop using it.
    JOB_ROUTER_DEFAULT_MAX_JOBS_PER_ROUTE = 200
    JOB_ROUTER_DEFAULT_MAX_IDLE_JOBS_PER_ROUTE = 10

    # This could be made an attribute of the job, rather than being hard-coded
    ROUTED_JOB_MAX_TIME = 1440

    # Now we define each of the routes to send jobs to
    JOB_ROUTER_ROUTE_NAMES = Site1 Site2 CondorSite

    JOB_ROUTER_ROUTE_Site1 @=rt
      GridResource = "batch slurm"
    @rt

    JOB_ROUTER_ROUTE_Site2 @=rt
      GridResource = "arc site2.edu"
      SET ArcRte = "ENV/PROXY"
    @rt

    JOB_ROUTER_ROUTE_CondorSite @=rt
      MaxIdleJobs = 20
      GridResource = "condor submit.site3.edu cm.site3.edu"
      SET remote_jobuniverse = 5
    @rt

    # define a pre-route transform that does the transforms all routes should do
    JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES = Defaults

    JOB_ROUTER_TRANSFORM_Defaults @=jrd
       # remove routed job if it goes on hold or stays idle for over 6 hours
       SET PeriodicRemove = JobStatus == 5 || \
                           (JobStatus == 1 && (time() - QDate) > 3600*6))
       # delete the global SOURCE_JOB_CONSTRAINT attribute so that routed jobs will not be routed again
       DELETE WantJobRouter
       SET Requirements = true
    @jrd


    # Reminder: you must restart HTCondor for changes to DAEMON_LIST to take effect.
    DAEMON_LIST = $(DAEMON_LIST) JOB_ROUTER

    # For testing, set this to a small value to speed things up.
    # Once you are running at large scale, set it to a higher value
    # to prevent the JobRouter from using too much cpu.
    JOB_ROUTER_POLLING_PERIOD = 10

    #It is good to save lots of schedd queue history
    #for use with the router_history command.
    MAX_HISTORY_ROTATIONS = 20

Routing Table Entry Commands and Macro values
-----------------------------------------------

A route consists of a sequence of Macro values and commands which are applied
in order to produce the routed job ClassAd.  Certain macro names have special meaning
when used in a router transform.  These special macro names are listed below
along a brief listing of the the transform commands.  For a more detailed description
of the transform commands refer to the :ref:`classads/transforms:Transform Commands` section.

The conversion of a job to a routed copy will usually require the job ClassAd to
be modified. The Routing Table specifies attributes of the different
possible routes and it may specify specific modifications that should be
made to the job when it is sent along a specific route. In addition to
this mechanism for transforming the job, external programs may be
invoked to transform the job. For more information, see
below: ref:`grid-computing/job-router:hooks for the job router`

The following attributes and instructions for modifying job attributes
may appear in a Routing Table entry.

:index:`GridResource<single: GridResource; Job Router Routing Table ClassAd attribute>`

``GridResource = <string>``
    Specifies the value for the :ad-attr:`GridResource` attribute that will be
    inserted into the routed copy of the job's ClassAd.

:index:`MaxJobs<single: MaxJobs; Job Router Routing Table ClassAd attribute>`

``MaxJobs = <integer>``
    An integer maximum number of jobs permitted on the route at one
    time. The default is 100.

:index:`MaxIdleJobs<single: MaxIdleJobs; Job Router Routing Table ClassAd attribute>`

``MaxIdleJobs = <integer>``
    An integer maximum number of routed jobs in the idle state. At or
    above this value, no more jobs will be sent to this site. This is
    intended to prevent too many jobs from being sent to sites which are
    too busy to run them. If the value set for this attribute is too
    small, the rate of job submission to the site will slow, because the
    *condor_job_router* daemon will submit jobs up to this limit, wait
    to see some of the jobs enter the running state, and then submit
    more. The disadvantage of setting this attribute's value too high is
    that a lot of jobs may be sent to a site, only to site idle for
    hours or days. The default value is 50.

:index:`FailureRateThreshold<single: FailureRateThreshold; Job Router Routing Table ClassAd attribute>`

``FailureRateThreshold = <float>``
    A maximum tolerated rate of job failures. Failure is determined by
    the expression sets for the attribute ``JobFailureTest`` expression.
    The default threshold is 0.03 jobs/second. If the threshold is
    exceeded, submission of new jobs is throttled until jobs begin
    succeeding, such that the failure rate is less than the threshold.
    This attribute implements black hole throttling, such that a site at
    which jobs are sent only to fail (a black hole) receives fewer jobs.

:index:`JobFailureTest<single: JobFailureTest; Job Router Routing Table ClassAd attribute>`

``JobFailureTest = <boolean expr>``
    An expression evaluated for each job that finishes, to determine
    whether it was a failure. The default value if no expression is
    defined assumes all jobs are successful. Routed jobs that are
    removed are considered to be failures. An example expression to
    treat all jobs running for less than 30 minutes as failures is
    ``target.RemoteWallClockTime < 1800``. A more flexible expression
    might reference a property or expression of the job that specifies a
    failure condition specific to the type of job.

:index:`SendIDTokens<single: SendIDTokens; Job Router Routing Table attribute>`

``SendIDTokens = <string expr>``
    A string expression that lists the names of the IDTOKENS to add to the
    input file transfer list of the routed job. The string should list one or
    more of the IDTOKEN names specified by the :macro:`JOB_ROUTER_CREATE_IDTOKEN_NAMES`
    configuration variable.
    if ``SendIDTokens`` is not specified, then the value of the JobRouter
    configuration variable :macro:`JOB_ROUTER_SEND_ROUTE_IDTOKENS` will be used.

:index:`UseSharedX509UserProxy<single: UseSharedX509UserProxy; Job Router Routing Table ClassAd attribute>`

``UseSharedX509UserProxy = <boolean epr>``
    A boolean expression that when ``True`` causes the value of
    ``SharedX509UserProxy`` to be the X.509 user proxy for the routed
    job. Note that if the *condor_job_router* daemon is running as
    root, the copy of this file that is given to the job will have its
    ownership set to that of the user running the job. This requires the
    trust of the user. It is therefore recommended to avoid this
    mechanism when possible. Instead, require users to submit jobs with
    :ad-attr:`X509UserProxy` set in the submit description file. If this
    feature is needed, use the boolean expression to only allow specific
    values of ``target.Owner`` to use this shared proxy file. The shared
    proxy file should be owned by the condor user. Currently, to use a
    shared proxy, the job must also turn on sandboxing by having the
    attribute ``JobShouldBeSandboxed``.

:index:`SharedX509UserProxy<single: SharedX509UserProxy; Job Router Routing Table ClassAd attribute>`

``SharedX509UserProxy = <string>``
    A string representing file containing the X.509 user proxy for the
    routed job.

:index:`JobShouldBeSandboxed<single: JobShouldBeSandboxed; Job Router Routing Table ClassAd attribute>`

``JobShouldBeSandboxed = <boolean expr>``
    A boolean expression that when ``True`` causes the created copy of
    the job to be sandboxed. A copy of the input files will be placed in
    the *condor_schedd* daemon's spool area for the target job, and
    when the job runs, the output will be staged back into the spool
    area. Once all of the output has been successfully staged back, it
    will be copied again, this time from the spool area of the sandboxed
    job back to the original job's output locations. By default,
    sandboxing is turned off. Only to turn it on if using a shared X.509
    user proxy or if direct staging of remote output files back to the
    final output locations is not desired.

:index:`EditJobInPlace<single: EditJobInPlace; Job Router Routing Table ClassAd attribute>`

``EditJobInPlace = <boolean expr>``
    A boolean expression that, when ``True``, causes the original job to
    be transformed in place rather than creating a new transformed
    version (a routed copy) of the job. In this mode, the Job Router
    Hook :macro:`<Keyword>_HOOK_TRANSLATE_JOB` and transformation rules
    in the routing table are applied during the job transformation. The
    routing table attribute :ad-attr:`GridResource` is ignored, and there is no
    default transformation of the job from a vanilla job to a grid
    universe job as there is otherwise. Once transformed, the job is
    still a candidate for matching routing rules, so it is up to the
    routing logic to control whether the job may be transformed multiple
    times or not. For example, to transform the job only once, an
    attribute could be set in the job ClassAd to prevent it from
    matching the same routing rule in the future. To transform the job
    multiple times with limited frequency, a timestamp could be inserted
    into the job ClassAd marking the time of the last transformation,
    and the routing entry could require that this timestamp either be
    undefined or older than some limit.

:index:`UNIVERSE<single: UNIVERSE; Job Router Routing Table command>`

``UNIVERSE <value>``
    A universe name or integer value specifying the desired universe for the routed copy
    of the job. The default value is 9, which is the **grid** universe.

:index:`REQUIREMENTS<single: REQUIREMENTS; Job Router Routing Table command>`

``REQUIREMENTS <expr>``
    A constraint expression that identifies jobs that may be
    matched to the route. If there is a :macro:`JOB_ROUTER_SOURCE_JOB_CONSTRAINT`
    then only jobs that match that constraint *and* this constraint expression
    can match this route.  These constraints are evaluated before any routing
    transforms are applied.

:index:`SET <attr><single: SET <attr>; Job Router Routing Table command>`

``SET <attr> <expr>``
    Sets the value of ``<attr>`` in the routed copy's job ClassAd to the
    specified value. An example of an attribute that might be set is
    ``PeriodicRemove``. For example, if the routed job goes on hold or
    stays idle for too long, remove it and return the original copy of
    the job to a normal state.

:index:`DEFAULT <attr><single: DEFAULT <attr>; Job Router Routing Table command>`

``DEFAULT <attr> <expr>``
    Sets the value of ``<attr>`` if the value is currently missing or undefined.
    This is equivalent to

    .. code-block:: condor-config

      if ! defined MY.<Attr>
        SET <Attr> <value>
      endif


:index:`EVALSET <attr><single: EVALSET <attr>; Job Router Routing Table command>`

``EVALSET <attr> <expr>``
    Defines an expression. The expression is evaluated, and the
    resulting value sets the value of the routed copy's job ClassAd
    attribute ``<attr>``. Use this when the attribute must not be an expression
    or when information available only to the *condor_job_router* is needed to
    determine the value. 

:index:`EVALMACRO <var><single: EVALMACRO <var>; Job Router Routing Table command>`

``EVALMACRO <var> <expr>``
    Defines an expression. The expression is evaluated, and the
    resulting value is store in the temporary variable ``<var>``.
    ``$(var)`` can the be used in later statements in this route or
    in a later transform that is part of this route.  This is often use to
    evaluate complex expressions that can later be used in ``if`` statements in the route.

:index:`COPY <attr><single: COPY <attr>; Job Router Routing Table command>`

``COPY <attr> <newattr>``
    Copies the value of ``<attr>`` from the original attribute name to a new attribute
    name in the routed copy. Useful to save the value of an expression that you intend
    to change as part of the route so that the value prior to routing is still visible in the job ClassAd.

``COPY /<regex>/ <attrpat>``
    Copies all attributes that match the regular expression ``<regex>`` to new attribute names.

:index:`RENAME <attr><single: RENAME <attr>; Job Router Routing Table command>`

``RENAME <attr> <newattr>``
    Renames the attribute ``<attr>`` to a new attribute name. This is the equivalent of 
    a COPY statement followed by a DELETE statement. 

``RENAME /<regex>/ <attrpat>``
    Renames all attributes that match the regular expression ``<regex>`` to new attribute names.

:index:`DELETE <attr><single: DELETE <attr>; Job Router Routing Table command>`

``DELETE <attr>``
    Deletes ``<attr>`` from the routed copy of the job ClassAd.

``DELETE /<regex>/``
    Deletes all attributes that match the regular expression ``<regex>`` from the routed copy of the job.


Hooks for the Job Router
------------------------

:index:`Job Router hooks<single: Job Router hooks; Hooks>`

Job Router Hooks allow for an alternate transformation and/or monitoring
than the *condor_job_router* daemon implements. Routing is still
managed by the *condor_job_router* daemon, but if the Job Router Hooks
are specified, then these hooks will be used to transform and monitor
the job instead.

Job Router Hooks are similar in concept to Fetch Work Hooks, but they
are limited in their scope. A hook is an external program or script
invoked by the *condor_job_router* daemon at various points during the
life cycle of a routed job.

The following sections describe how and when these hooks are used, what
hooks are invoked at various stages of the job's life, and how to
configure HTCondor to use these Hooks.

Hooks Invoked for Job Routing
'''''''''''''''''''''''''''''

:index:`Job Router`

The Job Router Hooks allow for replacement of the transformation engine
used by HTCondor for routing a job. Since the external transformation
engine is not controlled by HTCondor, additional hooks provide a means
to update the job's status in HTCondor, and to clean up upon exit or
failure cases. This allows one job to be transformed to just about any
other type of job that HTCondor supports, as well as to use execution
nodes not normally available to HTCondor.

It is important to note that if the Job Router Hooks are utilized, then
HTCondor will not ignore or work around a failure in any hook execution.
If a hook is configured, then HTCondor assumes its invocation is
required and will not continue by falling back to a part of its internal
engine. For example, if there is a problem transforming the job using
the hooks, HTCondor will not fall back on its transformation
accomplished without the hook to process the job.

There are 2 ways in which the Job Router Hooks may be enabled. A job's
submit description file may cause the hooks to be invoked with

.. code-block:: condor-submit

    +HookKeyword = "HOOKNAME"

Adding this attribute to the job's ClassAd causes the
*condor_job_router* daemon on the access point to invoke hooks
prefixed with the defined keyword. ``HOOKNAME`` is a string chosen as an
example; any string may be used.

The job's ClassAd attribute definition of :ad-attr:`HookKeyword` takes
precedence, but if not present, hooks may be enabled by defining on the
access point the configuration variable

.. code-block:: condor-config

     JOB_ROUTER_HOOK_KEYWORD = HOOKNAME

Like the example attribute above, ``HOOKNAME`` represents a chosen name
for the hook, replaced as desired or appropriate.

There are 4 hooks that the Job Router can be configured to use. Each
hook will be described below along with data passed to the hook and
expected output. All hooks must exit successfully.
:index:`Translate Job<single: Translate Job; Job Router Hooks>`

-  The hook defined by the configuration variable
   :macro:`<Keyword>_HOOK_TRANSLATE_JOB` is invoked when the Job
   Router has determined that a job meets the definition for a route.
   This hook is responsible for doing the transformation of the job and
   configuring any resources that are external to HTCondor if
   applicable.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The first line will be the information on route that the job matched
       including the route name. This information will be formatted as a classad.
       If the route has a  ``TargetUniverse`` or :ad-attr:`GridResource` they will be
       included in the classad. The route information classad will be followed
       by a separator line of dashes like ``------`` followed by a newline.
       The remainder of the input will be the job ClassAd.
    Expected standard output from the hook
       The transformed job.
    Exit status of the hook
       0 for success, any non-zero value on failure.

   :index:`Update Job Info<single: Update Job Info; Job Router Hooks>`

-  The hook defined by the configuration variable
   :macro:`<Keyword>_HOOK_UPDATE_JOB_INFO` is invoked to provide
   status on the specified routed job when the Job Router polls the
   status of routed jobs at intervals set by
   :macro:`JOB_ROUTER_POLLING_PERIOD`.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The routed job ClassAd that is to be updated.
    Expected standard output from the hook
       The job attributes to be updated in the routed job, or nothing,
       if there was no update. To prevent clashing with HTCondor's
       management of job attributes, only attributes that are not
       managed by HTCondor should be output from this hook.
    Exit status of the hook
       0 for success, any non-zero value on failure.

   :index:`Job Finalize<single: Job Finalize; Job Router Hooks>`

-  The hook defined by the configuration variable
   :macro:`<Keyword>_HOOK_JOB_FINALIZE` is invoked when the Job
   Router has found that the job has completed. Any output from the hook
   is treated as an update to the source job.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The source job ClassAd, followed by the routed copy Classad that
       completed, separated by the string "------" and a new line.
    Expected standard output from the hook
       An updated source job ClassAd, or nothing if there was no update.
    Exit status of the hook
       0 for success, any non-zero value on failure.

   :index:`Job Cleanup<single: Job Cleanup; Job Router Hooks>`

-  The hook defined by the configuration variable
   :macro:`<Keyword>_HOOK_JOB_CLEANUP` is invoked when the Job
   Router finishes managing the job. This hook will be invoked
   regardless of whether the job completes successfully or not, and must
   exit successfully.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The job ClassAd that the Job Router is done managing.
    Expected standard output from the hook
       None.
    Exit status of the hook
       0 for success, any non-zero value on failure.


