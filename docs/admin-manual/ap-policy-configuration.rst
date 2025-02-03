Configuration for Access Points
===============================

:index:`condor_schedd policy<single: condor_schedd policy; configuration>`
:index:`policy configuration<single: policy configuration; submit host>`

Schedd Job Transforms
---------------------

:index:`job transforms`

The *condor_schedd* can transform jobs as they are submitted.
Transformations can be used to guarantee the presence of required job
attributes, to set defaults for job attributes the user does not supply,
or to modify job attributes so that they conform to schedd policy; an
example of this might be to automatically set accounting attributes
based on the owner of the job while letting the job owner indicate a
preference.

There can be multiple job transforms. Each transform can have a
Requirements expression to indicate which jobs it should transform and
which it should ignore. Transforms without a Requirements expression
apply to all jobs. Job transforms are applied in order. The set of
transforms and their order are configured using the Configuration
variable :macro:`JOB_TRANSFORM_NAMES`.

For each entry in this list there must be a corresponding
:macro:`JOB_TRANSFORM_<name>`
configuration variable that specifies the transform rules. Transforms
can use the same syntax as *condor_job_router* transforms; although unlike
the *condor_job_router* there is no default transform, and all
matching transforms are applied - not just the first one. (See the
:doc:`/grid-computing/job-router` section for information on the
*condor_job_router*.)

When a submission is a late materialization job factory,
transforms that would match the first factory job will be applied to the Cluster ad at submit time.
When job ads are later materialized, attribute values set by the transform
will override values set by the job factory for those attributes.

The following example shows a set of two transforms: one that
automatically assigns an accounting group to jobs based on the
submitting user, and one that shows one possible way to transform
Vanilla jobs to Docker jobs.

.. code-block:: text

    JOB_TRANSFORM_NAMES = AssignGroup, SL6ToDocker

    JOB_TRANSFORM_AssignGroup @=end
       # map Owner to group using the existing accounting group attribute as requested group
       EVALSET AcctGroup = userMap("Groups",Owner,AcctGroup)
       EVALSET AccountingGroup = join(".",AcctGroup,Owner)
    @end

    JOB_TRANSFORM_SL6ToDocker @=end
       # match only vanilla jobs that have WantSL6 and do not already have a DockerImage
       REQUIREMENTS JobUniverse==5 && WantSL6 && DockerImage =?= undefined
       SET  WantDocker = true
       SET  DockerImage = "SL6"
       SET  Requirements = TARGET.HasDocker && $(MY.Requirements)
    @end

The AssignGroup transform above assumes that a mapfile that can map an
owner to one or more accounting groups has been configured via
:macro:`SCHEDD_CLASSAD_USER_MAP_NAMES`, and given the name "Groups".

The SL6ToDocker transform above is most likely incomplete, as it assumes
a custom attribute (``WantSL6``) that your pool may or may not use.

Submit Requirements
-------------------

:index:`submit requirements`

The *condor_schedd* may reject job submissions, such that rejected jobs
never enter the queue. Rejection may be best for the case in which there
are jobs that will never be able to run; for instance, a job specifying
an obsolete universe, like standard.
Another appropriate example might be to reject all jobs that
do not request a minimum amount of memory. Or, it may be appropriate to
prevent certain users from using a specific submit host.

Rejection criteria are configured. Configuration variable
:macro:`SUBMIT_REQUIREMENT_NAMES`
lists criteria, where each criterion is given a name. The chosen name is
a major component of the default error message output if a user attempts
to submit a job which fails to meet the requirements. Therefore, choose
a descriptive name. For the three example submit requirements described:

.. code-block:: text

    SUBMIT_REQUIREMENT_NAMES = NotStandardUniverse, MinimalRequestMemory, NotChris

The criterion for each submit requirement is then specified in
configuration variable 
:macro:`SUBMIT_REQUIREMENT_<Name>`, where ``<Name>`` matches the
chosen name listed in :macro:`SUBMIT_REQUIREMENT_NAMES`. The value is a
boolean ClassAd expression. The three example criterion result in these
configuration variable definitions:

.. code-block:: text

    SUBMIT_REQUIREMENT_NotStandardUniverse = JobUniverse != 1
    SUBMIT_REQUIREMENT_MinimalRequestMemory = RequestMemory > 512
    SUBMIT_REQUIREMENT_NotChris = Owner != "chris"

Submit requirements are evaluated in the listed order; the first
requirement that evaluates to ``False`` causes rejection of the job,
terminates further evaluation of other submit requirements, and is the
only requirement reported. Each submit requirement is evaluated in the
context of the *condor_schedd* ClassAd, which is the ``MY.`` name space
and the job ClassAd, which is the ``TARGET.`` name space. Note that
:ad-attr:`JobUniverse` and :ad-attr:`RequestMemory` are both job ClassAd attributes.

Further configuration may associate a rejection reason with a submit
requirement with the :macro:`SUBMIT_REQUIREMENT_<Name>_REASON`.

.. code-block:: text

    SUBMIT_REQUIREMENT_NotStandardUniverse_REASON = "This pool does not accept standard universe jobs."
    SUBMIT_REQUIREMENT_MinimalRequestMemory_REASON = strcat( "The job only requested ", \
      RequestMemory, " Megabytes.  If that small amount is really enough, please contact ..." )
    SUBMIT_REQUIREMENT_NotChris_REASON = "Chris, you may only submit jobs to the instructional pool."

The value must be a ClassAd expression which evaluates to a string.
Thus, double quotes were required to make strings for both
``SUBMIT_REQUIREMENT_NotStandardUniverse_REASON`` and
``SUBMIT_REQUIREMENT_NotChris_REASON``. The ClassAd function strcat()
produces a string in the definition of
``SUBMIT_REQUIREMENT_MinimalRequestMemory_REASON``.

Rejection reasons are sent back to the submitting program and will
typically be immediately presented to the user. If an optional
:macro:`SUBMIT_REQUIREMENT_<Name>_REASON` is not defined, a default reason
will include the ``<Name>`` chosen for the submit requirement.
Completing the presentation of the example submit requirements, upon an
attempt to submit a standard universe job, :tool:`condor_submit` would print

.. code-block:: text

    Submitting job(s).
    ERROR: Failed to commit job submission into the queue.
    ERROR: This pool does not accept standard universe jobs.

Where there are multiple jobs in a cluster, if any job within the
cluster is rejected due to a submit requirement, the entire cluster of
jobs will be rejected.

Submit Warnings
---------------

:index:`submit warnings`

Starting in HTCondor 8.7.4, you may instead configure submit warnings. A
submit warning is a submit requirement for which
:macro:`SUBMIT_REQUIREMENT_<Name>_IS_WARNING` is true. A submit
warning does not cause the submission to fail; instead, it returns a
warning to the user's console (when triggered via :tool:`condor_submit`) or
writes a message to the user log (always). Submit warnings are intended
to allow HTCondor administrators to provide their users with advance
warning of new submit requirements. For example, if you want to increase
the minimum request memory, you could use the following configuration.

.. code-block:: text

    SUBMIT_REQUIREMENT_NAMES = OneGig $(SUBMIT_REQUIREMENT_NAMES)
    SUBMIT_REQUIREMENT_OneGig = RequestMemory > 1024
    SUBMIT_REQUIREMENT_OneGig_REASON = "As of <date>, the minimum requested memory will be 1024."
    SUBMIT_REQUIREMENT_OneGig_IS_WARNING = TRUE

When a user runs :tool:`condor_submit` to submit a job with :ad-attr:`RequestMemory`
between 512 and 1024, they will see (something like) the following,
assuming that the job meets all the other requirements.

.. code-block:: text

    Submitting job(s).
    WARNING: Committed job submission into the queue with the following warning:
    WARNING: As of <date>, the minimum requested memory will be 1024.

    1 job(s) submitted to cluster 452.

The job will contain (something like) the following:

.. code-block:: text

    000 (452.000.000) 10/06 13:40:45 Job submitted from host: <128.105.136.53:37317?addrs=128.105.136.53-37317+[fc00--1]-37317&noUDP&sock=19966_e869_5>
        WARNING: Committed job submission into the queue with the following warning: As of <date>, the minimum requested memory will be 1024.
    ...

Marking a submit requirement as a warning does not change when or how it
is evaluated, only the result of doing so. In particular, failing a
submit warning does not terminate further evaluation of the submit
requirements list. Currently, only one (the most recent) problem is
reported for each submit attempt. This means users will see (as they
previously did) only the first failed requirement; if all requirements
passed, they will see the last failed warning, if any.

Working with Remote Job Submission
''''''''''''''''''''''''''''''''''

:index:`of job queue, with remote job submission<single: of job queue, with remote job submission; High Availability>`

Remote job submission requires identification of the job queue,
submitting with a command similar to:

.. code-block:: console

      $ condor_submit -remote condor@example.com myjob.submit

This implies the identification of a single *condor_schedd* daemon,
running on a single machine. With the high availability of the job
queue, there are multiple *condor_schedd* daemons, of which only one at
a time is acting as the single submission point. To make remote
submission of jobs work properly, set the configuration variable
:macro:`SCHEDD_NAME` in the local configuration to
have the same value for each potentially running *condor_schedd*
daemon. In addition, the value chosen for the variable :macro:`SCHEDD_NAME`
will need to include the at symbol (@), such that HTCondor will not
modify the value set for this variable. See the description of
:macro:`MASTER_NAME` in the :ref:`admin-manual/configuration-macros:condor_master
configuration file macros` section for defaults and composition of valid values
for :macro:`SCHEDD_NAME`. As an example, include in each local configuration a value
similar to:

.. code-block:: condor-config

    SCHEDD_NAME = had-schedd@

Then, with this sample configuration, the submit command appears as:

.. code-block:: console

      $ condor_submit -remote had-schedd@  myjob.submit

Schedd Cron
-----------

:index:`Schedd Cron`

Just as an administrator can dynamically add new classad attributes
and values programmatically with script to the startd's ads, the
same can be done with the classads the *condor_schedd* sends to the
collector.  However, these are less generally useful, as there is
no matchmaking with the schedd ads.  Administrators might want to 
use this to advertise some performance or resource usage of
the machine the schedd is running on for further monitoring.

See the section in :ref:`admin-manual/ep-policy-configuration:Startd Cron`
for examples and information about this.

Dedicated Scheduling
--------------------

:index:`dedicated scheduling`
:index:`under the dedicated scheduler<single: under the dedicated scheduler; MPI application>`

The dedicated scheduler is a part of the *condor_schedd* that handles
the scheduling of parallel jobs that require more than one machine
concurrently running per job. MPI applications are a common use for the
dedicated scheduler, but parallel applications which do not require MPI
can also be run with the dedicated scheduler. All jobs which use the
parallel universe are routed to the dedicated scheduler within the
*condor_schedd* they were submitted to. A default HTCondor installation
does not configure a dedicated scheduler; the administrator must
designate one or more *condor_schedd* daemons to perform as dedicated
scheduler.

Selecting and Setting Up a Dedicated Scheduler
''''''''''''''''''''''''''''''''''''''''''''''

We recommend that you select a single machine within an HTCondor pool to
act as the dedicated scheduler. This becomes the machine from upon which
all users submit their parallel universe jobs. The perfect choice for
the dedicated scheduler is the single, front-end machine for a dedicated
cluster of compute nodes. For the pool without an obvious choice for a
access point, choose a machine that all users can log into, as well as
one that is likely to be up and running all the time. All of HTCondor's
other resource requirements for a access point apply to this machine,
such as having enough disk space in the spool directory to hold jobs.
See :ref:`admin-manual/logging:directories` for more information.

Configuration Examples for Dedicated Resources
''''''''''''''''''''''''''''''''''''''''''''''

Each execute machine may have its own policy for the execution of jobs,
as set by configuration. Each machine with aspects of its configuration
that are dedicated identifies the dedicated scheduler. And, the ClassAd
representing a job to be executed on one or more of these dedicated
machines includes an identifying attribute. An example configuration
file with the following various policy settings is
``/etc/examples/condor_config.local.dedicated.resource``.

Each execute machine defines the configuration variable
:macro:`DedicatedScheduler`, which identifies the dedicated scheduler it is
managed by. The local configuration file contains a modified form of

.. code-block:: text

    DedicatedScheduler = "DedicatedScheduler@full.host.name"
    STARTD_ATTRS = $(STARTD_ATTRS), DedicatedScheduler

Substitute the host name of the dedicated scheduler machine for the
string "full.host.name".

If running personal HTCondor, the name of the scheduler includes the
user name it was started as, so the configuration appears as:

.. code-block:: text

    DedicatedScheduler = "DedicatedScheduler@username@full.host.name"
    STARTD_ATTRS = $(STARTD_ATTRS), DedicatedScheduler

All dedicated execute machines must have policy expressions which allow
for jobs to always run, but not be preempted. The resource must also be
configured to prefer jobs from the dedicated scheduler over all other
jobs. Therefore, configuration gives the dedicated scheduler of choice
the highest rank. It is worth noting that HTCondor puts no other
requirements on a resource for it to be considered dedicated.

Job ClassAds from the dedicated scheduler contain the attribute
``Scheduler``. The attribute is defined by a string of the form

.. code-block:: text

    Scheduler = "DedicatedScheduler@full.host.name"

The host name of the dedicated scheduler substitutes for the string
full.host.name.

Different resources in the pool may have different dedicated policies by
varying the local configuration.

Policy Scenario: Machine Runs Only Jobs That Require Dedicated Resources
    One possible scenario for the use of a dedicated resource is to only
    run jobs that require the dedicated resource. To enact this policy,
    configure the following expressions:

    .. code-block:: text

        START     = Scheduler =?= $(DedicatedScheduler)
        SUSPEND   = False
        CONTINUE  = True
        PREEMPT   = False
        KILL      = False
        WANT_SUSPEND   = False
        WANT_VACATE    = False
        RANK      = Scheduler =?= $(DedicatedScheduler)

    The :macro:`START` expression specifies that a job
    with the ``Scheduler`` attribute must match the string corresponding
    :macro:`DedicatedScheduler` attribute in the machine ClassAd. The
    :macro:`RANK` expression specifies that this same job
    (with the ``Scheduler`` attribute) has the highest rank. This
    prevents other jobs from preempting it based on user priorities. The
    rest of the expressions disable any other of the *condor_startd*
    daemon's pool-wide policies, such as those for evicting jobs when
    keyboard and CPU activity is discovered on the machine.

Policy Scenario: Run Both Jobs That Do and Do Not Require Dedicated Resources
    While the first example works nicely for jobs requiring dedicated
    resources, it can lead to poor utilization of the dedicated
    machines. A more sophisticated strategy allows the machines to run
    other jobs, when no jobs that require dedicated resources exist. The
    machine is configured to prefer jobs that require dedicated
    resources, but not prevent others from running.

    To implement this, configure the machine as a dedicated resource as
    above, modifying only the :macro:`START` expression:

    .. code-block:: text

        START = True

Policy Scenario: Adding Desktop Resources To The Mix
    A third policy example allows all jobs. These desktop machines use a
    preexisting :macro:`START` expression that takes the machine owner's
    usage into account for some jobs. The machine does not preempt jobs
    that must run on dedicated resources, while it may preempt other
    jobs as defined by policy. So, the default pool policy is used for
    starting and stopping jobs, while jobs that require a dedicated
    resource always start and are not preempted.

    The :macro:`START`, :macro:`SUSPEND`, :macro:`PREEMPT`, and :macro:`RANK` policies are
    set in the global configuration. Locally, the configuration is
    modified to this hybrid policy by adding a second case.

    .. code-block:: text

        SUSPEND    = Scheduler =!= $(DedicatedScheduler) && ($(SUSPEND))
        PREEMPT    = Scheduler =!= $(DedicatedScheduler) && ($(PREEMPT))
        RANK_FACTOR    = 1000000
        RANK   = (Scheduler =?= $(DedicatedScheduler) * $(RANK_FACTOR)) \
                       + $(RANK)
        START  = (Scheduler =?= $(DedicatedScheduler)) || ($(START))

    Define ``RANK_FACTOR`` to be a larger
    value than the maximum value possible for the existing rank
    expression. :macro:`RANK` is a floating point value,
    so there is no harm in assigning a very large value.

Preemption with Dedicated Jobs
''''''''''''''''''''''''''''''

The dedicated scheduler can be configured to preempt running parallel
universe jobs in favor of higher priority parallel universe jobs. Note
that this is different from preemption in other universes, and parallel
universe jobs cannot be preempted either by a machine's user pressing a
key or by other means.

By default, the dedicated scheduler will never preempt running parallel
universe jobs. Two configuration variables control preemption of these
dedicated resources: :macro:`SCHEDD_PREEMPTION_REQUIREMENTS` and
:macro:`SCHEDD_PREEMPTION_RANK`. These
variables have no default value, so if either are not defined,
preemption will never occur. :macro:`SCHEDD_PREEMPTION_REQUIREMENTS` must
evaluate to ``True`` for a machine to be a candidate for this kind of
preemption. If more machines are candidates for preemption than needed
to satisfy a higher priority job, the machines are sorted by
:macro:`SCHEDD_PREEMPTION_RANK`, and only the highest ranked machines are
taken.

Note that preempting one node of a running parallel universe job
requires killing the entire job on all of its nodes. So, when preemption
occurs, it may end up freeing more machines than are needed for the new
job. Also, preempted jobs will be re-run, starting again from the
beginning. Thus, the administrator should be careful when enabling
preemption of these dedicated resources. Enable dedicated preemption
with the configuration:

.. code-block:: text

    STARTD_JOB_ATTRS = JobPrio
    SCHEDD_PREEMPTION_REQUIREMENTS = (My.JobPrio < Target.JobPrio)
    SCHEDD_PREEMPTION_RANK = 0.0

In this example, preemption is enabled by user-defined job priority. If
a set of machines is running a job at user priority 5, and the user
submits a new job at user priority 10, the running job will be preempted
for the new job. The old job is put back in the queue, and will begin
again from the beginning when assigned to a newly acquired set of
machines.

Grouping Dedicated Nodes into Parallel Scheduling Groups
''''''''''''''''''''''''''''''''''''''''''''''''''''''''

:index:`parallel scheduling groups`

In some parallel environments, machines are divided into groups, and
jobs should not cross groups of machines. That is, all the nodes of a
parallel job should be allocated to machines within the same group. The
most common example is a pool of machine using InfiniBand switches. For
example, each switch might connect 16 machines, and a pool might have
160 machines on 10 switches. If the InfiniBand switches are not routed
to each other, each job must run on machines connected to the same
switch. The dedicated scheduler's Parallel Scheduling Groups feature
supports this operation.

Each *condor_startd* must define which group it belongs to by setting the
:macro:`ParallelSchedulingGroup` variable in the configuration file, and
advertising it into the machine ClassAd. The value of this variable is a
string, which should be the same for all *condor_startd* daemons within a given
group. The property must be advertised in the *condor_startd* ClassAd by
appending :macro:`ParallelSchedulingGroup` to the :macro:`STARTD_ATTRS`
configuration variable.

The submit description file for a parallel universe job which must not
cross group boundaries contains

.. code-block:: text

    +WantParallelSchedulingGroups = True

The dedicated scheduler enforces the allocation to within a group.

High Availability of the Job Queue
----------------------------------

:index:`of job queue<single: of job queue; High Availability>`

.. warning::
    This High Availability configuration depends entirely on using
    an extremely reliably shared file server.  In our experience, only
    expensive, proprietary file servers are suitable for this role.
    Frequently, casual configuration of a Highly Available HTCondor
    job queue will result in lowered reliability.

For a pool where all jobs are submitted through a single machine in the
pool, and there are lots of jobs, this machine becoming nonfunctional
means that jobs stop running. The *condor_schedd* daemon maintains the
job queue. No job queue due to having a nonfunctional machine implies
that no jobs can be run. This situation is worsened by using one machine
as the single submission point. For each HTCondor job (taken from the
queue) that is executed, a *condor_shadow* process runs on the machine
where submitted to handle input/output functionality. If this machine
becomes nonfunctional, none of the jobs can continue. The entire pool
stops running jobs.

The goal of High Availability in this special case is to transfer the
*condor_schedd* daemon to run on another designated machine. Jobs
caused to stop without finishing can be restarted from the beginning, or
can continue execution using the most recent checkpoint. New jobs can
enter the job queue. Without High Availability, the job queue would
remain intact, but further progress on jobs would wait until the machine
running the *condor_schedd* daemon became available (after fixing
whatever caused it to become unavailable).

HTCondor uses its flexible configuration mechanisms to allow the
transfer of the *condor_schedd* daemon from one machine to another. The
configuration specifies which machines are chosen to run the
*condor_schedd* daemon. To prevent multiple *condor_schedd* daemons
from running at the same time, a lock (semaphore-like) is held over the
job queue. This synchronizes the situation in which control is
transferred to a secondary machine, and the primary machine returns to
functionality. Configuration variables also determine time intervals at
which the lock expires, and periods of time that pass between polling to
check for expired locks.

To specify a single machine that would take over, if the machine running
the *condor_schedd* daemon stops working, the following additions are
made to the local configuration of any and all machines that are able to
run the *condor_schedd* daemon (becoming the single pool submission
point):

.. code-block:: condor-config

    MASTER_HA_LIST = SCHEDD
    SPOOL = /share/spool
    HA_LOCK_URL = file:/share/spool
    VALID_SPOOL_FILES = $(VALID_SPOOL_FILES) SCHEDD.lock

Configuration macro :macro:`MASTER_HA_LIST`
identifies the *condor_schedd* daemon as the daemon that is to be
watched to make sure that it is running. Each machine with this
configuration must have access to the lock (the job queue) which
synchronizes which single machine does run the *condor_schedd* daemon.
This lock and the job queue must both be located in a shared file space,
and is currently specified only with a file URL. The configuration
specifies the shared space (:macro:`SPOOL`), and the URL of the lock.
:tool:`condor_preen` is not currently aware of the lock file and will delete
it if it is placed in the :macro:`SPOOL` directory, so be sure to add file
``SCHEDD.lock`` to :macro:`VALID_SPOOL_FILES[with HA Schedd]`.

As HTCondor starts on machines that are configured to run the single
*condor_schedd* daemon, the :tool:`condor_master` daemon of the first
machine that looks at (polls) the lock and notices that no lock is held.
This implies that no *condor_schedd* daemon is running. This
:tool:`condor_master` daemon acquires the lock and runs the *condor_schedd*
daemon. Other machines with this same capability to run the
*condor_schedd* daemon look at (poll) the lock, but do not run the
daemon, as the lock is held. The machine running the *condor_schedd*
daemon renews the lock periodically.

If the machine running the *condor_schedd* daemon fails to renew the
lock (because the machine is not functioning), the lock times out
(becomes stale). The lock is released by the :tool:`condor_master` daemon if
:tool:`condor_off` or *condor_off -schedd* is executed, or when the
:tool:`condor_master` daemon knows that the *condor_schedd* daemon is no
longer running. As other machines capable of running the
*condor_schedd* daemon look at the lock (poll), one machine will be the
first to notice that the lock has timed out or been released. This
machine (correctly) interprets this situation as the *condor_schedd*
daemon is no longer running. This machine's :tool:`condor_master` daemon then
acquires the lock and runs the *condor_schedd* daemon.

See the :ref:`admin-manual/configuration-macros:condor_master configuration
file macros` section for details relating to the configuration variables used
to set timing and polling intervals.

Performance Tuning of the AP
----------------------------

Of the three roles (AP, CM, EP) in a HTCondor system, the AP is the most common
place performance tuning is done.  The CM is mostly stateless, and can
typically scale out to very large pools without much additional work.  The EP
daemons aren't resource intensive.  However, as the AP stores the state of all
the jobs under its control, and persistently stores frequent updates to those
jobs, it is not uncommon for the AP to exhaust system resources, like cpu, or
disk and network bandwidth.

Monitoring AP Performance
'''''''''''''''''''''''''

The *condor_schedd* is single threaded.  Practically, this means that it only
does one thing at a time, and often when it may be "busy" doing that one thing,
it is actually waiting on the system for some i/o to complete.  As such, it
will rarely appear to use 100% of a cpu in any system monitoring tool.  To help
guage how busy the schedd is, it keeps track of a metric called
:ad-attr:`RecentDaemonCoreDutyCycle`.  This is a floating point value that
ranges from 0.0 (completely idle) to 1.0 (competely busy).  Values over 0.95
indicate the schedd is overloaded.  In extreme cases :tool:`condor_q` and
:tool:`condor_submit` may timeout and fail trying to communicate to an
overloaded schedd.  An administrator can see this attribute by running

.. code-block:: console

    $ condor_status -direct -schedd name-of-schedd -af RecentDaemonCoreDutyCycle


Horizontal Scaling
''''''''''''''''''

While the *condor_schedd* and the machine it runs on can be tuned to handle a
greater rate of jobs, every machine has some limit of jobs it can support.  The
main strategy for supporting more jobs in the system as a whole is simply by
running more schedds, or horizontal scaling.  This may require partitioning
users onto differening submit machines, or submiting remotely, but at the end
of the day, the best way to scale out a very large HTCondor system is by adding
more *condor_schedd*'s.

Putting the schedd's database on the fastest disk
'''''''''''''''''''''''''''''''''''''''''''''''''

The *condor_schedd* frequently saves state to a file on disk, so that in event
of a crash, no jobs will be lost on a restart.  The cost of this reliability,
though, is relatively high.  In addition to writing to the disk, the schedd
uses the fsync system call to force all the data onto the disk. By default,
this file named job_queue.log is written to the :macro:`SPOOL` directory.
However, the configuration option :macro:`JOB_QUEUE_LOG` will override this path.  Setting
:macro:`JOB_QUEUE_LOG` to point to a file on a solid state or nvme drive will
make the schedd faster.  Ideally, this path should be on a filesystem that only
holds this file.

Avoiding shared filesystems for event logs
''''''''''''''''''''''''''''''''''''''''''

Another type of file the *condor_schedd* frequently writes to are job event
logs, those specified by the :subcom:`log` submit command.  When these are on
NFS or other distributed or slow filesystems, the whole system can slow down
tremendously.  If possible, encourage users not to put their event logs on such
slow filesystems.

Using third party (url / plugin) transfers when able
''''''''''''''''''''''''''''''''''''''''''''''''''''

HTCondor can transfer user's sandboxes to the EP in many ways.  The default
method, called HTCondor file transfer, or "cedar" file transfer, copies files
from the AP to the EP.  Obviously, this uses cpu, disk and network bandwidth on
the AP.  To the degree possible, changing large input file file transfers from
cedar, to http transfers from some third party server, moves the load off of
the AP, and onto an http server.  If one http server isn't sufficent there are
many methods for scaling http servers to handle additional load.

Limiting CPU or I/O bound procesing on the AP
'''''''''''''''''''''''''''''''''''''''''''''

The machine the *condor_schedd* runs on is typically a machine users can log
into, to prepare and submit jobs.  Sometimes, users will start long-running,
cpu or I/O heavy jobs on the submit machine, which can slow down the various
HTCondor services on that machine.  We encourage admins to try to limit this,
either by social pressure, or enforced by system limits on the user cpu.

.. _enabling htcondor annex:

Enabling ``htcondor annex`` on an AP
------------------------------------

The macro template :macro:`use feature:HPC_ANNEX` enables the ``annex``
noun of the :doc:`../man-pages/htcondor` command and configures HTCondor
to support it.

.. note::

    The following section is not normative; it reflects the implementation
    at the time of writing.

The annex pilot starts an EP which directly connects to the AP, using an
IDTOKEN generated by :doc:`../man-pages/condor_token_fetch`.  The AP will
only run a job on a directly-connected EP if that token's identifier is
the same as that job's :ad-attr:`Owner` attribute.  Obtaining this token will
naturally fail if the user running ``htcondor annex`` does not have
permission to submit jobs.  (The signing key used to generate the token is
automatically created as a result of the macro template and is used for no
other purpose.)

However, because the schedd does not have a stable address by default (its
shared port ID changes), the annex pilot needs a collector to look the
schedd up in.  So you will notice a small collector on the side; the
macro template calls it the "AP collector".  This collector performs two
other tasks: it generates the random key used to sign the pilot job's
token, and it holds a copy of the slot ads generated by annex EPs so that
``htcondor annex`` does not have to query the schedd for them, reducing
the load on that daemon.

The configuration assumes that IDTOKENS are enabled for both the schedd
and the collector, rather than trying to modify the security configuration
of those two daemons.

Finally, the macro template adds a job transform so that jobs submitted
with a ``TargetAnnexName`` attribute -- as jobs submitted via
``htcondor job submit --annex-name`` will have -- will only run on resources
with the same annex name and with the same owner.

Third-Party Storage for Self-Checkpointing Applications
-------------------------------------------------------

:ref:`users-manual/self-checkpointing-applications:self-checkpointing applications`
require more storage and bandwidth at the AP than equivalent non-checkpointing
jobs.  This can cause problems for both the HTCondor administrator -- having to
deal with the additional load -- and for the submitter, whose self-checkpointing
jobs may run unnecessarily slowly because of the limitations imposed by the AP's
disk and/or network bandwidth, or worse, be unable to run at all because the AP
has run out of storage space.

The solution is for users to set a :subcom:`checkpoint_destination` -- or the
administrator to choose a default for them
(see :macro:`use feature:DefaultCheckpointDestination`).  This allows HTCondor
to store and retrieve checkpoints from third-party storage services (e.g., a
Pelican data federation or S3).  Of course, superceded checkpoints, and
checkpoints from jobs which have completed, must also be deleted.  This
requires addtional configuration.  Specifically, although HTCondor's
file-transfer plug-ins know how to upload and download files, they don't know
how to delete them; some other method must be specified to HTCondor.  See
:ref:`self-checkpointing-jobs` for details.

In order to conserve storage space, HTCondor defaults to keeping only one
"extra" checkpoint; each time a new checkpoint finishes uploading,
HTCondor will keep the newly-uploaded checkpoint, the immediately-previous
checkpoint, and remove (or schedule the removal of) all the rest (of which
there will normally be only one).  Note that this means self-checkpoint jobs
which use third-party storage will need there
three times the storage space of a single checkpoint: one for the current
checkpoint, one for the previous checkpoint, and one for the checkpoint
currently being uploaded.  You can adjust the number of checkpoints to keep
by setting :macro:`DEFAULT_NUM_EXTRA_CHECKPOINTS`.

HTCondor stores this extra checkpoint by default to guard against corrupted
checkpoints: it computes and records the checksum of every file uploaded as
part of a checkpoint and computes and compares the checksum of every file
download from a checkpoint; if any don't match, the checkpoint is considered
invalid and deleted, and the job is rescheduled (and will resume from the
"extra" checkpoint(s), if one remains available).
