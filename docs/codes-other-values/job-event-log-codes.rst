Job Event Log Codes
===================

:index:`event codes for jobs<single: event codes for jobs; log files>`

.. list-table:: Table: Event Codes in a Job Event Log
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Constant Name
     - Description
   * - 000
     - SUBMIT
     - This event occurs when a user submits a job. It is the first event you will see for a job, and it should only occur once.
   * - 001
     - EXECUTE
     - This shows up when a job is running. It might occur more than once.
   * - 002
     - EXECUTABLE_ERROR
     - The job could not be run because the executable was bad.
   * - 003
     - CHECKPOINTED
     - No longer used.
   * - 004
     - JOB_EVICTED
     - A job was removed from a machine before it finished, usually for a policy reason. Perhaps an interactive user has claimed the computer, or perhaps another job is higher priority.
   * - 005
     - JOB_TERMINATED
     - The job has completed.
   * - 006
     - IMAGE_SIZE
     - An informational event, to update the amount of memory that the job is using while running. It does not reflect the state of the job.
   * - 007
     - SHADOW_EXCEPTION
     - The *condor_shadow*, a program on the submit computer that watches over the job and performs some services for the job, failed for some catastrophic reason. The job will leave the machine and go back into the queue.
   * - 009
     - JOB_ABORTED
     - The user canceled the job.
   * - 010
     - JOB_SUSPENDED
     - The job is still on the computer, but it is no longer executing. This is usually for a policy reason, such as an interactive user using the computer.
   * - 011
     - JOB_UNSUSPENDED
     - The job has resumed execution, after being suspended earlier.
   * - 012
     - JOB_HELD
     - The job has transitioned to the hold state. This might happen if the user applies the :tool:`condor_hold` command to the job.
   * - 013
     - JOB_RELEASED
     - The job was in the hold state and is to be re-run.
   * - 014
     - NODE_EXECUTE
     - A parallel universe program is running on a node.
   * - 015
     - NODE_TERMINATED
     - A parallel universe program has completed on a node.
   * - 016
     - POST_SCRIPT_TERMINATED
     - A node in a DAGMan work flow has a script that should be run after a job. The script is run on the submit host. This event signals that the post script has completed.
   * - 021
     - REMOTE_ERROR
     - The *condor_starter* (which monitors the job on the execution machine) has failed.
   * - 022
     - JOB_DISCONNECTED
     - The *condor_shadow* and *condor_starter* (which communicate while the job runs) have lost contact.
   * - 023
     - JOB_RECONNECTED
     - The *condor_shadow* and *condor_starter* (which communicate while the job runs) have been able to resume contact before the job lease expired.
   * - 024
     - JOB_RECONNECT_FAILED
     - The *condor_shadow* and *condor_starter* (which communicate while the job runs) were unable to resume contact before the job lease expired.
   * - 025
     - GRID_RESOURCE_UP
     - A grid resource that was previously unavailable is now available.
   * - 026
     - GRID_RESOURCE_DOWN
     - The grid resource that a job is to run on is unavailable.
   * - 027
     - GRID_SUBMIT
     - A job has been submitted, and is under the auspices of the grid resource.
   * - 028
     - JOB_AD_INFORMATION
     - Extra job ClassAd attributes are noted. This event is written as a supplement to other events when the configuration parameter :macro:`EVENT_LOG_JOB_AD_INFORMATION_ATTRS` is set.
   * - 029
     - JOB_STATUS_UNKNOWN
     - No updates of the job's remote status have been received for 15 minutes.
   * - 030
     - JOB_STATUS_KNOWN
     - An update has been received for a job whose remote status was previous logged as unknown.
   * - 031
     - JOB_STAGE_IN
     - A grid universe job is doing the stage in of input files.
   * - 032
     - JOB_STAGE_OUT
     - A grid universe job is doing the stage out of output files.
   * - 033
     - ATTRIBUTE_UPDATE
     - A Job ClassAd attribute is changed due to action by the *condor_schedd* daemon. This includes changes by :tool:`condor_prio`.
   * - 034
     - PRESKIP
     - For DAGMan, this event is logged if a PRE SCRIPT exits with the defined PRE_SKIP value in the DAG input file. This makes it possible for DAGMan to do recovery in a workflow that has such an event, as it would otherwise not have any event for the DAGMan node to which the script belongs, and in recovery, DAGMan's internal tables would become corrupted.
   * - 035
     - CLUSTER_SUBMIT
     - This event occurs when a user submits a cluster with multiple procs.
   * - 036
     - CLUSTER_REMOVE
     - This event occurs after all the jobs in a multi-proc cluster have completed, or when the cluster is removed (by :tool:`condor_rm`).
   * - 037
     - FACTORY_PAUSED
     - This event occurs when job materialization for a cluster has been paused.
   * - 038
     - FACTORY_RESUMED
     - This event occurs when job materialization for a cluster has been resumed
   * - 039
     - NONE
     - This event should never occur in a log but may be returned by log reading code in certain situations (e.g., timing out while waiting for a new event to appear in the log).
   * - 040
     - FILE_TRANSFER
     - This event occurs when a file transfer event occurs: transfer queued, transfer started, or transfer finished, for both the input and output sandboxes.
   * - 047
     - COMMON_FILES
     - This event occurs when a common files event occurs: common files transfer queued, started, or finished; or when waiting for common files to transfer started or finished.
