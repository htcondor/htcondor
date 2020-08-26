*condor_shadow* Exit Codes
===========================

:index:`of condor_shadow<single: of condor_shadow; exit codes>`

When a *condor_shadow* daemon exits, the *condor_shadow* exit code is
recorded in the *condor_schedd* log, and it identifies why the job
exited. Prose in the log appears of the form

.. code-block:: text

    Shadow pid XXXXX for job XX.X exited with status YYY

where ``YYY`` is the exit code, or

.. code-block:: text

    Shadow pid XXXXX for job XX.X reports job exit reason 100.

where the exit code is the value 100. The following table lists these codes:

+---------+------------------------------------+--------------------------------------------------------------+
| Value   | Error Name                         | Description                                                  |
+---------+------------------------------------+--------------------------------------------------------------+
| 4       | JOB_EXCEPTION                      | the job exited with an exception                             |
+---------+------------------------------------+--------------------------------------------------------------+
| 44      | DPRINTF_ERROR                      | there was a fatal error with dprintf()                       |
+---------+------------------------------------+--------------------------------------------------------------+
| 100     | JOB_EXITED                         | the job exited (not killed)                                  |
+---------+------------------------------------+--------------------------------------------------------------+
| 101     | JOB_CKPTED                         | the job did produce a checkpoint                             |
+---------+------------------------------------+--------------------------------------------------------------+
| 102     | JOB_KILLED                         | the job was killed                                           |
+---------+------------------------------------+--------------------------------------------------------------+
| 103     | JOB_COREDUMPED                     | the job was killed and a core file was produced              |
+---------+------------------------------------+--------------------------------------------------------------+
| 105     | JOB_NO_MEM                         | not enough memory to start the condor_shadow                 |
+---------+------------------------------------+--------------------------------------------------------------+
| 106     | JOB_SHADOW_USAGE                   | incorrect arguments to condor_shadow                         |
+---------+------------------------------------+--------------------------------------------------------------+
| 107     | JOB_NOT_CKPTED                     | the job vacated without a checkpoint                         |
+---------+------------------------------------+--------------------------------------------------------------+
| 107     | JOB_SHOULD_REQUEUE                 | same number as JOB_NOT_CKPTED,                               |
+         |                                    | to achieve the same behavior.                                |
|         |                                    | This exit code implies that we want                          |
|         |                                    | the job to be put back in the job queue                      |
|         |                                    | and run again.                                               |
+---------+------------------------------------+--------------------------------------------------------------+
| 108     | JOB_NOT_STARTED                    | can not connect to the *condor_startd* or request refused    |
+---------+------------------------------------+--------------------------------------------------------------+
| 109     | JOB_BAD_STATUS                     | job status != RUNNING on start up                            |
+---------+------------------------------------+--------------------------------------------------------------+
| 110     | JOB_EXEC_FAILED                    | exec failed for some reason other than ENOMEM                |
+---------+------------------------------------+--------------------------------------------------------------+
| 111     | JOB_NO_CKPT_FILE                   | there is no checkpoint file (as it was lost)                 |
+---------+------------------------------------+--------------------------------------------------------------+
| 112     | JOB_SHOULD_HOLD                    | the job should be put on hold                                |
+---------+------------------------------------+--------------------------------------------------------------+
| 113     | JOB_SHOULD_REMOVE                  | the job should be removed                                    |
+---------+------------------------------------+--------------------------------------------------------------+
| 114     | JOB_MISSED_DEFERRAL_TIME           | the job goes on hold, because it did not run within the      |
|         |                                    | specified window of time                                     |
+---------+------------------------------------+--------------------------------------------------------------+
| 115     | JOB_EXITED_AND_CLAIM_CLOSING       | the job exited (not killed) but the *condor_startd*          |
|         |                                    | is not accepting any more jobs on this claim                 |
+---------+------------------------------------+--------------------------------------------------------------+
| 116     | JOB_RECONNECT_FAILED               | the *condor_shadow* was started in reconnect mode, and yet   |
|         |                                    | failed to reconnect to the starter                           |
+---------+------------------------------------+--------------------------------------------------------------+


