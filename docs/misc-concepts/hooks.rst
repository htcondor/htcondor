      

Hooks
=====

A hook is an external program or script invoked by HTCondor.

Job hooks that fetch work allow sites to write their own programs or
scripts, and allow HTCondor to invoke these hooks at the right moments
to accomplish the desired outcome. This eliminates the expense of the
matchmaking and scheduling provided by the *condor\_schedd* and the
*condor\_negotiator*, although at the price of the flexibility they
offer. Therefore, job hooks that fetch work allow HTCondor to more
easily and directly interface with external scheduling systems.

Hooks may also behave as a Job Router.

The Daemon ClassAd hooks permit the *condor\_startd* and the
*condor\_schedd* daemons to execute hooks once or on a periodic basis.

Note that standard universe jobs execute different *condor\_starter* and
*condor\_shadow* daemons that do not implement any hook mechanisms.

Job Hooks That Fetch Work
-------------------------

In the past, HTCondor has always sent work to the execute machines by
pushing jobs to the *condor\_startd* daemon, either from the
*condor\_schedd* daemon or via *condor\_cod*. Beginning with the
HTCondor version 7.1.0, the *condor\_startd* daemon now has the ability
to pull work by fetching jobs via a system of plug-ins or hooks. Any
site can configure a set of hooks to fetch work, completely outside of
the usual HTCondor matchmaking system.

A projected use of the hook mechanism implements what might be termed a
glide-in factory, especially where the factory is behind a firewall.
Without using the hook mechanism to fetch work, a glide-in
*condor\_startd* daemon behind a firewall depends on CCB to help it
listen and eventually receive work pushed from elsewhere. With the hook
mechanism, a glide-in *condor\_startd* daemon behind a firewall uses the
hook to pull work. The hook needs only an outbound network connection to
complete its task, thereby being able to operate from behind the
firewall, without the intervention of CCB.

Periodically, each execution slot managed by a *condor\_startd* will
invoke a hook to see if there is any work that can be fetched. Whenever
this hook returns a valid job, the *condor\_startd* will evaluate the
current state of the slot and decide if it should start executing the
fetched work. If the slot is unclaimed and the ``Start`` expression
evaluates to ``True``, a new claim will be created for the fetched job.
If the slot is claimed, the *condor\_startd* will evaluate the ``Rank``
expression relative to the fetched job, compare it to the value of
``Rank`` for the currently running job, and decide if the existing job
should be preempted due to the fetched job having a higher rank. If the
slot is unavailable for whatever reason, the *condor\_startd* will
refuse the fetched job and ignore it. Either way, once the
*condor\_startd* decides what it should do with the fetched job, it will
invoke another hook to reply to the attempt to fetch work, so that the
external system knows what happened to that work unit.

If the job is accepted, a claim is created for it and the slot moves
into the Claimed state. As soon as this happens, the *condor\_startd*
will spawn a *condor\_starter* to manage the execution of the job. At
this point, from the perspective of the *condor\_startd*, this claim is
just like any other. The usual policy expressions are evaluated, and if
the job needs to be suspended or evicted, it will be. If a higher-ranked
job being managed by a *condor\_schedd* is matched with the slot, that
job will preempt the fetched work.

The *condor\_starter* itself can optionally invoke additional hooks to
help manage the execution of the specific job. There are hooks to
prepare the execution environment for the job, periodically update
information about the job as it runs, notify when the job exits, and to
take special actions when the job is being evicted.

Assuming there are no interruptions, the job completes, and the
*condor\_starter* exits, the *condor\_startd* will invoke the hook to
fetch work again. If another job is available, the existing claim will
be reused and a new *condor\_starter* is spawned. If the hook returns
that there is no more work to perform, the claim will be evicted, and
the slot will return to the Owner state.

Work Fetching Hooks Invoked by HTCondor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are a handful of hooks invoked by HTCondor related to fetching
work, some of which are called by the *condor\_startd* and others by the
*condor\_starter*. Each hook is described, including when it is invoked,
what task it is supposed to accomplish, what data is passed to the hook,
what output is expected, and, when relevant, the exit status expected.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_FETCH_WORK`` is invoked whenever the
   *condor\_startd* wants to see if there is any work to fetch. There is
   a related configuration variable called ``FetchWorkDelay`` which
   determines how long the *condor\_startd* will wait between attempts
   to fetch work, which is described in detail in within
   section \ `4.4.1 <#x51-4410004.4.1>`__ on
   page \ `1371 <#x51-4410004.4.1>`__. ``<Keyword>_HOOK_FETCH_WORK`` is
   the most important hook in the whole system, and is the only hook
   that must be defined for any of the other *condor\_startd* hooks to
   operate.

   The job ClassAd returned by the hook needs to contain enough
   information for the *condor\_starter* to eventually spawn the work.
   The required and optional attributes in this ClassAd are identical to
   the ones described for Computing on Demand (COD) jobs in
   section \ `4.3.3 <ComputingOnDemandCOD.html#x50-4240004.3.3>`__ on
   COD Application Attributes,
   page \ `1336 <ComputingOnDemandCOD.html#x50-4240004.3.3>`__.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       ClassAd of the slot that is looking for work.
    Expected standard output from the hook
       ClassAd of a job that can be run. If there is no work, the hook
       should return no output.
    User id that the hook runs as
       The ``<Keyword>_HOOK_FETCH_WORK`` hook runs with the same
       privileges as the *condor\_startd*. When Condor was started as
       root, this is usually the condor user, or the user specified in
       the ``CONDOR_IDS`` configuration variable.
    Exit status of the hook
       Ignored.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_REPLY_FETCH`` is invoked whenever
   ``<Keyword>_HOOK_FETCH_WORK`` returns data and the *condor\_startd*
   decides if it is going to accept the fetched job or not.

   The *condor\_startd* will not wait for this hook to return before
   taking other actions, and it ignores all output. The hook is simply
   advisory, and it has no impact on the behavior of the
   *condor\_startd*.

    Command-line arguments passed to the hook
       Either the string accept or reject.
    Standard input given to the hook
       A copy of the job ClassAd and the slot ClassAd (separated by the
       string ----- and a new line).
    Expected standard output from the hook
       None.
    User id that the hook runs as
       The ``<Keyword>_HOOK_REPLY_FETCH`` hook runs with the same
       privileges as the *condor\_startd*. When Condor was started as
       root, this is usually the condor user, or the user specified in
       the ``CONDOR_IDS`` configuration variable.
    Exit status of the hook
       Ignored.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_EVICT_CLAIM`` is invoked whenever the
   *condor\_startd* needs to evict a claim representing fetched work.

   The *condor\_startd* will not wait for this hook to return before
   taking other actions, and ignores all output. The hook is simply
   advisory, and has no impact on the behavior of the *condor\_startd*.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       A copy of the job ClassAd and the slot ClassAd (separated by the
       string ----- and a new line).
    Expected standard output from the hook
       None.
    User id that the hook runs as
       The ``<Keyword>_HOOK_EVICT_CLAIM`` hook runs with the same
       privileges as the *condor\_startd*. When Condor was started as
       root, this is usually the condor user, or the user specified in
       the ``CONDOR_IDS`` configuration variable.
    Exit status of the hook
       Ignored.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_PREPARE_JOB`` is invoked by the *condor\_starter*
   before a job is going to be run. This hook provides a chance to
   execute commands to set up the job environment, for example, to
   transfer input files.

   The *condor\_starter* waits until this hook returns before attempting
   to execute the job. If the hook returns a non-zero exit status, the
   *condor\_starter* will assume an error was reached while attempting
   to set up the job environment and abort the job.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       A copy of the job ClassAd.
    Expected standard output from the hook
       A set of attributes to insert or update into the job ad. For
       example, changing the ``Cmd`` attribute to a quoted string
       changes the executable to be run.
    User id that the hook runs as
       The ``<Keyword>_HOOK_PREPARE_JOB`` hook runs with the same
       privileges as the job itself. If slot users are defined, the hook
       runs as the slot user, just as the job does.
    Exit status of the hook
       0 for success preparing the job, any non-zero value on failure.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_UPDATE_JOB_INFO`` is invoked periodically during the
   life of the job to update information about the status of the job.
   When the job is first spawned, the *condor\_starter* will invoke this
   hook after ``STARTER_INITIAL_UPDATE_INTERVAL`` seconds (defaults to
   8). Thereafter, the *condor\_starter* will invoke the hook every
   ``STARTER_UPDATE_INTERVAL`` seconds (defaults to 300, which is 5
   minutes).

   The *condor\_starter* will not wait for this hook to return before
   taking other actions, and ignores all output. The hook is simply
   advisory, and has no impact on the behavior of the *condor\_starter*.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       A copy of the job ClassAd that has been augmented with additional
       attributes describing the current status and execution behavior
       of the job.

       The additional attributes included inside the job ClassAd are:

        ``JobState``
           The current state of the job. Can be either ``"Running"`` or
           ``"Suspended"``.
        ``JobPid``
           The process identifier for the initial job directly spawned
           by the *condor\_starter*.
        ``NumPids``
           The number of processes that the job has currently spawned.
        ``JobStartDate``
           The epoch time when the job was first spawned by the
           *condor\_starter*.
        ``RemoteSysCpu``
           The total number of seconds of system CPU time (the time
           spent at system calls) the job has used.
        ``RemoteUserCpu``
           The total number of seconds of user CPU time the job has
           used.
        ``ImageSize``
           The memory image size of the job in Kbytes.

    Expected standard output from the hook
       None.
    User id that the hook runs as
       The ``<Keyword>_HOOK_UPDATE_JOB_INFO`` hook runs with the same
       privileges as the job itself.
    Exit status of the hook
       Ignored.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_JOB_EXIT`` is invoked by the *condor\_starter*
   whenever a job exits, either on its own or when being evicted from an
   execution slot.

   The *condor\_starter* will wait for this hook to return before taking
   any other actions. In the case of jobs that are being managed by a
   *condor\_shadow*, this hook is invoked before the *condor\_starter*
   does its own optional file transfer back to the submission machine,
   writes to the local job event log file, or notifies the
   *condor\_shadow* that the job has exited.

    Command-line arguments passed to the hook
       A string describing how the job exited:

       -  exit The job exited or died with a signal on its own.
       -  remove The job was removed with *condor\_rm* or as the result
          of user job policy expressions (for example,
          ``PeriodicRemove``).
       -  hold The job was held with *condor\_hold* or the user job
          policy expressions (for example, ``PeriodicHold``).
       -  evict The job was evicted from the execution slot for any
          other reason (``PREEMPT`` evaluated to TRUE in the
          *condor\_startd*, *condor\_vacate*, *condor\_off*, etc).

    Standard input given to the hook
       A copy of the job ClassAd that has been augmented with additional
       attributes describing the execution behavior of the job and its
       final results.

       The job ClassAd passed to this hook contains all of the extra
       attributes described above for ``<Keyword>_HOOK_UPDATE_JOB_INFO``
       , and the following additional attributes that are only present
       once a job exits:

        ``ExitReason``
           A human-readable string describing why the job exited.
        ``ExitBySignal``
           A boolean indicating if the job exited due to being killed by
           a signal, or if it exited with an exit status.
        ``ExitSignal``
           If ``ExitBySignal`` is true, the signal number that killed
           the job.
        ``ExitCode``
           If ``ExitBySignal`` is false, the integer exit code of the
           job.
        ``JobDuration``
           The number of seconds that the job ran during this
           invocation.

    Expected standard output from the hook
       None.
    User id that the hook runs as
       The ``<Keyword>_HOOK_JOB_EXIT`` hook runs with the same
       privileges as the job itself.
    Exit status of the hook
       Ignored.

Keywords to Define Job Fetch Hooks in the HTCondor Configuration files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Hooks are defined in the HTCondor configuration files by prefixing the
name of the hook with a keyword. This way, a given machine can have
multiple sets of hooks, each set identified by a specific keyword.

Each slot on the machine can define a separate keyword for the set of
hooks that should be used with ``SLOT<N>_JOB_HOOK_KEYWORD`` . For
example, on slot 1, the variable name will be called
``SLOT1_JOB_HOOK_KEYWORD``. If the slot-specific keyword is not defined,
the *condor\_startd* will use a global keyword as defined by
``STARTD_JOB_HOOK_KEYWORD`` .

Once a job is fetched via ``<Keyword>_HOOK_FETCH_WORK`` , the
*condor\_startd* will insert the keyword used to fetch that job into the
job ClassAd as ``HookKeyword``. This way, the same keyword will be used
to select the hooks invoked by the *condor\_starter* during the actual
execution of the job. However, the ``STARTER_JOB_HOOK_KEYWORD`` can be
defined to force the *condor\_starter* to always use a given keyword for
its own hooks, instead of looking the job ClassAd for a ``HookKeyword``
attribute.

For example, the following configuration defines two sets of hooks, and
on a machine with 4 slots, 3 of the slots use the global keyword for
running work from a database-driven system, and one of the slots uses a
custom keyword to handle work fetched from a web service.

::

      # Most slots fetch and run work from the database system. 
      STARTD_JOB_HOOK_KEYWORD = DATABASE 
     
      # Slot4 fetches and runs work from a web service. 
      SLOT4_JOB_HOOK_KEYWORD = WEB 
     
      # The database system needs to both provide work and know the reply 
      # for each attempted claim. 
      DATABASE_HOOK_DIR = /usr/local/condor/fetch/database 
      DATABASE_HOOK_FETCH_WORK = $(DATABASE_HOOK_DIR)/fetch_work.php 
      DATABASE_HOOK_REPLY_FETCH = $(DATABASE_HOOK_DIR)/reply_fetch.php 
     
      # The web system only needs to fetch work. 
      WEB_HOOK_DIR = /usr/local/condor/fetch/web 
      WEB_HOOK_FETCH_WORK = $(WEB_HOOK_DIR)/fetch_work.php

The keywords ``"DATABASE"`` and ``"WEB"`` are completely arbitrary, so
each site is encouraged to use different (more specific) names as
appropriate for their own needs.

Defining the FetchWorkDelay Expression
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are two events that trigger the *condor\_startd* to attempt to
fetch new work:

-  the *condor\_startd* evaluates its own state
-  the *condor\_starter* exits after completing some fetched work

Even if a given compute slot is already busy running other work, it is
possible that if it fetched new work, the *condor\_startd* would prefer
this newly fetched work (via the ``Rank`` expression) over the work it
is currently running. However, the *condor\_startd* frequently evaluates
its own state, especially when a slot is claimed. Therefore,
administrators can define a configuration variable which controls how
long the *condor\_startd* will wait between attempts to fetch new work.
This variable is called ``FetchWorkDelay`` .

The ``FetchWorkDelay`` expression must evaluate to an integer, which
defines the number of seconds since the last fetch attempt completed
before the *condor\_startd* will attempt to fetch more work. However, as
a ClassAd expression (evaluated in the context of the ClassAd of the
slot considering if it should fetch more work, and the ClassAd of the
currently running job, if any), the length of the delay can be based on
the current state the slot and even the currently running job.

For example, a common configuration would be to always wait 5 minutes
(300 seconds) between attempts to fetch work, unless the slot is
Claimed/Idle, in which case the *condor\_startd* should fetch
immediately:

::

    FetchWorkDelay = ifThenElse(State == "Claimed" && Activity == "Idle", 0, 300)

If the *condor\_startd* wants to fetch work, but the time since the last
attempted fetch is shorter than the current value of the delay
expression, the *condor\_startd* will set a timer to fetch as soon as
the delay expires.

If this expression is not defined, the *condor\_startd* will default to
a five minute (300 second) delay between all attempts to fetch work.

Example Hook: Specifying the Executable at Execution Time
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The availability of multiple versions of an application leads to the
need to specify one of the versions. As an example, consider that the
java universe utilizes a single, fixed JVM. There may be multiple JVMs
available, and the HTCondor job may need to make the choice of JVM
version. The use of a job hook solves this problem. The job does not use
the java universe, and instead uses the vanilla universe in combination
with a prepare job hook to overwrite the ``Cmd`` attribute of the job
ClassAd. This attribute is the name of the executable the
*condor\_starter* daemon will invoke, thereby selecting the specific JVM
installation.

In the configuration of the execute machine:

::

    JAVA5_HOOK_PREPARE_JOB = $(LIBEXEC)/java5_prepare_hook

With this configuration, a job that sets the ``HookKeyword`` attribute
with

::

    +HookKeyword = "JAVA5"

in the submit description file causes the *condor\_starter* will run the
hook specified by ``JAVA5_HOOK_PREPARE_JOB`` before running this job.
Note that the double quote marks are required to correctly define the
attribute. Any output from this hook is an update to the job ClassAd.
Therefore, the hook that changes the executable may be

::

    #!/bin/sh 
     
    # Read and discard the job ClassAd 
    cat > /dev/null 
    echo 'Cmd = "/usr/java/java5/bin/java"'

If some machines in your pool have this hook and others do not, this
fact should be advertised. Add to the configuration of every execute
machine that has the hook:

::

    HasJava5PrepareHook = True 
    STARTD_ATTRS = HasJava5PrepareHook $(STARTD_ATTRS)

The submit description file for this example job may be

::

    universe = vanilla 
    executable = /usr/bin/java 
    arguments = Hello 
    # match with a machine that has the hook 
    requirements = HasJava5PrepareHook 
     
    should_transfer_files = always 
    when_to_transfer_output = on_exit 
    transfer_input_files = Hello.class 
     
    output = hello.out 
    error  = hello.err 
    log    = hello.log 
     
    +HookKeyword="JAVA5" 
    queue 

Note that the **requirements** command ensures that this job matches
with a machine that has ``JAVA5_HOOK_PREPARE_JOB`` defined.

Hooks for a Job Router
----------------------

Job Router Hooks allow for an alternate transformation and/or monitoring
than the *condor\_job\_router* daemon implements. Routing is still
managed by the *condor\_job\_router* daemon, but if the Job Router Hooks
are specified, then these hooks will be used to transform and monitor
the job instead.

Job Router Hooks are similar in concept to Fetch Work Hooks, but they
are limited in their scope. A hook is an external program or script
invoked by the *condor\_job\_router* daemon at various points during the
life cycle of a routed job.

The following sections describe how and when these hooks are used, what
hooks are invoked at various stages of the job’s life, and how to
configure HTCondor to use these Hooks.

Hooks Invoked for Job Routing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Job Router Hooks allow for replacement of the transformation engine
used by HTCondor for routing a job. Since the external transformation
engine is not controlled by HTCondor, additional hooks provide a means
to update the job’s status in HTCondor, and to clean up upon exit or
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

There are 2 ways in which the Job Router Hooks may be enabled. A job’s
submit description file may cause the hooks to be invoked with

::

      +HookKeyword = "HOOKNAME"

Adding this attribute to the job’s ClassAd causes the
*condor\_job\_router* daemon on the submit machine to invoke hooks
prefixed with the defined keyword. ``HOOKNAME`` is a string chosen as an
example; any string may be used.

The job’s ClassAd attribute definition of ``HookKeyword`` takes
precedence, but if not present, hooks may be enabled by defining on the
submit machine the configuration variable

::

     JOB_ROUTER_HOOK_KEYWORD = HOOKNAME

Like the example attribute above, ``HOOKNAME`` represents a chosen name
for the hook, replaced as desired or appropriate.

There are 4 hooks that the Job Router can be configured to use. Each
hook will be described below along with data passed to the hook and
expected output. All hooks must exit successfully.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_TRANSLATE_JOB`` is invoked when the Job Router has
   determined that a job meets the definition for a route. This hook is
   responsible for doing the transformation of the job and configuring
   any resources that are external to HTCondor if applicable.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The first line will be the route that the job matched as defined
       in HTCondor’s configuration files followed by the job ClassAd,
       separated by the string "------" and a new line.
    Expected standard output from the hook
       The transformed job.
    Exit status of the hook
       0 for success, any non-zero value on failure.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_UPDATE_JOB_INFO`` is invoked to provide status on
   the specified routed job when the Job Router polls the status of
   routed jobs at intervals set by ``JOB_ROUTER_POLLING_PERIOD`` .

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The routed job ClassAd that is to be updated.
    Expected standard output from the hook
       The job attributes to be updated in the routed job, or nothing,
       if there was no update. To prevent clashing with HTCondor’s
       management of job attributes, only attributes that are not
       managed by HTCondor should be output from this hook.
    Exit status of the hook
       0 for success, any non-zero value on failure.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_JOB_FINALIZE`` is invoked when the Job Router has
   found that the job has completed. Any output from the hook is treated
   as an update to the source job.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The source job ClassAd, followed by the routed copy Classad that
       completed, separated by the string "------" and a new line.
    Expected standard output from the hook
       An updated source job ClassAd, or nothing if there was no update.
    Exit status of the hook
       0 for success, any non-zero value on failure.

-  The hook defined by the configuration variable
   ``<Keyword>_HOOK_JOB_CLEANUP`` is invoked when the Job Router
   finishes managing the job. This hook will be invoked regardless of
   whether the job completes successfully or not, and must exit
   successfully.

    Command-line arguments passed to the hook
       None.
    Standard input given to the hook
       The job ClassAd that the Job Router is done managing.
    Expected standard output from the hook
       None.
    Exit status of the hook
       0 for success, any non-zero value on failure.

Daemon ClassAd Hooks
--------------------

 Overview

The *Daemon ClassAd Hook* mechanism is used to run executables (called
jobs) directly from the *condor\_startd* and *condor\_schedd* daemons.
The output from these jobs is incorporated into the machine ClassAd
generated by the respective daemon. This mechanism and associated jobs
have been identified by various names, including the Startd Cron,
dynamic attributes, and a distribution of executables collectively known
as Hawkeye.

Pool management tasks can be enhanced by using a daemon’s ability to
periodically run executables. The executables are expected to generate
ClassAd attributes as their output; these ClassAds are then incorporated
into the machine ClassAd. Policy expressions can then reference dynamic
attributes (created by the ClassAd hook jobs) in the machine ClassAd.

 Job output

The output of the job is incorporated into one or more ClassAds when the
job exits. When the job outputs the special line:

::

      - update:true

the output of the job is merged into all proper ClassAds, and an update
goes to the *condor\_collector* daemon.

As of version 8.3.0, it is possible for a Startd Cron job (but not a
Schedd Cron job) to define multiple ClassAds, using the mechanism
defined below:

-  An output line starting with ``’-’`` has always indicated
   end-of-ClassAd. The ``’-’`` can now be followed by a uniqueness tag
   to indicate the name of the ad that should be replaced by the new ad.
   This name is joined to the name of the Startd Cron job to produced a
   full name for the ad. This allows a single Startd Cron job to return
   multiple ads by giving each a unique name, and to replace multiple
   ads by using the same unique name as a previous invocation. The
   optional uniqueness tag can also be followed by the optional keyword
   ``update:<bool>``, which can be used to override the Startd Cron
   configuration and suppress or force immediate updates.

   In other words, the syntax is:

   - [*name*\ ] [**update:  **\ *bool*]

-  Each ad can contain one of four possible attributes to control what
   slot ads the ad is merged into when the *condor\_startd* sends
   updates to the collector. These attributes are, in order of highest
   to lower priority (in other words, if ``SlotMergeConstraint``
   matches, the other attributes are not considered, and so on):

   -  **SlotMergeConstraint **\ *expression*: the current ad is merged
      into all slot ads for which this expression is true. The
      expression is evaluated with the slot ad as the TARGET ad.
   -  **SlotName\|Name **\ *string*: the current ad is merged into all
      slots whose ``Name`` attributes match the value of ``SlotName`` up
      to the length of ``SlotName``.
   -  **SlotTypeId **\ *integer*: the current ad is merged into all ads
      that have the same value for their ``SlotTypeId`` attribute.
   -  **SlotId **\ *integer*: the current ad is merged into all ads that
      have the same value for their ``SlotId`` attribute.

For example, if the Startd Cron job returns:

::

      Value=1 
      SlotId=1 
      -s1 
      Value=2 
      SlotId=2 
      -s2 
      Value=10 
      - update:true

it will set ``Value=10`` for all slots except slot1 and slot2. On those
slots it will set ``Value=1`` and ``Value=2`` respectively. It will also
send updates to the collector immediately.

 Configuration

Configuration variables related to Daemon ClassAd Hooks are defined in
section  `3.5.31 <ConfigurationMacros.html#x33-2270003.5.31>`__.

Here is a complete configuration example. It defines all three of the
available types of jobs: ones that use the *condor\_startd*, benchmark
jobs, and ones that use the *condor\_schedd*.

::

    # 
    # Startd Cron Stuff 
    # 
    # auxiliary variable to use in identifying locations of files 
    MODULES = $(ROOT)/modules 
     
    STARTD_CRON_CONFIG_VAL = $(RELEASE_DIR)/bin/condor_config_val 
    STARTD_CRON_MAX_JOB_LOAD = 0.2 
    STARTD_CRON_JOBLIST = 
     
    # Test job 
    STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) test 
    STARTD_CRON_TEST_MODE = OneShot 
    STARTD_CRON_TEST_RECONFIG_RERUN = True 
    STARTD_CRON_TEST_PREFIX = test_ 
    STARTD_CRON_TEST_EXECUTABLE = $(MODULES)/test 
    STARTD_CRON_TEST_KILL = True 
    STARTD_CRON_TEST_ARGS = abc 123 
    STARTD_CRON_TEST_SLOTS = 1 
    STARTD_CRON_TEST_JOB_LOAD = 0.01 
     
    # job 'date' 
    STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) date 
    STARTD_CRON_DATE_MODE = Periodic 
    STARTD_CRON_DATE_EXECUTABLE = $(MODULES)/date 
    STARTD_CRON_DATE_PERIOD = 15s 
    STARTD_CRON_DATE_JOB_LOAD = 0.01 
     
    # Job 'foo' 
    STARTD_CRON_JOBLIST = $(STARTD_CRON_JOBLIST) foo 
    STARTD_CRON_FOO_EXECUTABLE = $(MODULES)/foo 
    STARTD_CRON_FOO_PREFIX = Foo 
    STARTD_CRON_FOO_MODE = Periodic 
    STARTD_CRON_FOO_PERIOD = 10m 
    STARTD_CRON_FOO_JOB_LOAD = 0.2 
     
    # 
    # Benchmark Stuff 
    # 
    BENCHMARKS_JOBLIST = mips kflops 
     
    # MIPS benchmark 
    BENCHMARKS_MIPS_EXECUTABLE = $(LIBEXEC)/condor_mips 
    BENCHMARKS_MIPS_JOB_LOAD = 1.0 
     
    # KFLOPS benchmark 
    BENCHMARKS_KFLOPS_EXECUTABLE = $(LIBEXEC)/condor_kflops 
    BENCHMARKS_KFLOPS_JOB_LOAD = 1.0 
     
    # 
    # Schedd Cron Stuff 
    # 
    SCHEDD_CRON_CONFIG_VAL = $(RELEASE_DIR)/bin/condor_config_val 
    SCHEDD_CRON_JOBLIST = 
     
    # Test job 
    SCHEDD_CRON_JOBLIST = $(SCHEDD_CRON_JOBLIST) test 
    SCHEDD_CRON_TEST_MODE = OneShot 
    SCHEDD_CRON_TEST_RECONFIG_RERUN = True 
    SCHEDD_CRON_TEST_PREFIX = test_ 
    SCHEDD_CRON_TEST_EXECUTABLE = $(MODULES)/test 
    SCHEDD_CRON_TEST_PERIOD = 5m 
    SCHEDD_CRON_TEST_KILL = True 
    SCHEDD_CRON_TEST_ARGS = abc 123 

      
