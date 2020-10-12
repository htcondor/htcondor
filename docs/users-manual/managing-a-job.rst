Managing a Job
==============

This section provides a brief summary of what can be done once jobs are
submitted. The basic mechanisms for monitoring a job are introduced, but
the commands are discussed briefly. You are encouraged to look at the
man pages of the commands referred to (located in :doc:`/man-pages/index`)
for more information.

Checking on the progress of jobs
--------------------------------

You can check on your jobs with the *condor_q*
command. :index:`condor_q<single: condor_q; HTCondor commands>`\ This
command has many options, by default, it displays only your jobs
queued in the local scheduler. An example of the output from *condor_q* is


.. code-block:: console

    $ condor_q

    -- Schedd: submit.chtc.wisc.edu : <127.0.0.1:9618?... @ 12/31/69 23:00:00
    OWNER    BATCH_NAME    SUBMITTED   DONE   RUN    IDLE   HOLD  TOTAL JOB_IDS
    nemo     batch23       4/22 20:44      _      _      _      1      _ 3671850.0
    nemo     batch24       4/22 20:56      _      _      _      1      _ 3673477.0
    nemo     batch25       4/22 20:57      _      _      _      1      _ 3673728.0
    nemo     batch26       4/23 10:44      _      _      _      1      _ 3750339.0
    nemo     batch27       7/2  15:11      _      _      _      _      _ 7594591.0
    nemo     batch28       7/10 03:22   4428      3      _      _   4434 7801943.0 ... 7858552.0
    nemo     batch29       7/14 14:18   5074   1182     30     19  80064 7859129.0 ... 7885217.0
    nemo     batch30       7/14 14:18   5172   1088     28     30  58310 7859106.0 ... 7885192.0

    2388 jobs; 0 completed, 1 removed, 58 idle, 2276 running, 53 held, 0 suspended

The goal of the HTCondor system is to effectively manage many jobs. As you may have thousands
of jobs in a queue, by default *condor_q* summarizes many similiar jobs on one line.  Depending
on the types of your jobs, this output may look a little different.

Often, when you are starting out, and have few jobs, you may want to see one line of output
per job.  The -nobatch option to *condor_q* does this, and output might look something like:

.. code-block:: console

    $ condor_q -nobatch

    -- Schedd submit.chtc.wisc.edu : <127.0.0.1:9618?...
    ID          OWNER        SUBMITTED     RUN_TIME ST PRI SIZE CMD
    1297254.0   nemo         5/31 18:05  14+17:40:01 R  0   7.3  condor_dagman
    1297255.0   nemo         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman
    1297256.0   nemo         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman
    1297259.0   nemo         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman
    1297261.0   nemo         5/31 18:05  14+17:39:55 R  0   7.3  condor_dagman
    1302278.0   nemo         6/4  12:22   1+00:05:37 I  0   390.6 mdrun_1.sh
    1304740.0   nemo         6/5  00:14   1+00:03:43 I  0   390.6 mdrun_1.sh
    1304967.0   nemo         6/5  05:08   0+00:00:00 I  0   0.0  mdrun_1.sh

    14 jobs; 4 idle, 8 running, 2 held

This still only shows your jobs.  You can display information about all the users
with jobs in this scheduler by adding the -allusers option to *condor_q*.

The output contains many columns of information about the queued jobs.
:index:`of queued jobs<single: of queued jobs; status>`\ :index:`state<single: state; job>` The
ST column (for status) shows the status of current jobs in the queue:

R
    The job is currently running.
I
    The job is idle. It is not running right now, because it is
    waiting for a machine to become available.
H
    The job is the hold state. In the hold state, the job will not be
    scheduled to run until it is released. See the :doc:`/man-pages/condor_hold`
    and the :doc:`/man-pages/condor_release` manual pages.

The RUN_TIME time reported for a job is the time that has been
committed to the job.

Another useful method of tracking the progress of jobs is through the
job event log. The specification of a ``log`` in the submit description
file causes the progress of the job to be logged in a file. Follow the
events by viewing the job event log file. Various events such as
execution commencement, checkpoint, eviction and termination are logged
in the file. Also logged is the time at which the event occurred.

When a job begins to run, HTCondor starts up a *condor_shadow* process
:index:`condor_shadow`\ :index:`condor_shadow<single: condor_shadow; remote system call>`
on the submit machine. The shadow process is the mechanism by which the
remotely executing jobs can access the environment from which it was
submitted, such as input and output files.

It is normal for a machine which has submitted hundreds of jobs to have
hundreds of *condor_shadow* processes running on the machine. Since the
text segments of all these processes is the same, the load on the submit
machine is usually not significant. If there is degraded performance,
limit the number of jobs that can run simultaneously by reducing the
``MAX_JOBS_RUNNING`` :index:`MAX_JOBS_RUNNING` configuration
variable.

You can also find all the machines that are running your job through the
*condor_status* command.
:index:`condor_status<single: condor_status; HTCondor commands>`\ For example, to find
all the machines that are running jobs submitted by
``breach@cs.wisc.edu``, type:

.. code-block:: console

    $ condor_status -constraint 'RemoteUser == "breach@cs.wisc.edu"'

    Name       Arch     OpSys        State      Activity   LoadAv Mem  ActvtyTime

    alfred.cs. INTEL    LINUX        Claimed    Busy       0.980  64    0+07:10:02
    biron.cs.w INTEL    LINUX        Claimed    Busy       1.000  128   0+01:10:00
    cambridge. INTEL    LINUX        Claimed    Busy       0.988  64    0+00:15:00
    falcons.cs INTEL    LINUX        Claimed    Busy       0.996  32    0+02:05:03
    happy.cs.w INTEL    LINUX        Claimed    Busy       0.988  128   0+03:05:00
    istat03.st INTEL    LINUX        Claimed    Busy       0.883  64    0+06:45:01
    istat04.st INTEL    LINUX        Claimed    Busy       0.988  64    0+00:10:00
    istat09.st INTEL    LINUX        Claimed    Busy       0.301  64    0+03:45:00
    ...

To find all the machines that are running any job at all, type:

.. code-block:: console

    $ condor_status -run

    Name       Arch     OpSys        LoadAv RemoteUser           ClientMachine

    adriana.cs INTEL    LINUX        0.980  hepcon@cs.wisc.edu   chevre.cs.wisc.
    alfred.cs. INTEL    LINUX        0.980  breach@cs.wisc.edu   neufchatel.cs.w
    amul.cs.wi X86_64   LINUX        1.000  nice-user.condor@cs. chevre.cs.wisc.
    anfrom.cs. X86_64   LINUX        1.023  ashoks@jules.ncsa.ui jules.ncsa.uiuc
    anthrax.cs INTEL    LINUX        0.285  hepcon@cs.wisc.edu   chevre.cs.wisc.
    astro.cs.w INTEL    LINUX        1.000  nice-user.condor@cs. chevre.cs.wisc.
    aura.cs.wi X86_64   WINDOWS      0.996  nice-user.condor@cs. chevre.cs.wisc.
    balder.cs. INTEL    WINDOWS      1.000  nice-user.condor@cs. chevre.cs.wisc.
    bamba.cs.w INTEL    LINUX        1.574  dmarino@cs.wisc.edu  riola.cs.wisc.e
    bardolph.c INTEL    LINUX        1.000  nice-user.condor@cs. chevre.cs.wisc.
    ...

Peeking in on a running job's output files
------------------------------------------

When a job is running, you may be curious about any output it has created.
The **condor_tail** command can copy output files from a running job on a remote
machine back to the submit machine.  **condor_tail** uses the same networking
stack as HTCondor proper, so it will work if the execute machine is behind a firewall.
Simply run, where xx.yy is the job id of a running job:

.. code-block:: console

    $ condor_tail xx.yy


or

.. code-block:: console

    $ condor_tail -f xx.yy

to continuously follow the standard output.  To copy a different file, run

.. code-block:: console

    $ condor_tail xx.yy name_of_output_file


Starting an interactive shell next to a running job on a remote machine
-----------------------------------------------------------------------

**condor_ssh_to_job** is a very powerful command, but is not available on
all platforms, or all installations.  Some administrators disable it, so check with
your local site if it does not appear to work.  **condor_ssh_to_job** takes the job
id of a running job as an argument, and establishes a shell running on the node
next to the job.  The environment of this shell is a similar to the job as possible.
Users of **condor_ssh_to_job** can look at files, attach to their job with the debugger
and otherwise inspect the job.

Removing a job from the queue
-----------------------------

A job can be removed from the queue at any time by using the
*condor_rm* :index:`condor_rm<single: condor_rm; HTCondor commands>`\ command. If
the job that is being removed is currently running, the job is killed
without a checkpoint, and its queue entry is removed. The following
example shows the queue of jobs before and after a job is removed.

.. code-block:: console

    $ condor_q -nobatch

    -- Schedd: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD
     125.0   raman           4/11 14:37   0+00:00:00 R  0   1.4  sleepy
     132.0   raman           4/11 16:57   0+00:00:00 R  0   1.4  hello

    2 jobs; 1 idle, 1 running, 0 held

    $ condor_rm 132.0
    Job 132.0 removed.

    $ condor_q -nobatch

    -- Schedd: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD
     125.0   raman           4/11 14:37   0+00:00:00 R  0   1.4  sleepy

    1 jobs; 1 idle, 0 running, 0 held

Placing a job on hold
---------------------

:index:`condor_hold<single: condor_hold; HTCondor commands>`
:index:`condor_release<single: condor_release; HTCondor commands>`
:index:`state<single: state; job>`

A job in the queue may be placed on hold by running the command
*condor_hold*. A job in the hold state remains in the hold state until
later released for execution by the command *condor_release*.

Use of the *condor_hold* command causes a hard kill signal to be sent
to a currently running job (one in the running state). 

Jobs that are running when placed on hold will start over from the 
beginning when released.

The :doc:`/man-pages/condor_hold` and the :doc:`/man-pages/condor_release`
manual pages contain usage details.

Changing the priority of jobs
-----------------------------

:index:`priority<single: priority; job>` :index:`of a job<single: of a job; priority>`

In addition to the priorities assigned to each user, HTCondor also
provides each user with the capability of assigning priorities to each
submitted job. These job priorities are local to each queue and can be
any integer value, with higher values meaning better priority.

The default priority of a job is 0, but can be changed using the
*condor_prio* command.
:index:`condor_prio<single: condor_prio; HTCondor commands>`\ For example, to change
the priority of a job to -15,

.. code-block:: console

    $ condor_q -nobatch raman

    -- Submitter: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD
     126.0   raman           4/11 15:06   0+00:00:00 I  0   0.3  hello

    1 jobs; 1 idle, 0 running, 0 held

    $ condor_prio -p -15 126.0

    $ condor_q -nobatch raman

    -- Submitter: froth.cs.wisc.edu : <128.105.73.44:33847> : froth.cs.wisc.edu
     ID      OWNER            SUBMITTED    CPU_USAGE ST PRI SIZE CMD
     126.0   raman           4/11 15:06   0+00:00:00 I  -15 0.3  hello

    1 jobs; 1 idle, 0 running, 0 held

It is important to note that these job priorities are completely
different from the user priorities assigned by HTCondor. Job priorities
do not impact user priorities. They are only a mechanism for the user to
identify the relative importance of jobs among all the jobs submitted by
the user to that specific queue.

Why is the job not running?
---------------------------

:index:`analysis<single: analysis; job>` :index:`not running<single: not running; job>`

Users occasionally find that their jobs do not run. There are many
possible reasons why a specific job is not running. The following prose
attempts to identify some of the potential issues behind why a job is
not running.

At the most basic level, the user knows the status of a job by using
*condor_q* to see that the job is not running. By far, the most common
reason (to the novice HTCondor job submitter) why the job is not running
is that HTCondor has not yet been through its periodic negotiation
cycle, in which queued jobs are assigned to machines within the pool and
begin their execution. This periodic event occurs by default once every
5 minutes, implying that the user ought to wait a few minutes before
searching for reasons why the job is not running.

Further inquiries are dependent on whether the job has never run at all,
or has run for at least a little bit.

For jobs that have never run,
:index:`condor_q<single: condor_q; HTCondor commands>`\ many problems can be
diagnosed by using the **-analyze** option of the *condor_q* command.
Here is an example; running *condor_q* 's analyzer provided the
following information:

.. code-block:: console

    $ condor_q -analyze 27497829

    -- Submitter: s1.chtc.wisc.edu : <128.104.100.43:9618?sock=5557_e660_3> : s1.chtc.wisc.edu
    User priority for ei@chtc.wisc.edu is not available, attempting to analyze without it.
    ---
    27497829.000:  Run analysis summary.  Of 5257 machines,
       5257 are rejected by your job's requirements
          0 reject your job because of their own requirements
          0 match and are already running your jobs
          0 match but are serving other users
          0 are available to run your job
            No successful match recorded.
            Last failed match: Tue Jun 18 14:36:25 2013

            Reason for last match failure: no match found

    WARNING:  Be advised:
       No resources matched request's constraints

    The Requirements expression for your job is:

        ( OpSys == "OSX" ) && ( TARGET.Arch == "X86_64" ) &&
        ( TARGET.Disk >= RequestDisk ) && ( TARGET.Memory >= RequestMemory ) &&
        ( ( TARGET.HasFileTransfer ) || ( TARGET.FileSystemDomain == MY.FileSystemDomain ) )


    Suggestions:
        Condition                         Machines Matched Suggestion
        ---------                         ---------------- ----------
    1   ( target.OpSys == "OSX" )         0                MODIFY TO "LINUX"
    2   ( TARGET.Arch == "X86_64" )       5190
    3   ( TARGET.Disk >= 1 )              5257
    4   ( TARGET.Memory >= ifthenelse(MemoryUsage isnt undefined,MemoryUsage,1) )
                                          5257
    5   ( ( TARGET.HasFileTransfer ) || ( TARGET.FileSystemDomain == "submit-1.chtc.wisc.edu" ) )
                                          5257

This example also shows that the job does not run because the platform
requested, Mac OS X, is not available on any of the machines in the
pool. Recall that unless informed otherwise in the
**Requirements** :index:`Requirements<single: Requirements; submit commands>`
expression in the submit description file, the platform requested for an
execute machine will be the same as the platform where *condor_submit*
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
identifying whether the job is in this category. The *condor_q* command
is not enough; it only tells the current state of the job. The needed
information will be in the **log** :index:`log<single: log; submit commands>`
file or the **error** :index:`error<single: error; submit commands>` file, as
defined in the submit description file for the job. If these files are
not defined, then there is little hope of determining if the job ran at
all. For a job that ran, even for the briefest amount of time, the
**log** :index:`log<single: log; submit commands>` file will contain an event
of type 1, which will contain the string Job executing on host.

A job may run for a short time, before failing due to a file permission
problem. The log file used by the *condor_shadow* daemon will contain
more information if this is the problem. This log file is associated
with the machine on which the job was submitted. The location and name
of this log file may be discovered on the submitting machine, using the
command

.. code-block:: console

    $ condor_config_val SHADOW_LOG

Job in the Hold State
---------------------

:index:`not running, on hold<single: not running, on hold; job>`

Should HTCondor detect something about a job that would prevent it
from ever running successfully, say, because the executable doesn't
exist, or input files are missing, HTCondor will put the job in Hold state.
A job in the Hold state will remain in the queue, and show up in the
output of the *condor_q* command, but is not eligible to run.
The job will stay in this state until it is released or removed.  Users
may also hold their jobs manually with the *condor_hold* command.

A table listing the reasons why a job may be held is at the
:doc:`/classad-attributes/job-classad-attributes` section. A
string identifying the reason that a particular job is in the Hold state
may be displayed by invoking *condor_q* -hold. For the example job ID 16.0,
use:

.. code-block:: console

      $ condor_q  -hold  16.0

This command prints information about the job, including the job ClassAd
attribute ``HoldReason``.

In the Job Event Log File
-------------------------

:index:`event log file<single: event log file; job>`
:index:`job event codes and descriptions<single: job event codes and descriptions; log files>`

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

A complete list of these values is at :doc:`/codes-other-values/job-event-log-codes` section.

Job Termination
---------------

:index:`termination<single: termination, job>`

From time to time, and for a variety of reasons, HTCondor may terminate
a job before it completes.  For instance, a job could be removed (via
*condor_rm*), preempted (by a user a with higher priority), or killed
(for using more memory than it requested).  In these cases, it might be
helpful to know why HTCondor terminated the job.  HTCondor calls its
records of these reasons "Tickets of Execution".

A ticket of execution is usually issued by the *condor_startd*, and
includes:

- when the *condor_startd* was told, or otherwise decided, to terminate the job
  (the ``when`` attribute);
- who made the decision to terminate, usually a Sinful string
  (the ``who`` attribute);
- and what method was employed to command the termination, as both as
  string and an integer (the ``How`` and ``HowCode`` attributes).

The relevant log events include a human-readable rendition of the ToE,
and the job ad is updated with the ToE after the usual delay.

As of version 8.9.4, HTCondor only issues ToE in three cases:

- when the job terminates of its own accord (issued by the starter,
  ``HowCode`` 0);
- and when the startd terminates the job because it received a
  ``DEACTIVATE_CLAIM`` commmand (``HowCode`` 1)
- or a ``DEACTIVATE_CLAIM_FORCIBLY`` command (``HowCode`` 2).

In both cases, HTCondor records the ToE in the job ad.  In the event
log(s), event 005 (job completion) includes the ToE for the first case,
and event 009 (job aborted) includes the ToE for the second and third cases.

Future HTCondor releases will issue ToEs in additional cases and include
them in additional log events.

Job Completion
--------------

:index:`completion<single: completion; job>`

When an HTCondor job completes, either through normal means or by
abnormal termination by signal, HTCondor will remove it from the job
queue. That is, the job will no longer appear in the output of
*condor_q*, and the job will be inserted into the job history file.
Examine the job history file with the *condor_history* command. If
there is a log file specified in the submit description file for the
job, then the job exit status will be recorded there as well, along with
other information described below.
:index:`notification<single: notification; submit commands>`

By default, HTCondor does not send an email message when the job
completes. Modify this behavior with the
**notification** :index:`notification<single: notification; submit commands>` command
in the submit description file. The message will include the exit status
of the job, which is the argument that the job passed to the exit system
call when it completed, or it will be notification that the job was
killed by a signal. Notification will also include the following
statistics (as appropriate) about the job:

 Submitted at:
    when the job was submitted with *condor_submit*
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

The job terminated event includes the following:

- the type of termination (normal or by signal)
- the return value (or signal number)
- local and remote usage for the last (most recent) run
  (in CPU-seconds)
- local and remote usage summed over all runs
  (in CPU-seconds)
- bytes sent and received by the job's last (most recent) run,
- bytes sent and received summed over all runs,
- a report on which partitionable resources were used, if any.  Resources
  include CPUs, disk, and memory; all are lifetime peak values.

Your administrator may have configured HTCondor to report on other resources,
particularly GPUs (lifetime average) and GPU memory usage (lifetime peak).
HTCondor currently assigns all the usage of a GPU to the job running in
the slot to which the GPU is assigned; if the admin allows more than one job
to run on the same GPU, or non-HTCondor jobs to use the GPU, GPU usage will be
misreported accordingly.

When configured to report GPU usage, HTCondor sets the following two
attributes in the job:

:index:`GPUsUsage<single: GPUsUsage; ClassAd job attribute>`
:index:`job ClassAd attribute<single: job ClassAd attribute; GPUsUsage>`

  ``GPUsUsage``
    GPU usage over the lifetime of the job, reported as a fraction of the
    the maximum possible utilization of one GPU.

:index:`GPUsMemoryUsage<single: GPUsMemoryUsage; ClassAd job attribute>`
:index:`job ClassAd attribute<single: job ClassAd attribute; GPUsMemoryUsage>`

  ``GPUsMemoryUsage``
    Peak memory usage over the lifetime of the job, in megabytes.

Summary of all HTCondor users and their jobs
--------------------------------------------
When jobs are submitted, HTCondor will attempt to find resources to run
the jobs. A list of all those with jobs submitted may be obtained
through *condor_status*
:index:`condor_status<single: condor_status; HTCondor commands>`\ with the *-submitters*
option. An example of this would yield output similar to:

.. code-block:: console

    $ condor_status -submitters

    Name                 Machine      Running IdleJobs HeldJobs

    ballard@cs.wisc.edu  bluebird.c         0       11        0
    nice-user.condor@cs. cardinal.c         6      504        0
    wright@cs.wisc.edu   finch.cs.w         1        1        0
    jbasney@cs.wisc.edu  perdita.cs         0        0        5

                               RunningJobs           IdleJobs           HeldJobs

     ballard@cs.wisc.edu                 0                 11                  0
     jbasney@cs.wisc.edu                 0                  0                  5
    nice-user.condor@cs.                 6                504                  0
      wright@cs.wisc.edu                 1                  1                  0

                   Total                 7                516                  5
