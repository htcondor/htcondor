Recipes for Users
=================

How to Run a Job on every machine in the pool
---------------------------------------------

Running a single command on every machine in the pool can be accomplished by submitting a set of HTCondor jobs using a single submit description file of the following form:

.. code-block:: condor-submit

   executable = X
   ...
   requirements = TARGET.machine == MY.TargetMachine

   +TargetMachine = "machine1"
   queue

   +TargetMachine = "machine2"
   queue

   +TargetMachine = "machine3"
   queue

There will be one +TargetMachine and one queue command for each machine in the pool.

A list of machine names for all machines in the pool may be obtained using the condor_status command:

condor_status -af Machine | sort -u

How to Avoid Running jobs on a Black Hole Machine
-------------------------------------------------

A common problem is when some execute machine is misconfigured or broken in such a way that it still accepts HTCondor jobs, but can't run them correctly. If jobs exit quickly on this kind of machine, it can quickly eat many of the jobs in your queue. We call this a "black hole" machine. Some conditions observed to cause black holes are documented in this wiki.

To work around black hole machines, you can add the following to your job submit file:

.. code-block:: condor-submit

   job_machine_attrs = Machine
   job_machine_attrs_history_length = 5
   requirements = target.machine =!= MachineAttrMachine1 && target.machine =!= MachineAttrMachine2 ...


Note that this can cause performance problems by creating an autocluster per job, so use 
this cautiously.

How to never restart a failed job
---------------------------------

By default, HTCondor manages jobs under the assumption that the user wants them
to be run as many times as necessary in order to successfully finish. If all
goes well, this means the job will only run once. However, various failures can
require that the job be restarted in order to succeed. Examples of such
failures include:

#. execute machine crashes while job is running
#. submit machine crashes while job is running and does not reconnect to job before the job lease expires
#. job is evicted by condor_vacate, PREEMPT, or is preempted by another job
#. output files from job fail to be transferred back to submit machine
#. input files fail to be transferred
#. network failures between execute and submit machine

In some cases, it is desired that jobs not be restarted. The user wants
HTCondor to try to run the job once, and if this attempt fails for any reason,
it should not make a second attempt. To achieve this, the following can be put
in the job's submit file:

.. code-block:: condor-submit

   requirements = NumJobStarts == 0
   periodic_remove = JobStatus == 1 && NumJobStarts > 0

Note that this does not guarantee that HTCondor will only start the job once.
The NumJobStarts job attribute is updated shortly after the job starts running.
Various types of failures can result in the job starting without this attribute
being updated (e.g. network failure between submit and execute machine). By
setting SHADOW_LAZY_QUEUE_UPDATE=false, the window of time between the job
starting and the update of NumJobStarts can be decreased, but this still does
not provide a guarantee that the job will never be started more than once. This
policy is therefore to start the job at least once, and, with best effort but
no strong guarantee, not more than once. As usual, HTCondor does provide a
strong guarantee that the job is never running more than once at the same time.

Instead of NumJobStarts, you can flag off of several over attributes that are
incremented when a job starts up:

#. :ad-attr:`NumShadowStarts` - Incremented when the condor_shadow starts, but before the job has started. Guarantees that a job will run at most once, but it a problem occurs between the shadow starting and the job starting, the job will never run.
#. :ad-attr:`NumJobMatches`
#. :ad-attr:`JobRunCount`

How to algorithmically vary a job's arguments
---------------------------------------------

You might want to submit a job repeatedly, varying the arguments for each run.
The core technique is to queue multiple jobs in a single submit file, using
submit macros and ClassAd expressions to calculate the argument.

Monotonically increasing count
''''''''''''''''''''''''''''''

You can use $(Process) in your submit file to get a monotonically increasing
count. This is also useful if you need an arbitrary unique identifier. For
example, to submit 10 jobs, where the first job will get the argument "0", the
second job "1", and so on, you could use:

.. code-block:: condor-submit

    executable = myprogram.exe
    arguments  = $(Process)
    queue 10

You can do simple math in the submit file using $INT() or $REAL(). The
technique is to use a temporary variable to declare the mathematical
expressions, and then refer to the temporary variable using the $INT() or
$REAL() macro expansion to evaluate and print the result. The $INT() and
$REAL() expansions take an optional argument to control the formatting of the
result.

.. code-block:: condor-submit

   executable = myprogram.exe
   tmp1 = $(Process) + 1
   tmp2 = $(Process) * 2.0
   arguments = $INT(tmp1) $REAL(tmp2,%.3f)
   queue 10

Selection from a list
'''''''''''''''''''''

You can use the Process variable to select an item from a list using the
$CHOICE() macro.  In this example, there will be 6 jobs submitted, the first
will have arguments=Alan, the second will have arguments=Betty, etc.

.. code-block:: condor-submit

   executable = myprogram.exe
   arguments = $CHOICE(Process, Alan, Betty, Claire, Dan, Eva, Frank)
   queue 6

The list can also be a submit variable like this. This example produces the same resulting jobs as the above example.

.. code-block:: condor-submit

   executable = myprogram.exe
   arguments = $CHOICE(Process, Names)
   Names = Alan, Betty, Claire, John, Harry, Sally
   queue 6

Random selection
''''''''''''''''

If you want each job to have a randomly assigned argument, you can use
$RANDOM_CHOICE. The random selection is not guaranteed to be cryptographically
strong, nor necessarily suitable for scientific use.

In this example, HTCondor assigns each job two random choices; an integer from 1 to 3, and a random name:

.. code-block:: condor-submit

   executable = myprogram.exe
   arguments  = $RANDOM_CHOICE(1,2,3) $RANDOM_CHOICE(Alan, Betty, Claire)
   queue 10
