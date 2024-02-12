Job Scheduling
==============

Priorities and Preemption
-------------------------

HTCondor has two independent priority controls: job priorities and user
priorities.

The HTCondor system calculate a "fair share" of machine slots to allocate to each user.
Whether each user can use all of these slots depends on a number of factors. For example,
if the user's jobs only match to a small number of machines, perhaps
the user will be running fewer jobs than allocated.  This fair share is based on the
*user priority*.  Each user can then specify the order in which each of their jobs
should be matched and run on the fair share, this is based on the *job priority*.

.. _jobprio:

Job Priority
''''''''''''

Job priorities allow a user to sort their own jobs to determine which are
tried to be run first.  A job priority can be any integer: larger values 
denote better priority.  So, 0 is a better job priority than -3, and 6 is a better than 5.
:index:`condor_prio<single: condor_prio; HTCondor commands>`

.. note::

   Job priorities are computed per user, so that whatever job priorities
   one user sets has no impact at all on any other user, in terms of how many jobs
   users can run or in what order.  Also, unmatchable high priority jobs do not block
   lower priority jobs.  That is, a priority 10 job will try to be matched before 
   a priority 9 job, but if the priority 10 job doesn't match any slots, HTCondor 
   will keep going, and try the priority 9 job next.

The job priority may be specified in the submit description file by setting

.. code-block:: condor-submit

    priority = 15

If no priority is set, the default is 0. See the Dagman section for ways that dagman
can automatically set the priority of any or all jobs in a dag.

Each job can be given a distinct priority. For an
already queued job, its priority may be changed with the :tool:`condor_prio`.
command.  This sets the value
of job ClassAd attribute :ad-attr:`JobPrio`.  :tool:`condor_prio` can be called on a running
job, but lowering a job priority will not trigger eviction of the running 
job.  The :tool:`condor_vacate_job` command can preempt a running job.

A fine-grained categorization of jobs and their ordering is available
for experts by using the job ClassAd attributes: :ad-attr:`PreJobPrio1`,
:ad-attr:`PreJobPrio2`, :ad-attr:`JobPrio`, :ad-attr:`PostJobPrio1`, or :ad-attr:`PostJobPrio2`.

User priority
'''''''''''''

:index:`priority<single: priority; preemption>`
:index:`priority<single: priority; user>`
:index:`of a user<single: of a user; priority>`

Slots are allocated to users based upon user priority. A lower
numerical value for user priority means proportionally better priority, 
so a user with priority 5 will be allocated 10 times the resources as
someone with user priority 50. User priorities in HTCondor can be 
examined with the :tool:`condor_userprio` command.  HTCondor
administrators can set and change individual user priorities with the
same utility.

HTCondor continuously calculates the share of available machines that
each user should be allocated. This share is inversely related to the
ratio between user priorities. For example, a user with a priority of 10
will get twice as many machines as a user with a priority of 20. The
priority of each individual user changes according to the number of
resources the individual is using. Each user starts out with the best
possible priority: 0.5. If the number of machines a user currently has
is greater than the user priority, the user priority will worsen by
numerically increasing over time. If the number of machines is less then
the priority, the priority will improve by numerically decreasing over
time. The long-term result is fair-share access across all users. The
speed at which HTCondor adjusts the priorities is controlled with the
configuration variable :macro:`PRIORITY_HALFLIFE[definition]`.
The default is one day. A user running 100 cores of jobs for a long
time will have their real user priority exponential grow to 100. 
If all the jobs are removed, one day later that user's real priority 
will be 50, and two days later it will shrink to 25.

HTCondor enforces that each user gets his/her fair share of machines
according to user priority by allocating available machines.
Optionally, a pool administrator can configure the system to preempt
the running jobs of users who are above their fair share in favor
of users who are below their fair share, but this is not the default.
For instance, if a low priority user is utilizing all available machines
and suddenly a higher priority user submits jobs, HTCondor may
vacate jobs belonging to the lower priority user. 

User priorities are keyed on ``<username>@<domain>``, for example
``johndoe@cs.wisc.edu``. The domain name to use, if any, is configured
by the HTCondor site administrator. Thus, user priority and therefore
resource allocation is not impacted by which machine the user submits
from or even if the user submits jobs from multiple machines.
:index:`nice job<single: nice job; priority>`

The user priority system can also support backfill or nice jobs (see
the :tool:`condor_submit` manual page). Nice jobs
artificially boost the user priority by ten million just for the nice
job. This effectively means that nice jobs will only run on machines
that no other HTCondor job (that is, non-niced job) wants. In a similar
fashion, an HTCondor administrator could set the user priority of any
specific HTCondor user very high. If done, for example, with a guest
account, the guest could only use cycles not wanted by other users of
the system.

How Jobs are Vacated
''''''''''''''''''''

:index:`vacate` :index:`vacate<single: vacate; preemption>`

When HTCondor needs a job to vacate a machine for whatever reason, it
sends the job an operating system signal specified in the :ad-attr:`KillSig`
attribute of the job's ClassAd. The value of this attribute can be
specified by the user at submit time by placing the :subcom:`kill_sig` option
in the HTCondor submit description file.

If a program wanted to do some work when asked to vacate a
machine, the program may set up a signal handler to handle this
signal. This clean up signal is specified with :subcom:`kill_sig`. Note that
the clean up work needs to be quick. If the job takes too long to exit
after getting the :subcom:`kill_sig`, HTCondor sends a SIGKILL signal 
which immediately terminates the process.

The default value for :ad-attr:`KillSig` is SIGTERM, the usual method 
to nicely terminate a Unix program.

.. _crontab:

Time Scheduling for Job Execution
---------------------------------

:index:`to execute at a specific time<single: to execute at a specific time; scheduling jobs>`
:index:`at a specific time<single: at a specific time; job execution>`

CronTab Scheduling
''''''''''''''''''

:index:`CronTab job scheduling`
:index:`periodic<single: periodic; job scheduling>`
:index:`to execute periodically<single: to execute periodically; scheduling jobs>`

HTCondor's CronTab scheduling functionality allows jobs to be scheduled
to execute periodically. A job's execution schedule is defined by
commands within the submit description file. The notation is much like
that used by the Unix *cron* daemon. As such, HTCondor developers are
fond of referring to CronTab :index:`Crondor`\ scheduling as
Crondor. 

.. sidebar:: Example Crondor Submit File

   .. code-block:: condor-submit
     :caption: A job that runs every 15 minutes

     Executable = /bin/sleep
     Arguments = 15

     cron_minute = 0,15,30,45
     cron_prep_time = 60
     OnExitRemove = false

     Error = error.$(Cluster)
     Output = out.$(Cluster)
     Log = log

     Request_Cpus   = 1
     Request_Memory = 100M
     Request_Disk   = 100M
     Queue

Also, unlike the Unix *cron* daemon, HTCondor never runs more than one
instance of a job at the same time.

The capability for repetitive or periodic execution of the job is
enabled by specifying an
:subcom:`on_exit_remove[and crondor]`
command for the job, such that the job does not leave the queue until
desired.

Semantics for CronTab Specification
'''''''''''''''''''''''''''''''''''

A job's execution schedule is defined by a set of specifications within
the submit description file. HTCondor uses these to calculate a
:ad-attr:`DeferralTime` for the job.

Table 2.3 lists the submit commands and acceptable
values for these commands. At least one of these must be defined in
order for HTCondor to calculate a :ad-attr:`DeferralTime` for the job. Once one
CronTab value is defined, the default for all the others uses all the
values in the allowed values ranges.

+----------------------------------------------+----------------------------+
| :subcom:`cron_minute[definition]`            | 0 - 59                     |
+----------------------------------------------+----------------------------+
| :subcom:`cron_hour[definition]`              | 0 - 23                     |
+----------------------------------------------+----------------------------+
| :subcom:`cron_day_of_month[definition]`      | 1 - 31                     |
+----------------------------------------------+----------------------------+
| :subcom:`cron_month[definition]`             | 1 - 12                     |
+----------------------------------------------+----------------------------+
| :subcom:`cron_day_of_week[definition]`       | 0 - 7 (Sunday is 0 or 7)   |
+----------------------------------------------+----------------------------+

Table 2.3: The list of submit commands and their value ranges.


The day of a job's execution can be specified by both the
**cron_day_of_month** and the **cron_day_of_week** attributes. The
day will be the logical or of both.

The semantics allow more than one value to be specified by using the \*
operator, ranges, lists, and steps (strides) within ranges.

 The asterisk operator
    The \* (asterisk) operator specifies that all of the allowed values
    are used for scheduling. For example,

    .. code-block:: condor-submit

              cron_month = *


    becomes any and all of the list of possible months:
    (1,2,3,4,5,6,7,8,9,10,11,12). Thus, a job runs any month in the
    year.

 Ranges
    A range creates a set of integers from all the allowed values
    between two integers separated by a hyphen. The specified range is
    inclusive, and the integer to the left of the hyphen must be less
    than the right hand integer. For example,

    .. code-block:: condor-submit

              cron_hour = 0-4


    represents the set of hours from 12:00 am (midnight) to 4:00 am, or
    (0,1,2,3,4).

 Lists
    A list is the union of the values or ranges separated by commas.
    Multiple entries of the same value are ignored. For example,

    .. code-block:: condor-submit

              cron_minute = 15,20,25,30
              cron_hour   = 0-3,9-12,15


    where this :subcom:`cron_minute` example represents (15,20,25,30) and
    :subcom:`cron_hour` represents (0,1,2,3,9,10,11,12,15).

 Steps
    Steps select specific numbers from a range, based on an interval. A
    step is specified by appending a range or the asterisk operator with
    a slash character (/), followed by an integer value. For example,

    .. code-block:: condor-submit

              cron_minute = 10-30/5
              cron_hour = */3


    where this :subcom:`cron_minute` example specifies every five minutes
    within the specified range to represent (10,15,20,25,30), and
    :subcom:`cron_hour` specifies every three hours of the day to represent
    (0,3,6,9,12,15,18,21).

Preparation Time and Execution Window
'''''''''''''''''''''''''''''''''''''

The
:subcom:`cron_prep_time[definition]`
command is analogous to the deferral time's
:subcom:`deferral_prep_time[definition]`
command. It specifies the number of seconds before the deferral time
that the job is to be matched and sent to the execution machine. This
permits HTCondor to make necessary preparations before the deferral time
occurs.

Consider the submit description file example that includes

.. code-block:: condor-submit

       cron_minute = 0
       cron_hour = *
       cron_prep_time = 300

The job is scheduled to begin execution at the top of every hour. Note
that the setting of **cron_hour** in this example is not required, as
the default value will be \*, specifying any and every hour of the day.
The job will be matched and sent to an execution machine no more than
five minutes before the next deferral time. For example, if a job is
submitted at 9:30am, then the next deferral time will be calculated to
be 10:00am. HTCondor may attempt to match the job to a machine and send
the job once it is 9:55am.

As the CronTab scheduling calculates and uses deferral time, jobs may
also make use of the deferral window. The submit command
:subcom:`cron_window[definition]` is
analogous to the submit command
:subcom:`deferral_window[q.v. cron_window]`.
Consider the submit description file example that includes

.. code-block:: condor-submit

       cron_minute = 0
       cron_hour = *
       cron_window = 360

As the previous example, the job is scheduled to begin execution at the
top of every hour. Yet with no preparation time, the job is likely to
miss its deferral time. The 6-minute window allows the job to begin
execution, as long as it arrives and can begin within 6 minutes of the
deferral time, as seen by the time kept on the execution machine.

Scheduling
''''''''''

When a job using the CronTab functionality is submitted to HTCondor, use
of at least one of the submit description file commands beginning with
**cron_** causes HTCondor to calculate and set a deferral time for when
the job should run. A deferral time is determined based on the current
time rounded later in time to the next minute. The deferral time is the
job's :ad-attr:`DeferralTime` attribute. A new deferral time is calculated when
the job first enters the job queue, when the job is re-queued, or when
the job is released from the hold state. New deferral times for all jobs
in the job queue using the CronTab functionality are recalculated when a
:tool:`condor_reconfig` or a :tool:`condor_restart` command that affects the job
queue is issued.

A job's deferral time is not always the same time that a job will
receive a match and be sent to the execution machine. This is because
HTCondor operates on the job queue at times that are independent of job
events, such as when job execution completes. Therefore, HTCondor may
operate on the job queue just after a job's deferral time states that it
is to begin execution. HTCondor attempts to start a job when the
following pseudo-code boolean expression evaluates to ``True``:

.. code-block:: text

       ( time() + SCHEDD_INTERVAL ) >= ( DeferralTime - CronPrepTime )

If the ``time()`` plus the number of seconds until the next time
HTCondor checks the job queue is greater than or equal to the time that
the job should be submitted to the execution machine, then the job is to
be matched and sent now.

Jobs using the CronTab functionality are not automatically re-queued by
HTCondor after their execution is complete. The submit description file
for a job must specify an appropriate
:subcom:`on_exit_remove[and crondor]`
command to ensure that a job remains in the queue. This job maintains
its original :ad-attr:`ClusterId` and :ad-attr:`ProcId`.

Submit Commands Usage Examples
''''''''''''''''''''''''''''''

Here are some examples of the submit commands necessary to schedule jobs
to run at multifarious times. Please note that it is not necessary to
explicitly define each attribute; the default value is \*.

Run 23 minutes after every two hours, every day of the week:

.. code-block:: condor-submit

       on_exit_remove = false
       cron_minute = 23
       cron_hour = 0-23/2
       cron_day_of_month = *
       cron_month = *
       cron_day_of_week = *

Run at 10:30pm on each of May 10th to May 20th, as well as every
remaining Monday within the month of May:

.. code-block:: condor-submit

       on_exit_remove = false
       cron_minute = 30
       cron_hour = 20
       cron_day_of_month = 10-20
       cron_month = 5
       cron_day_of_week = 2

Run every 10 minutes and every 6 minutes before noon on January 18th
with a 2-minute preparation time:

.. code-block:: condor-submit

       on_exit_remove = false
       cron_minute = */10,*/6
       cron_hour = 0-11
       cron_day_of_month = 18
       cron_month = 1
       cron_day_of_week = *
       cron_prep_time = 120

Submit Commands Limitations
'''''''''''''''''''''''''''

The use of the CronTab functionality has all of the same limitations of
deferral times, because the mechanism is based upon deferral times.

-  It is impossible to schedule vanilla universe jobs at
   intervals that are smaller than the interval at which HTCondor
   evaluates jobs. This interval is determined by the configuration
   variable :macro:`SCHEDD_INTERVAL`. As a
   vanilla universe job completes execution and is placed
   back into the job queue, it may not be placed in the idle state in
   time. This problem does not afflict local universe jobs.
-  HTCondor cannot guarantee that a job will be matched in order to make
   its scheduled deferral time. A job must be matched with an execution
   machine just as any other HTCondor job; if HTCondor is unable to find
   a match, then the job will miss its chance for executing and must
   wait for the next execution time specified by the CronTab schedule.


Jobs may be scheduled to begin execution at a specified time in the
future with HTCondor's job deferral functionality. All specifications
are in a job's submit description file. Job deferral functionality is
expanded to provide for the periodic execution of a job, known as the
CronTab scheduling.

Job Deferral
''''''''''''

:index:`job deferral time`
:index:`of a job<single: of a job; deferral time>`

The scheduling of jobs using HTCondor's CronTab feature
calculates and utilizes the :ad-attr:`DeferralTime` ClassAd attribute.
Job deferral allows the specification of the exact date and time at
which a job is to begin executing. HTCondor attempts to match the job to
an execution machine just like any other job, however, the job will wait
until the exact time to begin execution. A user can define the job to
allow some flexibility in the execution of jobs that miss their
execution time.

Deferred Execution Time
'''''''''''''''''''''''

:index:`of a job<single: of a job; deferral time>`
:index:`DeferralTime<single: DeferralTime; definition>`

A job's deferral time is the exact time that HTCondor should attempt to
execute the job. The deferral time attribute is defined as an expression
that evaluates to a Unix Epoch timestamp (the number of seconds elapsed
since 00:00:00 on January 1, 1970, Coordinated Universal Time). This is
the time that HTCondor will begin to execute the job.

After a job is matched and all of its files have been transferred to an
execution machine, HTCondor checks to see if the job's ClassAd contains
a deferral time. If it does, HTCondor calculates the number of seconds
between the execution machine's current system time and the job's
deferral time. If the deferral time is in the future, the job waits to
begin execution. While a job waits, its job ClassAd attribute
:ad-attr:`JobStatus` indicates the job is in the Running state. As the deferral
time arrives, the job begins to execute. If a job misses its execution
time, that is, if the deferral time is in the past, the job is evicted
from the execution machine and put on hold in the queue.

The specification of a deferral time does not interfere with HTCondor's
behavior. For example, if a job is waiting to begin execution when a
:tool:`condor_hold` command is issued, the job is removed from the execution
machine and is put on hold. If a job is waiting to begin execution when
a :tool:`condor_suspend` command is issued, the job continues to wait. When
the deferral time arrives, HTCondor begins execution for the job, but
immediately suspends it.

The deferral time is specified in the job's submit description file with
the command
:subcom:`deferral_time[definition]`.

Deferral Window
'''''''''''''''

:index:`DeferralWindow<single: DeferralWindow; definition>` 
If a job arrives at its execution machine after the deferral time has
passed, the job is evicted from the machine and put on hold in the job
queue. This may occur, for example, because the transfer of needed files
took too long due to a slow network connection. A deferral window
permits the execution of a job that misses its deferral time by
specifying a window of time within which the job may begin.

The deferral window is the number of seconds after the deferral time,
within which the job may begin. When a job arrives too late, HTCondor
calculates the difference in seconds between the execution machine's
current time and the job's deferral time. If this difference is less
than or equal to the deferral window, the job immediately begins
execution. If this difference is greater than the deferral window, the
job is evicted from the execution machine and is put on hold in the job
queue.

The deferral window is specified in the job's submit description file
with the command :subcom:`deferral_window[definition]`.

Preparation Time
''''''''''''''''

:index:`DeferralPrepTime<single: DeferralPrepTime; definition>`

When a job defines a deferral time far in the future and then is matched
to an execution machine, potential computation cycles are lost because
the deferred job has claimed the machine, but is not actually executing.
Other jobs could execute during the interval when the job waits for its
deferral time. To make use of the wasted time,a job defines a
:subcom:`deferral_prep_time[definition]`
with an integer expression that evaluates to a number of seconds. At
this number of seconds before the deferral time, the job may be matched
with a machine.

Deferral Usage Examples
'''''''''''''''''''''''

:index:`deferral_time<single: deferral_time; example>`

Here are examples of how the job deferral time, deferral window, and the
preparation time may be used.

The job's submit description file specifies that the job is to begin
execution on January 1st, 2006 at 12:00 pm:

.. code-block:: condor-submit

       deferral_time = 1136138400

The Unix *date* program may be used to calculate a Unix epoch time. The
syntax of the command to do this depends on the options provided within
that flavor of Unix. In some, it appears as

.. code-block:: console

    $ date --date "MM/DD/YYYY HH:MM:SS" +%s

and in others, it appears as

.. code-block:: console

    $ date -d "YYYY-MM-DD HH:MM:SS" +%s

MM is a 2-digit month number, DD is a 2-digit day of the month number,
and YYYY is a 4-digit year. HH is the 2-digit hour of the day, MM is the
2-digit minute of the hour, and SS are the 2-digit seconds within the
minute. The characters +%s tell the *date* program to give the output as
a Unix epoch time.

The job always waits 60 seconds after submission before beginning
execution:

.. code-block:: condor-submit

       deferral_time = (QDate + 60)

In this example, assume that the deferral time is 45 seconds in the past
as the job is available. The job begins execution, because 75 seconds
remain in the deferral window:

.. code-block:: condor-submit

       deferral_window = 120

In this example, a job is scheduled to execute far in the future, on
January 1st, 2010 at 12:00 pm. The
:subcom:`deferral_prep_time`
attribute delays the job from being matched until 60 seconds before the
job is to begin execution.

.. code-block:: condor-submit

       deferral_time      = 1262368800
       deferral_prep_time = 60

Deferral Limitations
''''''''''''''''''''

There are some limitations to HTCondor's job deferral feature.

-  Job deferral is not available for scheduler universe jobs. A
   scheduler universe job defining the :subcom:`deferral_time` produces a
   fatal error when submitted.
-  The time that the job begins to execute is based on the execution
   machine's system clock, and not the submission machine's system
   clock. Be mindful of the ramifications when the two clocks show
   dramatically different times.
-  A job's :ad-attr:`JobStatus` attribute is always in the Running state when
   job deferral is used. There is currently no way to distinguish
   between a job that is executing and a job that is waiting for its
   deferral time.

.. _matchmaking:

Matchmaking with ClassAds
-------------------------

Before you learn about how to submit a job, it is important to
understand how HTCondor allocates resources.
:index:`resource allocation<single: resource allocation; HTCondor>` Understanding the unique
framework by which HTCondor matches submitted jobs with machines is the
key to getting the most from HTCondor's scheduling algorithm.

HTCondor simplifies job submission by acting as a matchmaker of
ClassAds. HTCondor's ClassAds :index:`ClassAd` are analogous to
the classified advertising section of the newspaper. Sellers advertise
specifics about what they have to sell, hoping to attract a buyer.
Buyers may advertise specifics about what they wish to purchase. Both
buyers and sellers list constraints that need to be satisfied. For
instance, a buyer has a maximum spending limit, and a seller requires a
minimum purchase price. Furthermore, both want to rank requests to their
own advantage. Certainly a seller would rank one offer of $50 dollars
higher than a different offer of $25. In HTCondor, users submitting jobs
can be thought of as buyers of compute resources and machine owners are
sellers.

All machines in a HTCondor pool advertise their attributes,
:index:`attributes<single: attributes; ClassAd>` such as available memory, CPU type
and speed, virtual memory size, current load average, along with other
static and dynamic properties. This machine ClassAd
:index:`machine<single: machine; ClassAd>` also advertises under what conditions it
is willing to run a HTCondor job and what type of job it would prefer.
These policy attributes can reflect the individual terms and preferences
by which all the different owners have graciously allowed their machine
to be part of the HTCondor pool. You may advertise that your machine is
only willing to run jobs at night and when there is no keyboard activity
on your machine. In addition, you may advertise a preference (rank) for
running jobs submitted by you or one of your co-workers.

Likewise, when submitting a job, you specify a ClassAd with your
requirements and preferences. The ClassAd includes the type of machine you wish to
use. For instance, perhaps you are looking for the fastest floating
point performance available. You want HTCondor to rank available
machines based upon floating point performance. Or, perhaps you care
only that the machine has a minimum of 128 MiB of RAM. Or, perhaps you
will take any machine you can get! These job attributes and requirements
are bundled up into a job ClassAd.

HTCondor plays the role of a matchmaker by continuously reading all the
job ClassAds and all the machine ClassAds, matching and ranking job ads
with machine ads. HTCondor makes certain that all requirements in both
ClassAds are satisfied.

Inspecting Machine ClassAds with condor_status
''''''''''''''''''''''''''''''''''''''''''''''

:index:`condor_status<single: condor_status; HTCondor commands>`

Once HTCondor is installed, you will get a feel for what a machine
ClassAd does by trying the :tool:`condor_status` command. Try the
:tool:`condor_status` command to get a summary of information from ClassAds
about the resources available in your pool. Type :tool:`condor_status` and
hit enter to see a summary similar to the following:

.. code-block:: text

    Name               OpSys      Arch   State     Activity LoadAv Mem   ActvtyTime

    amul.cs.wisc.edu   LINUX      INTEL  Claimed   Busy     0.990  1896  0+00:07:04
    slot1@amundsen.cs. LINUX      INTEL  Owner     Idle     0.000  1456  0+00:21:58
    slot2@amundsen.cs. LINUX      INTEL  Owner     Idle     0.110  1456  0+00:21:59
    angus.cs.wisc.edu  LINUX      INTEL  Claimed   Busy     0.940   873  0+00:02:54
    anhai.cs.wisc.edu  LINUX      INTEL  Claimed   Busy     1.400  1896  0+00:03:03
    apollo.cs.wisc.edu LINUX      INTEL  Unclaimed Idle     1.000  3032  0+00:00:04
    arragon.cs.wisc.ed LINUX      INTEL  Claimed   Busy     0.980   873  0+00:04:29
    bamba.cs.wisc.edu  LINUX      INTEL  Owner     Idle     0.040  3032 15+20:10:19
    ...


The :tool:`condor_status` command has options that summarize machine ads in a
variety of ways. For example,

 *condor_status -available*
    shows only machines which are willing to run jobs now.
 *condor_status -run*
    shows only machines which are currently running jobs.
 *condor_status -long*
    lists the machine ClassAds for all machines in the pool.

The following shows a portion of a machine ClassAd
:index:`machine example<single: machine example; ClassAd>` :index:`machine ClassAd`
for a single machine: turunmaa.cs.wisc.edu. Some of the listed
attributes are used by HTCondor for scheduling. Other attributes are for
information purposes. An important point is that any of the attributes
in a machine ClassAd can be utilized at job submission time as part of a
request or preference on what machine to use. Additional attributes can
be easily added. For example, your site administrator can add a physical
location attribute to your machine ClassAds.

.. code-block:: condor-classad

    Machine = "turunmaa.cs.wisc.edu"
    FileSystemDomain = "cs.wisc.edu"
    Name = "turunmaa.cs.wisc.edu"
    CondorPlatform = "$CondorPlatform: x86_rhap_5 $"
    Cpus = 1
    CondorVersion = "$CondorVersion: 7.6.3 Aug 18 2011 BuildID: 361356 $"
    Requirements = START
    EnteredCurrentActivity = 1316094896
    MyAddress = "<128.105.175.125:58026>"
    EnteredCurrentState = 1316094896
    Memory = 1897
    CkptServer = "pitcher.cs.wisc.edu"
    OpSys = "LINUX"
    State = "Owner"
    START = true
    Arch = "INTEL"
    Mips = 2634
    Activity = "Idle"
    StartdIpAddr = "<128.105.175.125:58026>"
    TargetType = "Job"
    LoadAvg = 0.210000
    Disk = 92309744
    VirtualMemory = 2069476
    TotalSlots = 1
    UidDomain = "cs.wisc.edu"
    MyType = "Machine"
