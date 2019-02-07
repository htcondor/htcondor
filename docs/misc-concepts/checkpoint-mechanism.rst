      

HTCondor’s Checkpoint Mechanism
===============================

A checkpoint is a snapshot of the current state of a program, taken in
such a way that the program can be restarted from that state at a later
time. Taking checkpoints gives the HTCondor scheduler the freedom to
reconsider scheduling decisions through preemptive-resume scheduling. If
the scheduler decides to no longer allocate a machine to a job (for
example, when the owner of that machine returns), it can take a
checkpoint of the job and preempt the job without losing the work the
job has already accomplished. The job can be resumed later when the
scheduler allocates it a new machine. Additionally, periodic checkpoints
provides fault tolerance in HTCondor. Snapshots are taken periodically,
and after an interruption in service the program can continue from the
most recent snapshot.

HTCondor provides checkpoint services to single process jobs on some
Unix platforms. To enable the taking of checkpoints, the user must link
the program with the HTCondor system call library
(``libcondorsyscall.a``), using the *condor\_compile* command. This
means that the user must have the object files or source code of the
program to use HTCondor checkpoints. However, the checkpoint services
provided by HTCondor are strictly optional. So, while there are some
classes of jobs for which HTCondor does not provide checkpoint services,
these jobs may still be submitted to HTCondor to take advantage of
HTCondor’s resource management functionality. See
section \ `2.4.1 <RunningaJobtheStepsToTake.html#x16-190002.4.1>`__ on
page \ `33 <RunningaJobtheStepsToTake.html#x16-190002.4.1>`__ for a
description of the classes of jobs for which HTCondor does not provide
checkpoint services.

The taking of process checkpoints is implemented in the HTCondor system
call library as a signal handler. When HTCondor sends a checkpoint
signal to a process linked with this library, the provided signal
handler writes the state of the process out to a file or a network
socket. This state includes the contents of the process stack and data
segments, all shared library code and data mapped into the process’s
address space, the state of all open files, and any signal handlers and
pending signals. On restart, the process reads this state from the file,
restoring the stack, shared library and data segments, file state,
signal handlers, and pending signals. The checkpoint signal handler then
returns to user code, which continues from where it left off when the
checkpoint signal arrived.

HTCondor processes for which the taking of checkpoints is enabled take a
checkpoint when preempted from a machine. When a suitable replacement
execution machine is found of the same architecture and operating
system, the process is restored on this new machine using the
checkpoint, and computation resumes from where it left off. Jobs that
can not take checkpoints are preempted and restarted from the beginning.

HTCondor’s taking of periodic checkpoints provides fault tolerance.
Pools may be configured with the ``PERIODIC_CHECKPOINT`` variable, which
controls when and how often jobs which can take and use checkpoints do
so periodically. Examples of when are never, and every three hours. When
the time to take a periodic checkpoint occurs, the job suspends
processing, takes the checkpoint, and immediately continues from where
it left off. There is also a *condor\_ckpt* command which allows the
user to request that an HTCondor job immediately take a periodic
checkpoint.

In all cases, HTCondor jobs continue execution from the most recent
complete checkpoint. If service is interrupted while a checkpoint is
being taken, causing that checkpoint to fail, the process will restart
from the previous checkpoint. HTCondor uses a commit style algorithm for
writing checkpoints: a previous checkpoint is deleted only after a new
complete checkpoint has been written successfully.

In certain cases, taking a checkpoint may be delayed until a more
appropriate time. For example, an HTCondor job will defer a checkpoint
request if it is communicating with another process over the network.
When the network connection is closed, the checkpoint will be taken.

The HTCondor checkpoint feature can also be used for any Unix process
outside of the HTCondor batch environment. Standalone checkpoints are
described in section \ `4.2.1 <#x49-4160004.2.1>`__.

HTCondor can produce and use compressed checkpoints. Configuration
variables (detailed in
section \ `3.5.10 <ConfigurationMacros.html#x33-1970003.5.10>`__ control
whether compression is used. The default is to not compress.

By default, a checkpoint is written to a file on the local disk of the
machine where the job was submitted. An HTCondor pool can also be
configured with a checkpoint server or servers that serve as a
repository for checkpoints, as described in
section \ `3.10 <TheCheckpointServer.html#x38-3250003.10>`__ on
page \ `1114 <TheCheckpointServer.html#x38-3250003.10>`__. When a host
is configured to use a checkpoint server, jobs submitted on that machine
write and read checkpoints to and from the server, rather than the local
disk of the submitting machine, taking the burden of storing checkpoint
files off of the submitting machines and placing it instead on server
machines (with disk space dedicated for the purpose of storing
checkpoints).

Standalone Checkpoint Mechanism
-------------------------------

Using the HTCondor checkpoint library without the remote system call
functionality and outside of the HTCondor system is known as the
standalone mode checkpoint mechanism.

To prepare a program for taking standalone checkpoints, use the
*condor\_compile* utility as for a standard HTCondor job, but do not use
*condor\_submit*. Run the program from the command line. The checkpoint
library will print a message to let you know that taking checkpoints is
enabled and to inform you of the default name for the checkpoint image.
The message is of the form:

::

    HTCondor: Notice: Will checkpoint to program_name.ckpt 
    HTCondor: Notice: Remote system calls disabled.

Platforms that use address space randomization will need a modified
invocation of the program, as described in
section \ `8.1.1 <Linux.html#x75-5720008.1.1>`__ on
page \ `1617 <Linux.html#x75-5720008.1.1>`__. The invocation disables
the address space randomization.

To force the program to write a checkpoint image and stop, send it the
SIGTSTP signal or press control-Z. To force the program to write a
checkpoint image and continue executing, send it the SIGUSR2 signal.

To restart a program using a checkpoint, invoke the program with the
command line argument *-\_condor\_restart*, followed by the name of the
checkpoint image file. As an example, if the program is called *P1* and
the checkpoint is called ``P1.ckpt``, use

::

    P1 -_condor_restart P1.ckpt

Again, platforms that implement address space randomization will need a
modified invocation, as described in
section \ `8.1.1 <Linux.html#x75-5720008.1.1>`__.

By default, the program will restart in the same directory in which it
originally ran, and the program will fail if it can not change to that
absolute path. To suppress this behavior, also pass the
*-\_condor\_relocatable* argument to the program. Not all programs will
continue to work. Doing this may simplify moving standalone checkpoints
between machines. Continuing the example given above, the command would
be

::

    P1 -_condor_restart P1.ckpt -_condor_relocatable

Checkpoint Safety
-----------------

Some programs have fundamental limitations that make them unsafe for
taking checkpoints. For example, a program that both reads and writes a
single file may enter an unexpected state. Here is an example of the
ordered events that exhibit this issue.

#. Record a checkpoint image.
#. Read data from a file.
#. Write data to the same file.
#. Execution failure, so roll back to step 2.

In this example, the program would re-read data from the file, but
instead of finding the original data, would see data created in the
future, and yield unexpected results.

To prevent this sort of accident, HTCondor displays a warning if a file
is used for both reading and writing. You can ignore or disable these
warnings if you choose as described in section
`4.2.3 <#x49-4180004.2.3>`__, but please understand that your program
may compute incorrect results.

Checkpoint Warnings
-------------------

HTCondor displays warning messages upon encountering unexpected
behaviors in the program. For example, if file ``x`` is opened for
reading and writing, this message will be displayed:

::

    HTCondor: Warning: READWRITE: File '/tmp/x' used for both reading and writing.

Control how these messages are displayed with the -\_condor\_warning
command line argument. This argument accepts a warning category and a
mode. The category describes a certain class of messages, such as
READWRITE or ALL. The mode describes what to do with the category. It
may be ON, OFF, or ONCE. If a category is ON, it is always displayed. If
a category is OFF, it is never displayed. If a category is ONCE, it is
displayed only once. To show all the available categories and modes, use
-\_condor\_warning with no arguments.

For example, the additional command line argument to limit read/write
warnings to one instance is

::

    -_condor_warning READWRITE ONCE

To turn all ordinary notices off:

::

    -_condor_warning NOTICE OFF

The same effect can be accomplished within a program by using the
function \_condor\_warning\_config().

Checkpoint Library Interface
----------------------------

A program need not be rewritten to take advantage of checkpoints.
However, the checkpoint library provides several C entry points that
allow for a program to control its own checkpoint behavior. These
functions are provided.

-  ``void init_image_with_file_name( char *ckpt_file_name )``
   This function explicitly sets a file name to use when producing or
   using a checkpoint. ckpt() or ckpt\_and\_exit() must be called to
   produce the checkpoint, and restart() must be called to perform the
   actual restart.
-  ``void init_image_with_file_descriptor( int fd )``
   This function explicitly sets a file descriptor to use when producing
   or using a checkpoint. ckpt() or ckpt\_and\_exit() must be called to
   produce the checkpoint, and restart() must be called to perform the
   actual restart.
-  ``void ckpt()``
   This function causes a checkpoint image to be written to disk. The
   program will continue to execute. This is identical to sending the
   program a SIGUSR2 signal.
-  ``void ckpt_and_exit()``
   This function causes a checkpoint image to be written to disk. The
   program will then exit. This is identical to sending the program a
   SIGTSTP signal.
-  ``void restart()``
   This function causes the program to read the checkpoint image and to
   resume execution of the program from the point where the checkpoint
   was taken. This function does not return.
-  ``void _condor_ckpt_disable()``
   This function temporarily disables the taking of checkpoints. This
   can be handy if the program does something that is not
   checkpoint-safe. For example, if a program must not be interrupted
   while accessing a special file, call \_condor\_ckpt\_disable(),
   access the file, and then call \_condor\_ckpt\_enable(). Some program
   actions, such as opening a socket or a pipe, implicitly cause the
   taking of checkpoints to be disabled.
-  ``void _condor_ckpt_enable()``
   This function re-enables the taking of checkpoints after a call to
   \_condor\_ckpt\_disable(). If a checkpoint signal arrived while the
   taking of checkpoints was disabled, the checkpoint will be taken when
   this function is called. Disabling and enabling the taking of
   checkpoints must occur in matched pairs. \_condor\_ckpt\_enable()
   must be called once for every time that \_condor\_ckpt\_disable() is
   called.
-  ``int _condor_warning_config( const char *kind, const char *mode )``
   This function controls what warnings are displayed by HTCondor. The
   ``kind`` and ``mode`` arguments are the same as for the
   ``-_condor_warning`` option described in section
   `4.2.3 <#x49-4180004.2.3>`__. This function returns ``true`` if the
   arguments are understood and accepted. Otherwise, it returns
   ``false``.
-  ``extern int condor_compress_ckpt``
   Setting this variable to 1 (one) causes checkpoint images to be
   compressed. Setting it to 0 (zero) disables compression.

      
