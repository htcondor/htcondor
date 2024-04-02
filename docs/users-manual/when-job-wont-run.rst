How To Debug an Always Idle Job
===============================

Sometimes, when you submit a job to HTCondor, it sits idle seemingly forever,
:tool:`condor_q` shows it in the idle state, when you expect it should start running.
This can be frustrating, but there are tools to give visibility so you can
debug what is going on.

Jobs that start but are quickly evicted
---------------------------------------

One possibility is that the job is actually starting, but something goes wrong
very quickly after it starts, so the Execution Point evicts the job, and the
*condor_schedd* puts it back to idle.  :tool:`condor_q` would only show it in the
"R"unning state for a brief moment, so it is likely that even frequent
executions of :tool:`condor_q` will show it in the Idle state.

A quick look at the HTCondor job log will help to verify that this is what is
happening.  Assuming your submit file contains a line like:

.. code-block:: condor-submit

    log          = my_job.log


Then you should see a line in my_job.log, assuming that HTCondor assigned the
job id of 781.0 to your job (the job id is in parenthesis):

.. code-block:: text

    000 (781.000.000) 2022-01-30 15:15:35 Job submitted from host: <127.0.0.1:45527?addrs=127.0.0.1-45527>
    ...

Many jobs can share the same job log file, so be sure to find the entries for the job
in question.  If there is nothing further in this log, this flapping between
Running and Idle is not the problem, and you can check items further down this list.

However, if you see repeated entries like

.. code-block:: text

    001 (781.000.000) 2022-01-30 15:15:36 Job executing on host: <127.0.0.1:42089?addrs=127.0.0.1-42089>
    ...
    007 (781.000.000) 2022-01-30 15:15:37 Shadow exception!
         Error from slot1_2@bat: FATAL:    executable file not found in $PATH
         0  -  Run Bytes Sent By Job
         0  -  Run Bytes Received By Job
     ...
     001 (781.000.000) 2022-01-30 15:15:37 Job executing on host: <127.0.0.1:42089?addrs=127.0.0.1-42089>
     ...
     007 (781.000.000) 2022-01-30 15:15:38 Shadow exception!
     ...

Then this flapping is the problem, and you'll need to figure out why.  Perhaps a
*condor_submit -i* interactive login, and trying to start the job by hand is
useful, maybe you'll need to ask a system administrator.

Jobs that don't match any Execution Point
-----------------------------------------

Another common cause of an always-idle job is that the job doesn't match any
slot in the pool.  Perhaps the memory or disk requested in the submit file is
greater than any slot in the pool has.  Perhaps your administrator requires
jobs to set certain custom attributes to identify them, or for accounting.
HTCondor has a tool we call better-analyze that simulates the matching of slots
to jobs.  It isn't perfect, as it doesn't have full knowledge of the system,
but it is easy to run, and can help to quickly narrow down this kind of
problems.

.. code-block:: console

      $ condor_q -better-analyze 781.0


Now, as *condor_q -better-analyze* by default, tries to simulate matching
this job to all slots in the pool, this can take a while, and generate
a lot of output.  Sometimes, you are pretty sure that a job should match one 
particular slot, in that case, you can restrict the matching attempt to
that one slot by running

.. code-block:: console

      $ condor_q -better-analyze 781.0 -machine machine_in_question


which will emit information only about a potential match to
machine_in_question.  If the last few lines of this look like
this:

.. code-block:: console

        The Requirements expression for job 781.0 reduces to these conditions:

         Slots
          Step    Matched  Condition
          -----  --------  ---------
          [0]           1  TARGET.Arch == "X86_64"
          [1]           1  TARGET.OpSys == "LINUX"
          [3]           1  TARGET.Disk >= RequestDisk
          [5]           0  TARGET.Memory >= RequestMemory


          781.007:  Run analysis summary ignoring user priority.  Of 1 machines,
           1 are rejected by your job's requirements
           0 reject your job because of their own requirements
           0 match and are already running your jobs
           0 match but are serving other users
           0 are able to run your job

          WARNING:  Be advised:
             No machines matched the jobs's constraints


In this example, RequestMemory is set too high, so the job won't match any machines.
Maybe it was a typo.  Try setting it lower to see if the job will match.
If *condor_q -better-analyze* tells you that some machines do match, then 
this probably isn't the problem, or, it could be that very few machines in your
pool match your job, and you'll just need to wait until they are available.

Not enough priority
-------------------

Another reason your job isn't running is that other jobs of yours are running,
but your priority isn't good enough to allow any more of your jobs running.
If this is a problem, the HTCondor *condor_schedd* will run your jobs in
the order specified by the Job_Priority submit command.  You could 
give your more important jobs a higher job priority.  The command
*condor_userprio -all* will show you your current *userprio*, which
is what HTCondor uses to calculate your fair share.

Systemic problems
-----------------

The final case is that you have done nothing wrong, but there is some problem
with the system.  Maybe a network is down, or a system daemon has crashed,
or there is an overload somewhere.  If you are an expert, there may be
information in the debug logs, usually found in /var/log/condor.  In this
case, you may need to consult your system administrator, or ask for
help on the *condor-users* email list.
