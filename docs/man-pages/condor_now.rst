      

*condor_now*
=============

Start a job now.
:index:`condor_now<single: condor_now; HTCondor commands>`\ :index:`condor_now ommand`

Synopsis
--------

**condor_now** **-help**

**condor_now** [**-name** **] [**-debug** ] *now-job* *vacate-job*
[*vacate-job+* ]

Description
-----------

*condor_now* tries to run the *now-job* now. The *vacate-job* is
immediately vacated; after it terminates, if the schedd still has the
claim to the vacated job's slot - and it usually will - the schedd will
immediately start the now-job on that slot.

If you specify multiple *vacate-job* s, each will be immediately
vacated; after they all terminate, the schedd will try to coalesce their
slots into a single, larger, slot and then use that slot to run the
now-job.

You must specify each job using both the cluster and proc IDs.

Options
-------

 **-help**
    Print a usage reminder.
 **-debug**
    Print debugging output. Control the verbosity with the environment
    variables _CONDOR_TOOL_DEBUG, as usual.
 **-name** **
    Specify the scheduler('s name) and (optionally) the pool to find it
    in.

General Remarks
---------------

The now-job and the vacated-job must have the same owner; if you are not
the queue super-user, you must own both jobs. The jobs must be on the
same schedd, and both jobs must be in the vanilla universe. The now-job
must be idle and the vacated-job must be running.

Examples
--------

To begin running job 17.3 as soon as possible using job 4.2's slot:

.. code-block:: console

      $ condor_now 17.3 4.2

To try to figure out why that doesn't work for the 'magic' scheduler in
the 'gandalf' pool, set the environment variable _CONDOR_TOOL_DEBUG
to 'D_FULLDEBUG' and then:

.. code-block:: console

      $ condor_now -debug -schedd magic -pool gandalf 17.3 4.2

Exit Status
-----------

*condor_now* will exit with a status value of 0 (zero) if the schedd
accepts its request to vacate the vacate-job and start the now-job in
its place. It does not wait for the now-job to have started running.

