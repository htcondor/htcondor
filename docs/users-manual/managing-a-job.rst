      

Managing a Job
==============

This section provides a brief summary of what can be done once jobs are
submitted. The basic mechanisms for monitoring a job are introduced, but
the commands are discussed briefly. You are encouraged to look at the
man pages of the commands referred to (located in
Chapter \ `12 <CommandReferenceManualmanpages.html#x90-63900012>`__
beginning on
page \ `1739 <CommandReferenceManualmanpages.html#x90-63900012>`__) for
more information.

When jobs are submitted, HTCondor will attempt to find resources to run
the jobs. A list of all those with jobs submitted may be obtained
through *condor\_status* with the *-submitters* option. An example of
this would yield output similar to:

::

    %  condor_status -submitters 
     
    Name                 Machine      Running IdleJobs HeldJobs 
     
    ballard@cs.wisc.edu  bluebird.c         0       11        0 
    nice-user.condor@cs. cardinal.c         6      504        0 
    wright@cs.wisc.edu   finch.cs.w         1        1        0 
    jbasney@cs.wisc.edu  perdita.cs         0        0        5 
     
                               RunningJobs           IdleJobs           HeldJobs 
     
     ballard@cs.wisc.edu                 0                 11                  0 
     jbasney@cs.wisc.edu                 0                  0                  5 
    nice-user.condor@cs.                 6                504                  0 
      wright@cs.wisc.edu                 1                  1                  0 
     
                   Total                 7                516                  5

Checking on the progress of jobs
--------------------------------

At any time, you can check on the status of your jobs with the
*condor\_q* command. This command displays the status of all queued
jobs. An example of the output from *condor\_q* is

::

    %  condor_q 
     
    -- Submitter: submit.chtc.wisc.edu : <128.104.55.9:32772> : submit.chtc.wisc.edu 
     ID      OWNER            SUBMITTED     RUN_TIME ST PRI SIZE CMD 
    711197.0   aragorn         1/15 19:18   0+04:29:33 H  0   0.0  script.sh 
    894381.0   frodo           3/16 09:06  82+17:08:51 R  0   439.5 elk elk.in 
    894386.0   frodo           3/16 09:06  82+20:21:28 R  0   219.7 elk elk.in 
    894388.0   frodo           3/16 09:06  81+17:22:10 R  0   439.5 elk elk.in 
    1086870.0   gollum          4/27 09:07   0+00:10:14 I  0   7.3  condor_dagman 
    1086874.0   gollum          4/27 09:08   0+00:00:01 H  0   0.0  RunDC.bat 
    1297254.0   legolas         5/31 18:05  14+17:40:01 R  0   7.3  condor_dagman 
    1297255.0   legolas         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman 
    1297256.0   legolas         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman 
    1297259.0   legolas         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman 
    1297261.0   legolas         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman 
    1302278.0   legolas         6/4  12:22   1+00:05:37 I  0   390.6 mdrun_1.sh 
    1304740.0   legolas         6/5  00:14   1+00:03:43 I  0   390.6 mdrun_1.sh 
    1304967.0   legolas         6/5  05:08   0+00:00:00 I  0   0.0  mdrun_1.sh 
     
    14 jobs; 4 idle, 8 running, 2 held 

This output contains many columns of information about the queued jobs.
The ST column (for status) shows the status of current jobs in the
queue:

    R: The job is currently running.
    I: The job is idle. It is not running right now, because it is
    waiting for a machine to become available.
    H: The job is the hold state. In the hold state, the job will not be
    scheduled to run until it is released. See the *condor\_hold* manual
    page located on page \ `1918 <Condorhold.html#x117-82200012>`__ and
    the *condor\_release* manual page located on
    page \ `2040 <Condorrelease.html#x134-95900012>`__.

The RUN\_TIME time reported for a job is the time that has been
committed to the job.

Another useful method of tracking the progress of jobs is through the
job event log. The specification of a ``log`` in the submit description
file causes the progress of the job to be logged in a file. Follow the
events by viewing the job event log file. Various events such as
execution commencement, checkpoint, eviction and termination are logged
in the file. Also logged is the time at which the event occurred.

When a job begins to run, HTCondor starts up a *condor\_shadow* process
on the submit machine. The shadow process is the mechanism by which the
remotely executing jobs can access the environment from which it was
submitted, such as input and output files.

It is normal for a machine which has submitted hundreds of jobs to have
hundreds of *condor\_shadow* processes running on the machine. Since the
text segments of all these processes is the same, the load on the submit
machine is usually not significant. If there is degraded performance,
limit the number of jobs that can run simultaneously by reducing the
``MAX_JOBS_RUNNING`` configuration variable.

You can also find all the machines that are running your job through the
*condor\_status* command. For example, to find all the machines that are
running jobs submitted by ``breach@cs.wisc.edu``, type:

::

    %  condor_status -constraint 'RemoteUser == "breach@cs.wisc.edu"' 
     
    Name       Arch     OpSys        State      Activity   LoadAv Mem  ActvtyTime 
     
    alfred.cs. INTEL    LINUX        Claimed    Busy       0.980  64    0+07:10:02 
    biron.cs.w INTEL    LINUX        Claimed    Busy       1.000  128   0+01:10:00 
    cambridge. INTEL    LINUX        Claimed    Busy       0.988  64    0+00:15:00 
    falcons.cs INTEL    LINUX        Claimed    Busy       0.996  32    0+02:05:03 
    happy.cs.w INTEL    LINUX        Claimed    Busy       0.988  128   0+03:05:00 
    istat03.st INTEL    LINUX        Claimed    Busy       0.883  64    0+06:45:01 
    istat04.st INTEL    LINUX        Claimed    Busy       0.988  64    0+00:10:00 
    istat09.st INTEL    LINUX        Claimed    Busy       0.301  64    0+03:45:00 
    ...

To find all the machines that are running any job at all, type:

::

    %  condor_status -run 
     
    Name       Arch     OpSys        LoadAv RemoteUser           ClientMachine 
     
    adriana.cs INTEL    LINUX        0.980  hepcon@cs.wisc.edu   chevre.cs.wisc. 
    alfred.cs. INTEL    LINUX        0.980  breach@cs.wisc.edu   neufchatel.cs.w 
    amul.cs.wi X86_64   LINUX        1.000  nice-user.condor@cs. chevre.cs.wisc. 
    anfrom.cs. X86_64   LINUX        1.023  ashoks@jules.ncsa.ui jules.ncsa.uiuc 
    anthrax.cs INTEL    LINUX        0.285  hepcon@cs.wisc.edu   chevre.cs.wisc. 
    astro.cs.w INTEL    LINUX        1.000  nice-user.condor@cs. chevre.cs.wisc. 
    aura.cs.wi X86_64   WINDOWS      0.996  nice-user.condor@cs. chevre.cs.wisc. 
    balder.cs. INTEL    WINDOWS      1.000  nice-user.condor@cs. chevre.cs.wisc. 
    bamba.cs.w INTEL    LINUX        1.574  dmarino@cs.wisc.edu  riola.cs.wisc.e 
    bardolph.c INTEL    LINUX        1.000  nice-user.condor@cs. chevre.cs.wisc. 
    ...

Removing a job from the queue
-----------------------------

A job can be removed from the queue at any time by using the
*condor\_rm* command. If the job that is being removed is currently
running, the job is killed without a checkpoint, and its queue entry is
removed. The following example shows the queue of jobs before and after
a job is removed.

::

    %  condor_q 
     
    -- Submitter: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu 
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD 
     125.0   jbasney         4/10 15:35   0+00:00:00 I  -10 1.2  hello.remote 
     132.0   raman           4/11 16:57   0+00:00:00 R  0   1.4  hello 
     
    2 jobs; 1 idle, 1 running, 0 held 
     
    %  condor_rm 132.0 
    Job 132.0 removed. 
     
    %  condor_q 
     
    -- Submitter: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu 
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD 
     125.0   jbasney         4/10 15:35   0+00:00:00 I  -10 1.2  hello.remote 
     
    1 jobs; 1 idle, 0 running, 0 held

Placing a job on hold
---------------------

A job in the queue may be placed on hold by running the command
*condor\_hold*. A job in the hold state remains in the hold state until
later released for execution by the command *condor\_release*.

Use of the *condor\_hold* command causes a hard kill signal to be sent
to a currently running job (one in the running state). For a standard
universe job, this means that no checkpoint is generated before the job
stops running and enters the hold state. When released, this standard
universe job continues its execution using the most recent checkpoint
available.

Jobs in universes other than the standard universe that are running when
placed on hold will start over from the beginning when released.

The manual page for *condor\_hold* on
page \ `1918 <Condorhold.html#x117-82200012>`__ and the manual page for
*condor\_release* on page \ `2040 <Condorrelease.html#x134-95900012>`__
contain usage details.

Changing the priority of jobs
-----------------------------

In addition to the priorities assigned to each user, HTCondor also
provides each user with the capability of assigning priorities to each
submitted job. These job priorities are local to each queue and can be
any integer value, with higher values meaning better priority.

The default priority of a job is 0, but can be changed using the
*condor\_prio* command. For example, to change the priority of a job to
-15,

::

    %  condor_q raman 
     
    -- Submitter: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu 
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD 
     126.0   raman           4/11 15:06   0+00:00:00 I  0   0.3  hello 
     
    1 jobs; 1 idle, 0 running, 0 held 
     
    %  condor_prio -p -15 126.0 
     
    %  condor_q raman 
     
    -- Submitter: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu 
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD 
     126.0   raman           4/11 15:06   0+00:00:00 I  -15 0.3  hello 
     
    1 jobs; 1 idle, 0 running, 0 held

It is important to note that these job priorities are completely
different from the user priorities assigned by HTCondor. Job priorities
do not impact user priorities. They are only a mechanism for the user to
identify the relative importance of jobs among all the jobs submitted by
the user to that specific queue.

Why is the job not running?
---------------------------

Users occasionally find that their jobs do not run. There are many
possible reasons why a specific job is not running. The following prose
attempts to identify some of the potential issues behind why a job is
not running.

At the most basic level, the user knows the status of a job by using
*condor\_q* to see that the job is not running. By far, the most common
reason (to the novice HTCondor job submitter) why the job is not running
is that HTCondor has not yet been through its periodic negotiation
cycle, in which queued jobs are assigned to machines within the pool and
begin their execution. This periodic event occurs by default once every
5 minutes, implying that the user ought to wait a few minutes before
searching for reasons why the job is not running.

Further inquiries are dependent on whether the job has never run at all,
or has run for at least a little bit.

For jobs that have never run, many problems can be diagnosed by using
the **-analyze** option of the *condor\_q* command. Here is an example;
running *condor\_q*\ ’s analyzer provided the following information:

::

    $ condor_q -analyze 27497829 
     
    -- Submitter: s1.chtc.wisc.edu : <128.104.100.43:9618?sock=5557_e660_3> : s1.chtc.wisc.edu 
    User priority for ei@chtc.wisc.edu is not available, attempting to analyze without it. 
    --- 
    27497829.000:  Run analysis summary.  Of 5257 machines, 
       5257 are rejected by your job's requirements 
          0 reject your job because of their own requirements 
          0 match and are already running your jobs 
          0 match but are serving other users 
          0 are available to run your job 
            No successful match recorded. 
            Last failed match: Tue Jun 18 14:36:25 2013 
     
            Reason for last match failure: no match found 
     
    WARNING:  Be advised: 
       No resources matched request's constraints 
     
    The Requirements expression for your job is: 
     
        ( OpSys == "OSX" ) && ( TARGET.Arch == "X86_64" ) && 
        ( TARGET.Disk >= RequestDisk ) && ( TARGET.Memory >= RequestMemory ) && 
        ( ( TARGET.HasFileTransfer ) || ( TARGET.FileSystemDomain == MY.FileSystemDomain ) ) 
     
     
    Suggestions: 
        Condition                         Machines Matched Suggestion 
        ---------                         ---------------- ---------- 
    1   ( target.OpSys == "OSX" )         0                MODIFY TO "LINUX" 
    2   ( TARGET.Arch == "X86_64" )       5190 
    3   ( TARGET.Disk >= 1 )              5257 
    4   ( TARGET.Memory >= ifthenelse(MemoryUsage isnt undefined,MemoryUsage,1) ) 
                                          5257 
    5   ( ( TARGET.HasFileTransfer ) || ( TARGET.FileSystemDomain == "submit-1.chtc.wisc.edu" ) ) 
                                          5257

This example also shows that the job does not run because the platform
requested, Mac OS X, is not available on any of the machines in the
pool. Recall that unless informed otherwise in the **Requirements**
expression in the submit description file, the platform requested for an
execute machine will be the same as the platform where *condor\_submit*
is run to submit the job. And, while Mac OS X is a Unix-type operating
system, it is not the same as Linux, and thus will not match with
machines running Linux.

While the analyzer can diagnose most common problems, there are some
situations that it cannot reliably detect due to the instantaneous and
local nature of the information it uses to detect the problem. Thus, it
may be that the analyzer reports that resources are available to service
the request, but the job still has not run. In most of these situations,
the delay is transient, and the job will run following the next
negotiation cycle.

A second class of problems represents jobs that do or did run, for at
least a short while, but are no longer running. The first issue is
identifying whether the job is in this category. The *condor\_q* command
is not enough; it only tells the current state of the job. The needed
information will be in the **log** file or the **error** file, as
defined in the submit description file for the job. If these files are
not defined, then there is little hope of determining if the job ran at
all. For a job that ran, even for the briefest amount of time, the
**log** file will contain an event of type 1, which will contain the
string Job executing on host.

A job may run for a short time, before failing due to a file permission
problem. The log file used by the *condor\_shadow* daemon will contain
more information if this is the problem. This log file is associated
with the machine on which the job was submitted. The location and name
of this log file may be discovered on the submitting machine, using the
command

::

    %  condor_config_val SHADOW_LOG

Memory and swap space problems may be identified by looking at the log
file used by the *condor\_schedd* daemon. The location and name of this
log file may be discovered on the submitting machine, using the command

::

    %  condor_config_val SCHEDD_LOG

A swap space problem will show in the log with the following message:

::

    2/3 17:46:53 Swap space estimate reached! No more jobs can be run! 
    12/3 17:46:53     Solution: get more swap space, or set RESERVED_SWAP = 0 
    12/3 17:46:53     0 jobs matched, 1 jobs idle

As an explanation, HTCondor computes the total swap space on the submit
machine. It then tries to limit the total number of jobs it will spawn
based on an estimate of the size of the *condor\_shadow* daemon’s memory
footprint and a configurable amount of swap space that should be
reserved. This is done to avoid the situation within a very large pool
in which all the jobs are submitted from a single host. The huge number
of *condor\_shadow* processes would overwhelm the submit machine, and it
would run out of swap space and thrash.

Things can go wrong if a machine has a lot of physical memory and little
or no swap space. HTCondor does not consider the physical memory size,
so the situation occurs where HTCondor thinks it has no swap space to
work with, and it will not run the submitted jobs.

To see how much swap space HTCondor thinks a given machine has, use the
output of a *condor\_status* command of the following form:

::

    % condor_status -schedd [hostname] -long | grep VirtualMemory

If the value listed is 0, then this is what is confusing HTCondor. There
are two ways to fix the problem:

#. Configure the machine with some real swap space.
#. Disable this check within HTCondor. Define the amount of reserved
   swap space for the submit machine to 0. Set ``RESERVED_SWAP`` to 0 in
   the configuration file:

   ::

       RESERVED_SWAP = 0

   and then send a *condor\_restart* to the submit machine.

Job in the Hold State
---------------------

A variety of errors and unusual conditions may cause a job to be placed
into the Hold state. The job will stay in this state and in the job
queue until conditions are corrected and *condor\_release* is invoked.

A table listing the reasons why a job may be held is at
section \ `A.2 <JobClassAdAttributes.html#x170-1234000A.2>`__. A string
identifying the reason that a particular job is in the Hold state may be
displayed by invoking *condor\_q*. For the example job ID 16.0, use:

::

      condor_q  -hold  16.0

This command prints information about the job, including the job ClassAd
attribute ``HoldReason``.

In the Job Event Log File
-------------------------

In a job event log file are a listing of events in chronological order
that occurred during the life of one or more jobs. The formatting of the
events is always the same, so that they may be machine readable. Four
fields are always present, and they will most often be followed by other
fields that give further information that is specific to the type of
event.

The first field in an event is the numeric value assigned as the event
type in a 3-digit format. The second field identifies the job which
generated the event. Within parentheses are the job ClassAd attributes
of ``ClusterId`` value, ``ProcId`` value, and the node number for
parallel universe jobs or a set of zeros (for jobs run under all other
universes), separated by periods. The third field is the date and time
of the event logging. The fourth field is a string that briefly
describes the event. Fields that follow the fourth field give further
information for the specific event type.

These are all of the events that can show up in a job log file:

| **Event Number:** 000
| **Event Name:** Job submitted
| **Event Description:** This event occurs when a user submits a job. It
is the first event you will see for a job, and it should only occur
once.

| **Event Number:** 001
| **Event Name:** Job executing
| **Event Description:** This shows up when a job is running. It might
occur more than once.

| **Event Number:** 002
| **Event Name:** Error in executable
| **Event Description:** The job could not be run because the executable
was bad.

| **Event Number:** 003
| **Event Name:** Job was checkpointed
| **Event Description:** The job’s complete state was written to a
checkpoint file. This might happen without the job being removed from a
machine, because the checkpointing can happen periodically.

| **Event Number:** 004
| **Event Name:** Job evicted from machine
| **Event Description:** A job was removed from a machine before it
finished, usually for a policy reason. Perhaps an interactive user has
claimed the computer, or perhaps another job is higher priority.

| **Event Number:** 005
| **Event Name:** Job terminated
| **Event Description:** The job has completed.

| **Event Number:** 006
| **Event Name:** Image size of job updated
| **Event Description:** An informational event, to update the amount of
memory that the job is using while running. It does not reflect the
state of the job.

| **Event Number:** 007
| **Event Name:** Shadow exception
| **Event Description:** The *condor\_shadow*, a program on the submit
computer that watches over the job and performs some services for the
job, failed for some catastrophic reason. The job will leave the machine
and go back into the queue.

| **Event Number:** 008
| **Event Name:** Generic log event
| **Event Description:** Not used.

| **Event Number:** 009
| **Event Name:** Job aborted
| **Event Description:** The user canceled the job.

| **Event Number:** 010
| **Event Name:** Job was suspended
| **Event Description:** The job is still on the computer, but it is no
longer executing. This is usually for a policy reason, such as an
interactive user using the computer.

| **Event Number:** 011
| **Event Name:** Job was unsuspended
| **Event Description:** The job has resumed execution, after being
suspended earlier.

| **Event Number:** 012
| **Event Name:** Job was held
| **Event Description:** The job has transitioned to the hold state.
This might happen if the user applies the *condor\_hold* command to the
job.

| **Event Number:** 013
| **Event Name:** Job was released
| **Event Description:** The job was in the hold state and is to be
re-run.

| **Event Number:** 014
| **Event Name:** Parallel node executed
| **Event Description:** A parallel universe program is running on a
node.

| **Event Number:** 015
| **Event Name:** Parallel node terminated
| **Event Description:** A parallel universe program has completed on a
node.

| **Event Number:** 016
| **Event Name:** POST script terminated
| **Event Description:** A node in a DAGMan work flow has a script that
should be run after a job. The script is run on the submit host. This
event signals that the post script has completed.

| **Event Number:** 017
| **Event Name:** Job submitted to Globus
| **Event Description:** A grid job has been delegated to Globus
(version 2, 3, or 4). This event is no longer used.

| **Event Number:** 018
| **Event Name:** Globus submit failed
| **Event Description:** The attempt to delegate a job to Globus failed.

| **Event Number:** 019
| **Event Name:** Globus resource up
| **Event Description:** The Globus resource that a job wants to run on
was unavailable, but is now available. This event is no longer used.

| **Event Number:** 020
| **Event Name:** Detected Down Globus Resource
| **Event Description:** The Globus resource that a job wants to run on
has become unavailable. This event is no longer used.

| **Event Number:** 021
| **Event Name:** Remote error
| **Event Description:** The *condor\_starter* (which monitors the job
on the execution machine) has failed.

| **Event Number:** 022
| **Event Name:** Remote system call socket lost
| **Event Description:** The *condor\_shadow* and *condor\_starter*
(which communicate while the job runs) have lost contact.

| **Event Number:** 023
| **Event Name:** Remote system call socket reestablished
| **Event Description:** The *condor\_shadow* and *condor\_starter*
(which communicate while the job runs) have been able to resume contact
before the job lease expired.

| **Event Number:** 024
| **Event Name:** Remote system call reconnect failure
| **Event Description:** The *condor\_shadow* and *condor\_starter*
(which communicate while the job runs) were unable to resume contact
before the job lease expired.

| **Event Number:** 025
| **Event Name:** Grid Resource Back Up
| **Event Description:** A grid resource that was previously unavailable
is now available.

| **Event Number:** 026
| **Event Name:** Detected Down Grid Resource
| **Event Description:** The grid resource that a job is to run on is
unavailable.

| **Event Number:** 027
| **Event Name:** Job submitted to grid resource
| **Event Description:** A job has been submitted, and is under the
auspices of the grid resource.

| **Event Number:** 028
| **Event Name:** Job ad information event triggered.
| **Event Description:** Extra job ClassAd attributes are noted. This
event is written as a supplement to other events when the configuration
parameter ``EVENT_LOG_JOB_AD_INFORMATION_ATTRS`` is set.

| **Event Number:** 029
| **Event Name:** The job’s remote status is unknown
| **Event Description:** No updates of the job’s remote status have been
received for 15 minutes.

| **Event Number:** 030
| **Event Name:** The job’s remote status is known again
| **Event Description:** An update has been received for a job whose
remote status was previous logged as unknown.

| **Event Number:** 031
| **Event Name:** Job stage in
| **Event Description:** A grid universe job is doing the stage in of
input files.

| **Event Number:** 032
| **Event Name:** Job stage out
| **Event Description:** A grid universe job is doing the stage out of
output files.

| **Event Number:** 033
| **Event Name:** Job ClassAd attribute update
| **Event Description:** A Job ClassAd attribute is changed due to
action by the *condor\_schedd* daemon. This includes changes by
*condor\_prio*.

| **Event Number:** 034
| **Event Name:** Pre Skip event
| **Event Description:** For DAGMan, this event is logged if a PRE
SCRIPT exits with the defined PRE\_SKIP value in the DAG input file.
This makes it possible for DAGMan to do recovery in a workflow that has
such an event, as it would otherwise not have any event for the DAGMan
node to which the script belongs, and in recovery, DAGMan’s internal
tables would become corrupted.

| **Event Number:** 035
| **Event Name:** Factory Submit
| **Event Description:** This event occurs when a user submits a cluster
using late materialization.

| **Event Number:** 036
| **Event Name:** Cluster Removed
| **Event Description:** Only written for clusters using late
materialization. This event occurs after all the jobs in a cluster
submitted using late materialization have materialized and completed, or
when the cluster is removed (by *condor\_rm*).

| **Event Number:** 037
| **Event Name:** Factory Paused
| **Event Description:** This event occurs when job materialization for
a cluster has been paused.

| **Event Number:** 038
| **Event Name:** Factory Resumed
| **Event Description:** This event occurs when job materialization for
a cluster has been resumed

| **Event Number:** 039
| **Event Name:** None
| **Event Description:** This event should never occur in a log but may
be returned by log reading code in certain situations (e.g., timing out
while waiting for a new event to appear in the log).

Job Completion
--------------

When an HTCondor job completes, either through normal means or by
abnormal termination by signal, HTCondor will remove it from the job
queue. That is, the job will no longer appear in the output of
*condor\_q*, and the job will be inserted into the job history file.
Examine the job history file with the *condor\_history* command. If
there is a log file specified in the submit description file for the
job, then the job exit status will be recorded there as well.

By default, HTCondor does not send an email message when the job
completes. Modify this behavior with the **notification** command in the
submit description file. The message will include the exit status of the
job, which is the argument that the job passed to the exit system call
when it completed, or it will be notification that the job was killed by
a signal. Notification will also include the following statistics (as
appropriate) about the job:

 Submitted at:
    when the job was submitted with *condor\_submit*
 Completed at:
    when the job completed
 Real Time:
    the elapsed time between when the job was submitted and when it
    completed, given in a form of ``<days> <hours>:<minutes>:<seconds>``
 Virtual Image Size:
    memory size of the job, computed when the job checkpoints

Statistics about just the last time the job ran:

 Run Time:
    total time the job was running, given in the form
    ``<days> <hours>:<minutes>:<seconds>``
 Remote User Time:
    total CPU time the job spent executing in user mode on remote
    machines; this does not count time spent on run attempts that were
    evicted without a checkpoint. Given in the form
    ``<days> <hours>:<minutes>:<seconds>``
 Remote System Time:
    total CPU time the job spent executing in system mode (the time
    spent at system calls); this does not count time spent on run
    attempts that were evicted without a checkpoint. Given in the form
    ``<days> <hours>:<minutes>:<seconds>``

The Run Time accumulated by all run attempts are summarized with the
time given in the form ``<days> <hours>:<minutes>:<seconds>``.

And, statistics about the bytes sent and received by the last run of the
job and summed over all attempts at running the job are given.

      
