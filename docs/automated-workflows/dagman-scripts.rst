Scripts
=======

:index:`SCRIPT command<single: DAG input file; SCRIPT command>`

The optional *SCRIPT* command specifies processing that is done either
before a job within a node is submitted, after a job within a node
completes its execution, or when a job goes on hold. All scripts run
on the Access Point and not the Execution Point where the node job
is likely to run.

:index:`PRE and POST scripts<single: DAGMan; PRE and POST scripts>`

PRE and POST scripts
--------------------

:index:`PRE script<single: DAGMan; PRE script>` Processing
done before a job is submitted is called a *PRE* script. Processing done
after a job completes its execution is
:index:`POST script<single: DAGMan; POST script>` called a *POST* script. Note that
the executable specified does not necessarily have to be a shell script
(Unix) or batch file (Windows); but it should be relatively light weight
because it will be run directly on the access point, not submitted as
an HTCondor job.

The syntax used for *PRE* or *POST* commands is

.. code-block:: condor-dagman

    SCRIPT [DEFER status time] PRE <JobName | ALL_NODES> ExecutableName [arguments]

.. code-block:: condor-dagman

    SCRIPT [DEFER status time] POST <JobName | ALL_NODES> ExecutableName [arguments]

The *SCRIPT* command can use the *PRE* or *POST* keyword, which specifies
the relative timing of when the script is to be run. The *JobName*
identifies the node to which the script is attached. The
*ExecutableName* specifies the executable (e.g., shell script or batch
file) to be executed, and may not contain spaces. The optional
*arguments* are command line arguments to the script, and spaces delimiting
the arguments. Both *ExecutableName* and optional *arguments* are case
sensitive.

A PRE script is commonly used to place files in a staging area for the
jobs to use. A POST script is commonly used to clean up or remove files
once jobs are finished running. An example uses PRE and POST scripts to
stage files that are stored on tape. The PRE script reads compressed
input files from the tape drive, uncompresses them, and places the
resulting files in the current directory. The HTCondor jobs can then use
these files, producing output files. The POST script compresses the
output files, writes them out to the tape, and then removes both the
staged files and the output files.

:index:`HOLD script<single: DAGMan; HOLD script>`

HOLD scripts
------------

Additionally, the *SCRIPT* command can take a *HOLD* keyword, which indicates an
executable to be run when a job goes on hold. These are typically used to
notify a user when something goes wrong with their jobs.

The syntax used for a *HOLD* command is

.. code-block:: condor-dagman

    SCRIPT [DEFER status time] HOLD <JobName | ALL_NODES> ExecutableName [arguments]

Unlike *PRE* and *POST* scripts, *HOLD* scripts are not considered part of the
DAG workflow and are run on a best-effort basis. If one does not complete
successfully, it has no effect on the overall workflow and no error will be
reported.

DEFER retries
-------------

The optional *DEFER* keyword causes a retry of only the script, if the
execution of the script exits with the exit code given by *status*. The
retry occurs after at least *time* seconds, rather than being considered
failed. While waiting for the retry, the script does not count against a
*maxpre* or *maxpost* limit. The ordering of the *DEFER* keyword within
the *SCRIPT* specification is fixed. It must come directly after the
*SCRIPT* keyword; this is done to avoid backward compatibility issues
for any DAG with a *JobName* of DEFER.

Scripts as part of a DAG workflow
---------------------------------

Scripts are executed on the access point; the access point is not
necessarily the same machine upon which the node's job is run. Further,
a single cluster of HTCondor jobs may be spread across several machines.

If the PRE script fails, then the HTCondor job associated with the node
is not submitted, and (as of version 8.5.4) the POST script is not run
either (by default). However, if the job is submitted, and there is a
POST script, the POST script is always run once the job finishes. (The
behavior when the PRE script fails may be changed to run the POST
script by setting configuration variable :macro:`DAGMAN_ALWAYS_RUN_POST` to
``True`` or by passing the **-AlwaysRunPost** argument to
*condor_submit_dag*.)

Examples that use PRE or POST scripts
-------------------------------------

Examples use the diamond-shaped DAG. A first example uses a PRE script
to expand a compressed file needed as input to each of the HTCondor jobs
of nodes B and C. The DAG input file:

.. code-block:: condor-dagman

    # File name: diamond.dag

    JOB  A  A.condor
    JOB  B  B.condor
    JOB  C  C.condor
    JOB  D  D.condor
    SCRIPT PRE  B  pre.sh $JOB .gz
    SCRIPT PRE  C  pre.sh $JOB .gz
    PARENT A CHILD B C
    PARENT B C CHILD D

The script ``pre.sh`` uses its command line arguments to form the file
name of the compressed file. The script contains

.. code-block:: bash

    #!/bin/sh
    gunzip ${1}${2}

Therefore, the PRE script invokes

.. code-block:: bash

    gunzip B.gz

for node B, which uncompresses file ``B.gz``, placing the result in file ``B``.

A second example uses the ``$RETURN`` macro. The DAG input file contains
the POST script specification:

.. code-block:: condor-dagman

    SCRIPT POST A stage-out job_status $RETURN

If the HTCondor job of node A exits with the value -1, the POST script
is invoked as

.. code-block:: text

    stage-out job_status -1

The slightly different example POST script specification in the DAG
input file

.. code-block:: condor-dagman

    SCRIPT POST A stage-out job_status=$RETURN

invokes the POST script with

.. code-block:: text

    stage-out job_status=$RETURN

This example shows that when there is no space between the ``=`` sign
and the variable ``$RETURN``, there is no substitution of the macro's
value.

Special script argument macros
------------------------------

The five macros ``$JOB``, ``$RETRY``, ``$MAX_RETRIES``, ``$DAG_STATUS``
and ``$FAILED_COUNT`` can be used within the DAG input file as arguments
passed to a PRE or POST script. An additional three macros ``$JOBID``,
``$RETURN``, and ``$PRE_SCRIPT_RETURN`` can be used as arguments to POST
scripts. The use of these variables is limited to being used as an
individual command line *argument* to the script, surrounded by spaces,
in order to cause the substitution of the variable's value.

:index:`Defined special node macros<single: DAGMan; Defined special node macros>`

The special macros are as follows:

-  ``$JOB`` evaluates to the (case sensitive) string defined for
   *JobName*.
-  ``$RETRY`` evaluates to an integer value set to 0 the first time a
   node is run, and is incremented each time the node is retried. See
   :ref:`automated-workflows/node-pass-or-fail:retrying failed nodes` for
   the description of how to cause nodes to be retried.
-  ``$MAX_RETRIES`` evaluates to an integer value set to the maximum
   number of retries for the node. See
   :ref:`automated-workflows/node-pass-or-fail:retrying failed nodes` for the
   description of how to cause nodes to be retried. If no retries are set for
   the node, ``$MAX_RETRIES`` will be set to 0.
-  ``$JOBID`` (for POST scripts only) evaluates to a representation of
   the HTCondor job ID of the node job. It is the value of the job
   ClassAd attribute ``ClusterId``, followed by a period, and then
   followed by the value of the job ClassAd attribute ``ProcId``. An
   example of a job ID might be 1234.0. For nodes with multiple jobs in
   the same cluster, the ``ProcId`` value is the one of the last job
   within the cluster.
-  ``$RETURN`` (for POST scripts only) variable evaluates to the return
   value of the HTCondor job, if there is a single job within a cluster.
   With multiple jobs within the same cluster, there are two cases to
   consider. In the first case, all jobs within the cluster are
   successful; the value of ``$RETURN`` will be 0, indicating success.
   In the second case, one or more jobs from the cluster fail. When
   *condor_dagman* sees the first terminated event for a job that
   failed, it assigns that job's return value as the value of
   ``$RETURN``, and it attempts to remove all remaining jobs within the
   cluster. Therefore, if multiple jobs in the cluster fail with
   different exit codes, a race condition determines which exit code
   gets assigned to ``$RETURN``.

   A job that dies due to a signal is reported with a ``$RETURN`` value
   representing the additive inverse of the signal number. For example,
   SIGKILL (signal 9) is reported as -9. A job whose batch system
   submission fails is reported as -1001. A job that is externally
   removed from the batch system queue (by something other than
   *condor_dagman*) is reported as -1002.
-  ``$PRE_SCRIPT_RETURN`` (for POST scripts only) variable evaluates to
   the return value of the PRE script of a node, if there is one. If
   there is no PRE script, this value will be -1. If the node job was
   skipped because of failure of the PRE script, the value of
   ``$RETURN`` will be -1004 and the value of ``$PRE_SCRIPT_RETURN``
   will be the exit value of the PRE script; the POST script can use
   this to see if the PRE script exited with an error condition, and
   assign success or failure to the node, as appropriate.
-  ``$DAG_STATUS`` is the status of the DAG. Note that this macro's
   value and definition is unrelated to the attribute named
   ``DagStatus`` as defined for use in a node status file. This macro's
   value is the same as the job ClassAd attribute ``DAG_Status`` that is
   defined within the *condor_dagman* job's ClassAd. This macro may
   have the following values:

   -  0: OK
   -  1: error; an error condition different than those listed here
   -  2: one or more nodes in the DAG have failed
   -  3: the DAG has been aborted by an ABORT-DAG-ON specification
   -  4: removed; the DAG has been removed by *condor_rm*
   -  5: cycle; a cycle was found in the DAG
   -  6: halted; the DAG has been halted
      (see :ref:`automated-workflows/dagman-interaction:suspending a running dag`)

-  ``$FAILED_COUNT`` is defined by the number of nodes that have failed
   in the DAG.
