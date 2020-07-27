Automatically managing a job
============================

While a user can manually manage an HTCondor job in ways described
in the previous section, it is often better to give HTCondor policies
with which it can automatically manage a job without user intervention.

Automatically rerunning a failed job
------------------------------------

By default, when a job exits, HTCondor considers it completed, removes it from 
the job queue and places it in the history file.  If a job exits
with a non-zero exit code, this usually means that some error has happened.
If this error is ephemeral, a user might want to re-run the job again, to see 
if the job succeeds on a second invocation.  HTCondor can does this automatically with the 
**max_retries** option in the submit file, to tell HTCondor the maximum
number of times to restart the job from scratch.  In the rare case where some
value other than zero indicates success, a submit file can set **success_exit_code**
to the integer value that is considered successful.

.. code-block:: condor-submit

    # Example submit description with max_retries

    executable   = myexe
    arguments    = SomeArgument

    # Retry this job 5 times if non-zero exit code
    max_retries = 5

    output       = outputfile
    error        = errorfile
    log          = myexe.log

    request_cpus   = 1
    request_memory = 1024
    request_disk   = 10240

    should_transfer_files = yes

    queue


Automatically removing a job in the queue
-----------------------------------------

HTCondor can automatically remove a job, running or otherwise, from the queue
if a given constraint is true.  In the submit description file, set
**periodic_remove** to a classad expression.  When this expression evaluates
to true, the scheduler will remove the job, just as if **condor_rm** had
run on that job.  See :doc:`/users-manual/matchmaking-with-classads` for information
about the classad language and :doc:`/classad-attributes/index` for the list of attributes
which can be used in these expressions.  For example, to automatically remove a 
job which has been in the queue for more than 100 hours, the submit file could have

.. code-block:: condor-submit

       periodic_remove = (time() - QDate) > (100 * 3600)

or, to remove jobs that have been running for more than two hours:

.. code-block:: condor-submit

       periodic_remove = (JobStatus == 2) && (time() - EnteredCurrentStatus) > (2 * 3600)

Automatically placing a job on hold
-----------------------------------

Often, if a job is doing something unexpected, it is more useful to hold the job,
rather than remove it.  If the problem with the job can be fixed, the job can then be
released and started again.  Much like the **periodic_remove** command, there is a 
**periodic_hold** command that works in a similar way, but instead of removing the job,
puts the job on hold.  Unliked **periodice_remove**, there are additional commands
that help to tell the user why the job was placed on hold.  **periodic_hold_reason**
is a string which is put into the **HoldReason** attribute to explain why we put the
job on hold.  **periodic_hold_subcode** is an integer that is put into the
**HoldReasonSubCode** that is useful for **periodic_release** to examine.  Neither
**periodic_hold_subcode** nor **periodic_hold_reason** are required, but are good
practice to include if **periodic_hold** is defined.


Automatically releasing a held job
----------------------------------

In the same way that a job can be automatically held, jobs in the held state
can be released with the **periodic_release** command.  Often, using a **periodic_hold** with 
a paired **periodic_release** is a good way to restart a stuck job.  Jobs can go
into the hold state for many reasons, so best practice, when trying to release
a job that was held with **periodic_hold** is to include the **HoldReasonSubCode**
in the **periodic_release** expression.

.. code-block:: condor-submit

       periodic_hold = (JobStatus == 2) && (time() - EnteredCurrentStatus) > (2 * 3600)
       periodic_hold_reason = "Job ran for more than two hours"
       periodic_hold_subcode = 42
       periodic_release = (HoldReasonSubCode == 42)


Holding a completed job
-----------------------

A job may exit, and HTCondor consider it completed, even though something has
gone wrong with the job.  A submit file may contain a **on_exit_hold** expression
to tell HTCondor to put the job on hold, instead of moving it to the history.  A held
job informs users that there may have been a problem with the job that should be investigated.
For example, if a job should never exit by a signal, the job can be put on hold if it
does with

.. code-block:: condor-submit

       on_exit_hold = ExitBySignal == true


