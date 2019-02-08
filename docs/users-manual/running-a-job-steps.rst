      

Running a Job: the Steps To Take
================================

The road to using HTCondor effectively is a short one. The basics are
quickly and easily learned.

Here are all the steps needed to run a job using HTCondor.

 Code Preparation.
    A job run under HTCondor must be able to run as a background batch
    job. HTCondor runs the program unattended and in the background. A
    program that runs in the background will not be able to do
    interactive input and output. HTCondor can redirect console output
    (``stdout`` and ``stderr``) and keyboard input (``stdin``) to and
    from files for the program. Create any needed files that contain the
    proper keystrokes needed for program input. Make certain the program
    will run correctly with the files.
 The HTCondor Universe.
    HTCondor has several runtime environments (called a universe) from
    which to choose. Of the universes, two are likely choices when
    learning to submit a job to HTCondor: the standard universe and the
    vanilla universe. The standard universe allows a job running under
    HTCondor to handle system calls by returning them to the machine
    where the job was submitted. The standard universe also provides the
    mechanisms necessary to take a checkpoint and migrate a partially
    completed job, should the machine on which the job is executing
    become unavailable. To use the standard universe, it is necessary to
    relink the program with the HTCondor library using the
    *condor\_compile* command. The manual page for *condor\_compile* on
    page \ `1829 <Condorcompile.html#x104-72500012>`__ has details.

    The vanilla universe provides a way to run jobs that cannot be
    relinked. There is no way to take a checkpoint or migrate a job
    executed under the vanilla universe. For access to input and output
    files, jobs must either use a shared file system, or use HTCondor’s
    File Transfer mechanism.

    Choose a universe under which to run the HTCondor program, and
    re-link the program if necessary.

 Submit description file.
    Controlling the details of a job submission is a submit description
    file. The file contains information about the job such as what
    executable to run, the files to use in place of ``stdin`` and
    ``stdout``, and the platform type required to run the program. The
    number of times to run a program may be included; it is simple to
    run the same program multiple times with multiple data sets.

    Write a submit description file to go with the job, using the
    examples provided in
    section \ `2.5 <SubmittingaJob.html#x17-280002.5>`__ for guidance.

 Submit the Job.
    Submit the program to HTCondor with the *condor\_submit* command.

Once submitted, HTCondor does the rest toward running the job. Monitor
the job’s progress with the *condor\_q* and *condor\_status* commands.
You may modify the order in which HTCondor will run your jobs with
*condor\_prio*. If desired, HTCondor can even inform you in a log file
every time your job is checkpointed and/or migrated to a different
machine.

When your program completes, HTCondor will tell you (by e-mail, if
preferred) the exit status of your program and various statistics about
its performances, including time used and I/O performed. If you are
using a log file for the job (which is recommended) the exit status will
be recorded in the log file. You can remove a job from the queue
prematurely with *condor\_rm*.

Choosing an HTCondor Universe
-----------------------------

A universe in HTCondor defines an execution environment. HTCondor
Version 8.9.1 supports several different universes for user jobs:

-  standard
-  vanilla
-  grid
-  java
-  scheduler
-  local
-  parallel
-  vm
-  docker

The **universe** under which a job runs is specified in the submit
description file. If a universe is not specified, the default is
vanilla, unless your HTCondor administrator has changed the default.
However, we strongly encourage you to specify the universe, since the
default can be changed by your HTCondor administrator, and the default
that ships with HTCondor has changed.

The standard universe provides migration and reliability, but has some
restrictions on the programs that can be run. The vanilla universe
provides fewer services, but has very few restrictions. The grid
universe allows users to submit jobs using HTCondor’s interface. These
jobs are submitted for execution on grid resources. The java universe
allows users to run jobs written for the Java Virtual Machine (JVM). The
scheduler universe allows users to submit lightweight jobs to be spawned
by the program known as a daemon on the submit host itself. The parallel
universe is for programs that require multiple machines for one job. See
section \ `2.9 <ParallelApplicationsIncludingMPIApplications.html#x21-700002.9>`__
for more about the Parallel universe. The vm universe allows users to
run jobs where the job is no longer a simple executable, but a disk
image, facilitating the execution of a virtual machine. The docker
universe runs a Docker container as an HTCondor job.

Standard Universe
~~~~~~~~~~~~~~~~~

In the standard universe, HTCondor provides checkpointing and remote
system calls. These features make a job more reliable and allow it
uniform access to resources from anywhere in the pool. To prepare a
program as a standard universe job, it must be relinked with
*condor\_compile*. Most programs can be prepared as a standard universe
job, but there are a few restrictions.

HTCondor checkpoints a job at regular intervals. A checkpoint image is
essentially a snapshot of the current state of a job. If a job must be
migrated from one machine to another, HTCondor makes a checkpoint image,
copies the image to the new machine, and restarts the job continuing the
job from where it left off. If a machine should crash or fail while it
is running a job, HTCondor can restart the job on a new machine using
the most recent checkpoint image. In this way, jobs can run for months
or years even in the face of occasional computer failures.

Remote system calls make a job perceive that it is executing on its home
machine, even though the job may execute on many different machines over
its lifetime. When a job runs on a remote machine, a second process,
called a *condor\_shadow* runs on the machine where the job was
submitted. When the job attempts a system call, the *condor\_shadow*
performs the system call instead and sends the results to the remote
machine. For example, if a job attempts to open a file that is stored on
the submitting machine, the *condor\_shadow* will find the file, and
send the data to the machine where the job is running.

To convert your program into a standard universe job, you must use
*condor\_compile* to relink it with the HTCondor libraries. Put
*condor\_compile* in front of your usual link command. You do not need
to modify the program’s source code, but you do need access to the
unlinked object files. A commercial program that is packaged as a single
executable file cannot be converted into a standard universe job.

For example, if you would have linked the job by executing:

::

    % cc main.o tools.o -o program

Then, relink the job for HTCondor with:

::

    % condor_compile cc main.o tools.o -o program

There are a few restrictions on standard universe jobs:

#. Multi-process jobs are not allowed. This includes system calls such
   as ``fork()``, ``exec()``, and ``system()``.
#. Interprocess communication is not allowed. This includes pipes,
   semaphores, and shared memory.
#. Network communication must be brief. A job may make network
   connections using system calls such as ``socket()``, but a network
   connection left open for long periods will delay checkpointing and
   migration.
#. Sending or receiving the SIGUSR2 or SIGTSTP signals is not allowed.
   HTCondor reserves these signals for its own use. Sending or receiving
   all other signals is allowed.
#. Alarms, timers, and sleeping are not allowed. This includes system
   calls such as ``alarm()``, ``getitimer()``, and ``sleep()``.
#. Multiple kernel-level threads are not allowed. However, multiple
   user-level threads are allowed.
#. Memory mapped files are not allowed. This includes system calls such
   as ``mmap()`` and ``munmap()``.
#. File locks are allowed, but not retained between checkpoints.
#. All files must be opened read-only or write-only. A file opened for
   both reading and writing will cause trouble if a job must be rolled
   back to an old checkpoint image. For compatibility reasons, a file
   opened for both reading and writing will result in a warning but not
   an error.
#. A fair amount of disk space must be available on the submitting
   machine for storing a job’s checkpoint images. A checkpoint image is
   approximately equal to the virtual memory consumed by a job while it
   runs. If disk space is short, a special checkpoint server can be
   designated for storing all the checkpoint images for a pool.
#. On Linux, the job must be statically linked. *condor\_compile* does
   this by default.
#. Reading to or writing from files larger than 2 GBytes is only
   supported when the submit side *condor\_shadow* and the standard
   universe user job application itself are both 64-bit executables.

Vanilla Universe
~~~~~~~~~~~~~~~~

The vanilla universe in HTCondor is intended for programs which cannot
be successfully re-linked. Shell scripts are another case where the
vanilla universe is useful. Unfortunately, jobs run under the vanilla
universe cannot checkpoint or use remote system calls. This has
unfortunate consequences for a job that is partially completed when the
remote machine running a job must be returned to its owner. HTCondor has
only two choices. It can suspend the job, hoping to complete it at a
later time, or it can give up and restart the job from the beginning on
another machine in the pool.

Since HTCondor’s remote system call features cannot be used with the
vanilla universe, access to the job’s input and output files becomes a
concern. One option is for HTCondor to rely on a shared file system,
such as NFS or AFS. Alternatively, HTCondor has a mechanism for
transferring files on behalf of the user. In this case, HTCondor will
transfer any files needed by a job to the execution site, run the job,
and transfer the output back to the submitting machine.

Under Unix, HTCondor presumes a shared file system for vanilla jobs.
However, if a shared file system is unavailable, a user can enable the
HTCondor File Transfer mechanism. On Windows platforms, the default is
to use the File Transfer mechanism. For details on running a job with a
shared file system, see
section \ `2.5.8 <SubmittingaJob.html#x17-370002.5.8>`__ on
page \ `89 <SubmittingaJob.html#x17-370002.5.8>`__. For details on using
the HTCondor File Transfer mechanism, see
section \ `2.5.9 <SubmittingaJob.html#x17-380002.5.9>`__ on
page \ `91 <SubmittingaJob.html#x17-380002.5.9>`__.

Grid Universe
~~~~~~~~~~~~~

The Grid universe in HTCondor is intended to provide the standard
HTCondor interface to users who wish to start jobs intended for remote
management systems.
Section \ `5.3 <TheGridUniverse.html#x56-4540005.3>`__ on
page \ `1406 <TheGridUniverse.html#x56-4540005.3>`__ has details on
using the Grid universe. The manual page for *condor\_submit* on
page \ `2135 <Condorsubmit.html#x149-108000012>`__ has detailed
descriptions of the grid-related attributes.

Java Universe
~~~~~~~~~~~~~

A program submitted to the Java universe may run on any sort of machine
with a JVM regardless of its location, owner, or JVM version. HTCondor
will take care of all the details such as finding the JVM binary and
setting the classpath.

Scheduler Universe
~~~~~~~~~~~~~~~~~~

The scheduler universe allows users to submit lightweight jobs to be run
immediately, alongside the *condor\_schedd* daemon on the submit host
itself. Scheduler universe jobs are not matched with a remote machine,
and will never be preempted. The job’s requirements expression is
evaluated against the *condor\_schedd*\ ’s ClassAd.

Originally intended for meta-schedulers such as *condor\_dagman*, the
scheduler universe can also be used to manage jobs of any sort that must
run on the submit host.

However, unlike the local universe, the scheduler universe does not use
a *condor\_starter* daemon to manage the job, and thus offers limited
features and policy support. The local universe is a better choice for
most jobs which must run on the submit host, as it offers a richer set
of job management features, and is more consistent with other universes
such as the vanilla universe. The scheduler universe may be retired in
the future, in favor of the newer local universe.

Local Universe
~~~~~~~~~~~~~~

The local universe allows an HTCondor job to be submitted and executed
with different assumptions for the execution conditions of the job. The
job does not wait to be matched with a machine. It instead executes
right away, on the machine where the job is submitted. The job will
never be preempted. The job’s requirements expression is evaluated
against the *condor\_schedd*\ ’s ClassAd.

Parallel Universe
~~~~~~~~~~~~~~~~~

The parallel universe allows parallel programs, such as MPI jobs, to be
run within the opportunistic HTCondor environment. Please see
section \ `2.9 <ParallelApplicationsIncludingMPIApplications.html#x21-700002.9>`__
for more details.

VM Universe
~~~~~~~~~~~

HTCondor facilitates the execution of VMware and Xen virtual machines
with the vm universe.

Please see
section \ `2.11 <VirtualMachineApplications.html#x23-1160002.11>`__ for
details.

Docker Universe
~~~~~~~~~~~~~~~

The docker universe runs a docker container on an execute host as a job.
Please see
section \ `2.12 <DockerUniverseApplications.html#x24-1260002.12>`__ for
details.

      
