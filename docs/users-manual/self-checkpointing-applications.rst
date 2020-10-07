Self-Checkpointing Applications
===============================

:index:`Self-Checkpointing`

This section is about writing jobs for an executable which periodically
saves checkpoint information, and how to make HTCondor store that
information safely, in case it's needed to continue the job on another
machine or at a later time.

This section is *not* about how to checkpoint a given executable; that's
up to you or your software provider.

How To Run Self-Checkpointing Jobs
----------------------------------

The best way to run self-checkpointing code is to set ``checkpoint_exit_code``
in your submit file.  (Any exit code will work, but if you can choose,
consider error code ``85``.  On Linux systems, this is ``ERESTART``, which
seems appropriate.)  If the ``executable`` exits
with ``checkpoint_exit_code``, HTCondor will transfer the checkpoint to
the submit node, and then immediately restart the ``executable`` in the
same sandbox on the same machine, with same the ``arguments``.  This
immediate transfer makes the checkpoint available for continuing the job
even if the job is interrupted in a way that doesn't allow for files to
be transferred (e.g., power failure), or if the file transfer doesn't
complete in the time allowed.

For a job to use ``checkpoint_exit_code`` successfully, its ``executable``
must meet a number of requirements.

Requirements
------------

Your self-checkpointing code may not meet all of the following
requirements. In many cases, however, you will be able to add a wrapper
script, or modify an existing one, to meet these requirements. (Thus,
your ``executable`` may be a script, rather than the code that's writing
the checkpoint.) If you can not, consult `Working Around the
Assumptions`_ and/or the `Other Options`_.

#. Your executable exits after taking a checkpoint with an exit code it
   does not otherwise use.

   -  If your executable does not exit when it takes a checkpoint,
      HTCondor will not transfer its checkpoint. If your executable
      exits normally when it takes a checkpoint, HTCondor will not be
      able to tell the difference between taking a checkpoint and
      actually finishing; that is, if the checkpoint code and the
      terminal exit code are the same, your job will never finish.

#. When restarted, your executable determines on its own if a checkpoint
   is available, and if so, uses it.

   -  If your job does not look for a checkpoint each time it starts up,
      it will start from scratch each time; HTCondor does not run a
      different command line when restarting a job which has taken a
      checkpoint.

#. Starting your executable up from a checkpoint is relatively quick.

   -  If starting your executable up from a checkpoint is relatively
      slow, your job may not run efficiently enough to be useful,
      depending on the frequency of checkpoints and interruptions.

Using checkpoint_exit_code
--------------------------

The following Python script (``example.py``) is a toy example of code that
checkpoints itself. It counts from 0 to 10 (exclusive), sleeping for 10
seconds at each step. It writes a checkpoint file (containing the next number)
after each nap, and exits with code 85 at count 3, 6, and 9. It exits
with code 0 when complete.

.. code-block:: python

    #!/usr/bin/env python

    import sys
    import time

    value = 0
    try:
        with open('example.checkpoint', 'r') as f:
            value = int(f.read())
    except IOError:
        pass

    print("Starting from {0}".format(value))
    for i in range(value,10):
        print("Computing timestamp {0}".format(value))
        time.sleep(10)
        value += 1
        with open('example.checkpoint', 'w') as f:
            f.write("{0}".format(value))
        if value%3 == 0:
            sys.exit(85)

    print("Computation complete")
    sys.exit(0)

The following submit file (``example.submit``) commands HTCondor to transfer the
file ``example.checkpoint`` to the submit node whenever the script exits with code
85.  If interrupted, the job will resume from the most recent of those
checkpoints.  Before version 8.9.8, you *must* include your checkpoint file(s)
in ``transfer_output_files``; otherwise HTCondor will not transfer it
(them).  Starting with version 8.9.8, you may instead use
``transfer_checkpoint_files``, as documented on
the :doc:`/man-pages/condor_submit` man page.

.. code-block:: condor-submit

    checkpoint_exit_code        = 85
    transfer_output_files       = example.checkpoint
    should_transfer_files       = yes

    executable                  = example.py
    arguments                   =

    output                      = example.out
    error                       = example.err
    log                         = example.log

    queue 1

This example does not remove the "checkpoint file" generated for
timestep 9 when the executable completes.  This could be done in
``example.py`` immediately before it exits, but that would cause the
final file transfer to fail, if you specified the file in
``transfer_output_files``.  The script could instead remove the file
and then re-create it empty, it desired.

How Frequently to Checkpoint
----------------------------

Obviously, the longer the code spends writing checkpoints, and the
longer your job spends transferring them, the longer it will take for
you to get the job's results. Conversely, the more frequently the job
transfers new checkpoints, the less time the job loses if it's
interrupted. For most users and for most jobs, taking a checkpoint about
once an hour works well, and it's not a bad duration to start
experimenting with. A number of factors will skew this interval up or
down:

-  If your job(s) usually run on resources with strict time limits, you
   may want to adjust how often your job checkpoints to minimize wasted
   time. For instance, if your job writes a checkpoint after each hour,
   and each checkpoint takes five minutes to write out and then
   transfer, your fifth checkpoint will finish twenty-five minutes into
   the fifth hour, and you won't gain any benefit from the next
   thirty-five minutes of computation. If you instead write a checkpoint
   every eighty-four minutes, your job will only waste four minutes.
-  If a particular code writes larger checkpoints, or writes smaller
   checkpoints unusually slowly, you may want to take a checkpoint less
   frequently than you would for other jobs of a similar length, to keep
   the total overhead (delay) the same. The opposite is also true: if
   the job can take checkpoints particularly quickly, or the checkpoints
   are particularly small, the job could checkpoint more often for the
   same amount of overhead.
-  Some code naturally checkpoints at longer or shorter intervals. If a
   code writes a checkpoint every five minutes, it may make sense for
   the ``executable`` to wait for the code to write ten or more
   checkpoints before exiting (which asks HTCondor to transfer the
   checkpoint file(s)). If a job is a sequence of steps, the natural (or
   only possible) checkpoint interval may be between steps.
-  How long it takes to restart from a checkpoint. It should never take
   longer to restart from a checkpoint than to recompute from the
   beginning, but the restart process is part of the overhead of taking
   a checkpoint. The longer a code takes to restart, the less often the
   ``executable`` should exit.

Measuring how long it takes to make checkpoints is left as an exercise
for the reader. Since version 8.9.1, however, HTCondor will report in
the job's log (if a log is enabled for that job) how long file
transfers, including checkpoint transfers, took.

Debugging Self-Checkpointing Jobs
---------------------------------

Because a job may be interrupted at any time, it's valid to interrupt
the job at any time and see if a valid checkpoint is transferred. To do
so, use ``condor_vacate_job`` to evict the job. When that's done (watch
the user log), use ``condor_hold`` to put it on hold, so that it can't
restart while you're looking at the checkpoint (and potentially,
overwrite it). Finally, to obtain the checkpoint file(s) themselves, use
``condor_config_val`` to ask where ``SPOOL`` is, and then look in
appropriate subdirectory (named after the job ID).

For example, if your job is ID 635.0, and is logging to the file
``job.log``, you can copy the files in the checkpoint to a subdirectory of
the current as follows:

.. code-block:: console

    $ condor_vacate_job 635.0

Wait for the job to finish being evicted;
hit CTRL-C when you see 'Job was evicted.'
and immediately hold the job.

.. code-block:: console

    $ tail --follow job.log
    $ condor_hold 635.0

Copy the checkpoint files from the spool.
Note that _condor_stderr and _condor_stdout are the files corresponding
to the job's output and error submit commands; they aren't named
correctly until the the job finishes.

.. code-block:: console

    $ cp -a `condor_config_val SPOOL`/635/0/cluster635.proc0.subproc0 .

Now examine the checkpoint files to see if they look right.
When you're done, release the job to see if it actually works right.

.. code-block:: console

    $ condor_release 635.0
    $ condor_ssh_to_job 635.0

Working Around the Assumptions
------------------------------

The basic technique here is to write a wrapper script (or modify an
existing one), so that the ``executable`` has the necessary behavior,
even if the code does not.

#. *Your executable exits after taking a checkpoint with an exit code it
   does not otherwise use.*

   -  If your code exits when it takes a checkpoint, but not with a
      unique code, your wrapper script will have to determine, when the
      executable exits, if it did so because it took a checkpoint. If
      so, the wrapper script will have to exit with a unique code. If
      the code could usefully exit with any code, and the wrapper script
      therefore can not exit with a unique code, you can instead
      instruct HTCondor to consider being killed by a particular signal as
      a sign of successful checkpoint; set
      ``+SuccessCheckpointExitBySignal`` to ``TRUE`` and
      ``+SuccessCheckpointExitSignal`` to the particular signal. (If you
      do not set ``checkpoint_exit_code``, you must set
      ``+WantFTOnCheckpoint``.)
   -  If your code does not exit when it takes a checkpoint, the wrapper
      script will have to determine when a checkpoint has been made,
      kill the program, and then exit with a unique code.

#. *When restarted, your executable determines on its own if a
   checkpoint is available, and if so, uses it.*

   -  If your code requires different arguments to start from a
      checkpoint, the wrapper script must check for the presence of a
      checkpoint and start the executable with correspondingly modified
      arguments.

#. *Starting your executable up from a checkpoint is relatively quick.*

   -  The longer the start-up delay, the slower the job's overall
      progress. If your job's progress is too slow as a result of
      start-up delay, and your code can take checkpoints without
      exiting, read the 'Delayed Transfers' and 'Manual Transfers'
      sections below.

Other Options
-------------

The preceding sections of this HOWTO explain how a job meeting the
requirements can take checkpoints at arbitrary intervals and transfer
them back to the submit node. Although this is the method of operation
most likely to result in an interrupted job continuing from a valid
checkpoint, other, less reliable options exist.

Delayed Transfers
~~~~~~~~~~~~~~~~~

This method is risky, because it does not allow your job to recover from
any failure mode other than an eviction (and sometimes not even then).
It may also require changes to your ``executable``. The advantage of
this method is that it doesn't require your code to restart, or even a
recent version of HTCondor.

The basic idea is to take checkpoints as the job runs, but not transfer
them back to the submit node until the job is evicted. This implies that
your ``executable`` doesn't exit until the job is complete (which is the
normal case). If your code has long start-up delays, you'll naturally
not want it to exit after it writes a checkpoint; otherwise, the wrapper
script could restart the code as necessary.

To use this method, set ``when_to_transfer_output`` to
``ON_EXIT_OR_EVICT`` instead of setting ``checkpoint_exit_code``. This
will cause HTCondor to transfer your checkpoint file(s) (which you
listed in ``transfer_output_files``, as noted above) when the job is
evicted. Of course, since this is the only time your checkpoint file(s)
will be transferred, if the transfer fails, your job has to start over
from the beginning. One reason file transfer on eviction fails is if it
takes too long, so this method may not work if your
``transfer_output_files`` contain too much data.

Furthermore, eviction can happen at any time, including while the code
is updating its checkpoint file(s). If the code does not update its
checkpoint file(s) atomically, HTCondor will transfer the
partially-updated checkpoint file(s), potentially overwriting the
previous, complete one(s); this will probably prevent the code from
picking up where it left off.

In some cases, you can work around this problem by using a wrapper
script. The idea is that renaming a file is an atomic operation, so if
your code writes checkpoints to one file, call it ``checkpoint``, your
wrapper script -- when it detects that the checkpoint is complete --
would rename that file ``checkpoint.atomic``. That way,
``checkpoint.atomic`` always has a complete checkpoint in it. With a
such a script, instead of putting ``checkpoint`` in
``transfer_output_files``, you would put ``checkpoint.atomic``, and
HTCondor would never see a partially-complete checkpoint file. (The
script would also, of course, have to copy ``checkpoint.atomic`` to
``checkpoint`` before running the code.)

Manual Transfers
~~~~~~~~~~~~~~~~

If you're comfortable with programming, instead of running a job with
``checkpoint_exit_code``, you could use ``condor_chirp``, or other tools,
to manage your checkpoint file(s). Your ``executable`` would be
responsible for downloading the checkpoint file(s) on start-up, and
periodically uploading the checkpoint file(s) during execution. We don't
recommend you do this for the same reasons we recommend against managing
your own input and output file transfers.

Early Checkpoint Exits
~~~~~~~~~~~~~~~~~~~~~~

If your executable's natural checkpoint interval is half or more of your
pool's max job runtime, it may make sense to checkpoint and then
immediately ask to be rescheduled, rather than lower your user priority
doing work you know will be thrown away. In this case, you can use the
``OnExitRemove`` job attribute to determine if your job should be
rescheduled after exiting. Don't set ``ON_EXIT_OR_EVICT``, and don't set
``+WantFTOnCheckpoint``; just have the job exit with a unique code after
its checkpoint.

Signals
-------

Signals offer additional options for running self-checkpointing jobs. If
you're not familiar with signals, this section may not make sense to
you.

Periodic Signals
~~~~~~~~~~~~~~~~

HTCondor supports transferring checkpoint file(s) for an ``executable``
which takes a checkpoint when sent a particular signal, if the ``executable``
then exits in a unique way. Set ``+WantCheckpointSignal`` to ``TRUE`` to
periodically receive checkpoint signals, and ``+CheckpointSig`` to
specify which one. (The interval is specified by the administrator of
the execute machine.) The unique way may be a specific exit code, for which
you would set ``checkpoint_exit_code``, or a signal, for which you would
set ``+SuccessCheckpointExitBySignal`` to ``TRUE`` and
``+SuccessCheckpointExitSignal`` to the particular signal. (If you do
not set ``checkpoint_exit_code``, you must set ``+WantFTOnCheckpoint``.)

Delayed Transfer with Signals
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This method is very similar to but riskier than delayed transfers,
because in addition to delaying the transfer of the checkpoint files(s),
it also delays their creation. Thus, this option should almost never be
used; if taking and transferring your checkpoint file(s) is fast enough
to reliably complete during an eviction, you're not losing much by doing
so periodically, and it's unlikely that a code which takes small
checkpoints quickly takes a long time to start up. However, this method
will work even with very old version of HTCondor.

To use this method, set ``when_to_transfer_output`` to
``ON_EXIT_OR_EVICT`` and ``KillSig`` to the particular signal that
causes your job to checkpoint.
