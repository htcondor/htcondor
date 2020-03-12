Job Event Log Codes
===================

:index:`event codes for jobs<single: event codes for jobs; log files>`

Table `B.2 <#x182-12460022>`_ lists codes that appear as the first

These are all of the events that can show up in a job log file:

| **Event Number:** 000
| **Event Name:** Job submitted
| **Event Description:** This event occurs when a user submits a job. It
  is the first event you will see for a job, and it should only occur
  once.

| **Event Number:** 001
| **Event Name:** Job executing
| **Event Description:** This shows up when a job is running. It might
  occur more than once.

| **Event Number:** 002
| **Event Name:** Error in executable
| **Event Description:** The job could not be run because the executable
  was bad.

| **Event Number:** 003
| **Event Name:** Job was checkpointed
| **Event Description:** The job's complete state was written to a
  checkpoint file. This might happen without the job being removed from a
  machine, because the checkpointing can happen periodically.

| **Event Number:** 004
| **Event Name:** Job evicted from machine
| **Event Description:** A job was removed from a machine before it
  finished, usually for a policy reason. Perhaps an interactive user has
  claimed the computer, or perhaps another job is higher priority.

| **Event Number:** 005
| **Event Name:** Job terminated
| **Event Description:** The job has completed.

| **Event Number:** 006
| **Event Name:** Image size of job updated
| **Event Description:** An informational event, to update the amount of
  memory that the job is using while running. It does not reflect the
  state of the job.

| **Event Number:** 007
| **Event Name:** Shadow exception
| **Event Description:** The *condor_shadow*, a program on the submit
  computer that watches over the job and performs some services for the
  job, failed for some catastrophic reason. The job will leave the machine
  and go back into the queue.

| **Event Number:** 008
| **Event Name:** Generic log event
| **Event Description:** Not used.

| **Event Number:** 009
| **Event Name:** Job aborted
| **Event Description:** The user canceled the job.

| **Event Number:** 010
| **Event Name:** Job was suspended
| **Event Description:** The job is still on the computer, but it is no
  longer executing. This is usually for a policy reason, such as an
  interactive user using the computer.

| **Event Number:** 011
| **Event Name:** Job was unsuspended
| **Event Description:** The job has resumed execution, after being
  suspended earlier.

| **Event Number:** 012
| **Event Name:** Job was held
| **Event Description:** The job has transitioned to the hold state.
  This might happen if the user applies the *condor_hold* command to the
  job.

| **Event Number:** 013
| **Event Name:** Job was released
| **Event Description:** The job was in the hold state and is to be
  re-run.

| **Event Number:** 014
| **Event Name:** Parallel node executed
| **Event Description:** A parallel universe program is running on a
  node.

| **Event Number:** 015
| **Event Name:** Parallel node terminated
| **Event Description:** A parallel universe program has completed on a
  node.

| **Event Number:** 016
| **Event Name:** POST script terminated
| **Event Description:** A node in a DAGMan work flow has a script that
  should be run after a job. The script is run on the submit host. This
  event signals that the post script has completed.

| **Event Number:** 017
| **Event Name:** Job submitted to Globus
| **Event Description:** A grid job has been delegated to Globus
  (version 2, 3, or 4). This event is no longer used.

| **Event Number:** 018
| **Event Name:** Globus submit failed
| **Event Description:** The attempt to delegate a job to Globus failed.

| **Event Number:** 019
| **Event Name:** Globus resource up
| **Event Description:** The Globus resource that a job wants to run on
  was unavailable, but is now available. This event is no longer used.

| **Event Number:** 020
| **Event Name:** Detected Down Globus Resource
| **Event Description:** The Globus resource that a job wants to run on
  has become unavailable. This event is no longer used.

| **Event Number:** 021
| **Event Name:** Remote error
| **Event Description:** The *condor_starter* (which monitors the job
  on the execution machine) has failed.

| **Event Number:** 022
| **Event Name:** Remote system call socket lost
| **Event Description:** The *condor_shadow* and *condor_starter*
  (which communicate while the job runs) have lost contact.

| **Event Number:** 023
| **Event Name:** Remote system call socket reestablished
| **Event Description:** The *condor_shadow* and *condor_starter*
  (which communicate while the job runs) have been able to resume contact
  before the job lease expired.

| **Event Number:** 024
| **Event Name:** Remote system call reconnect failure
| **Event Description:** The *condor_shadow* and *condor_starter*
  (which communicate while the job runs) were unable to resume contact
  before the job lease expired.

| **Event Number:** 025
| **Event Name:** Grid Resource Back Up
| **Event Description:** A grid resource that was previously unavailable
  is now available.

| **Event Number:** 026
| **Event Name:** Detected Down Grid Resource
| **Event Description:** The grid resource that a job is to run on is
  unavailable.

| **Event Number:** 027
| **Event Name:** Job submitted to grid resource
| **Event Description:** A job has been submitted, and is under the
  auspices of the grid resource.

| **Event Number:** 028
| **Event Name:** Job ad information event triggered.
| **Event Description:** Extra job ClassAd attributes are noted. This
  event is written as a supplement to other events when the configuration
  parameter ``EVENT_LOG_JOB_AD_INFORMATION_ATTRS``
  :index:`EVENT_LOG_JOB_AD_INFORMATION_ATTRS` is set.

| **Event Number:** 029
| **Event Name:** The job's remote status is unknown
| **Event Description:** No updates of the job's remote status have been
  received for 15 minutes.

| **Event Number:** 030
| **Event Name:** The job's remote status is known again
| **Event Description:** An update has been received for a job whose
  remote status was previous logged as unknown.

| **Event Number:** 031
| **Event Name:** Job stage in
| **Event Description:** A grid universe job is doing the stage in of
  input files.

| **Event Number:** 032
| **Event Name:** Job stage out
| **Event Description:** A grid universe job is doing the stage out of
  output files.

| **Event Number:** 033
| **Event Name:** Job ClassAd attribute update
| **Event Description:** A Job ClassAd attribute is changed due to
  action by the *condor_schedd* daemon. This includes changes by
  *condor_prio*.

| **Event Number:** 034
| **Event Name:** Pre Skip event
| **Event Description:** For DAGMan, this event is logged if a PRE
  SCRIPT exits with the defined PRE_SKIP value in the DAG input file.
  This makes it possible for DAGMan to do recovery in a workflow that has
  such an event, as it would otherwise not have any event for the DAGMan
  node to which the script belongs, and in recovery, DAGMan's internal
  tables would become corrupted.

| **Event Number:** 035
| **Event Name:** Cluster Submit
| **Event Description:** This event occurs when a user submits a cluster
  with multiple procs.

| **Event Number:** 036
| **Event Name:** Cluster Remove
| **Event Description:** This event occurs after all the jobs in a multi-proc 
  cluster have completed, or when the cluster is removed (by *condor_rm*).

| **Event Number:** 037
| **Event Name:** Factory Paused
| **Event Description:** This event occurs when job materialization for
  a cluster has been paused.

| **Event Number:** 038
| **Event Name:** Factory Resumed
| **Event Description:** This event occurs when job materialization for
  a cluster has been resumed

| **Event Number:** 039
| **Event Name:** None
| **Event Description:** This event should never occur in a log but may
  be returned by log reading code in certain situations (e.g., timing out
  while waiting for a new event to appear in the log).

| **Event Number:** 040
| **Event Name:** File Transfer
| **Event Description:** This event occurs when a file transfer event
  occurs: transfer queued, transfer started, or transfer finished, for
  both the input and output sandboxes.


Table B.2: Event Codes in a Job Event Log

+-------+---------------------------+---------------------------------------------------+
| 001   | EXECUTE                   | Execute                                           |
+-------+---------------------------+---------------------------------------------------+
| 002   | EXECUTABLE_ERROR          | Executable error                                  |
+-------+---------------------------+---------------------------------------------------+
| 003   | CHECKPOINTED              | Checkpointed                                      |
+-------+---------------------------+---------------------------------------------------+
| 004   | JOB_EVICTED               | Job evicted                                       |
+-------+---------------------------+---------------------------------------------------+
| 005   | JOB_TERMINATED            | Job terminated                                    |
+-------+---------------------------+---------------------------------------------------+
| 006   | IMAGE_SIZE                | Image size                                        |
+-------+---------------------------+---------------------------------------------------+
| 007   | SHADOW_EXCEPTION          | Shadow exception                                  |
+-------+---------------------------+---------------------------------------------------+
| 009   | JOB_ABORTED               | Job aborted                                       |
+-------+---------------------------+---------------------------------------------------+
| 010   | JOB_SUSPENDED             | Job suspended                                     |
+-------+---------------------------+---------------------------------------------------+
| 011   | JOB_UNSUSPENDED           | Job unsuspended                                   |
+-------+---------------------------+---------------------------------------------------+
| 012   | JOB_HELD                  | Job held                                          |
+-------+---------------------------+---------------------------------------------------+
| 013   | JOB_RELEASED              | Job released                                      |
+-------+---------------------------+---------------------------------------------------+
| 014   | NODE_EXECUTE              | Node execute                                      |
+-------+---------------------------+---------------------------------------------------+
| 015   | NODE_TERMINATED           | Node terminated                                   |
+-------+---------------------------+---------------------------------------------------+
| 016   | POST_SCRIPT_TERMINATED    | Post script terminated                            |
+-------+---------------------------+---------------------------------------------------+
| 017   | GLOBUS_SUBMIT             | Globus submit (no longer used)                    |
+-------+---------------------------+---------------------------------------------------+
| 018   | GLOBUS_SUBMIT_FAILED      | Globus submit failed                              |
+-------+---------------------------+---------------------------------------------------+
| 019   | GLOBUS_RESOURCE_UP        | Globus resource up (no longer used)               |
+-------+---------------------------+---------------------------------------------------+
| 020   | GLOBUS_RESOURCE_DOWN      | Globus resource down (no longer used)             |
+-------+---------------------------+---------------------------------------------------+
| 021   | REMOTE_ERROR              | Remote error                                      |
+-------+---------------------------+---------------------------------------------------+
| 022   | JOB_DISCONNECTED          | Job disconnected                                  |
+-------+---------------------------+---------------------------------------------------+
| 023   | JOB_RECONNECTED           | Job reconnected                                   |
+-------+---------------------------+---------------------------------------------------+
| 024   | JOB_RECONNECT_FAILED      | Job reconnect failed                              |
+-------+---------------------------+---------------------------------------------------+
| 025   | GRID_RESOURCE_UP          | Grid resource up                                  |
+-------+---------------------------+---------------------------------------------------+
| 026   | GRID_RESOURCE_DOWN        | Grid resource down                                |
+-------+---------------------------+---------------------------------------------------+
| 027   | GRID_SUBMIT               | Grid submit                                       |
+-------+---------------------------+---------------------------------------------------+
| 028   | JOB_AD_INFORMATION        | Job ClassAd attribute values added to event log   |
+-------+---------------------------+---------------------------------------------------+
| 029   | JOB_STATUS_UNKNOWN        | Job status unknown                                |
+-------+---------------------------+---------------------------------------------------+
| 030   | JOB_STATUS_KNOWN          | Job status known                                  |
+-------+---------------------------+---------------------------------------------------+
| 031   | JOB_STAGE_IN              | Grid job stage in                                 |
+-------+---------------------------+---------------------------------------------------+
| 032   | JOB_STAGE_OUT             | Grid job stage out                                |
+-------+---------------------------+---------------------------------------------------+
| 033   | ATTRIBUTE_UPDATE          | Job ClassAd attribute update                      |
+-------+---------------------------+---------------------------------------------------+
| 034   | PRESKIP                   | DAGMan PRE_SKIP defined                           |
+-------+---------------------------+---------------------------------------------------+
| 035   | CLUSTER_SUBMIT            | Cluster submitted                                 |
+-------+---------------------------+---------------------------------------------------+
| 036   | CLUSTER_REMOVE            | Cluster removed                                   |
+-------+---------------------------+---------------------------------------------------+
| 037   | FACTORY_PAUSED            | Factory paused                                    |
+-------+---------------------------+---------------------------------------------------+
| 038   | FACTORY_RESUMED           | Factory resumed                                   |
+-------+---------------------------+---------------------------------------------------+
| 039   | NONE                      | No event could be returned                        |
+-------+---------------------------+---------------------------------------------------+
| 040   | FILE_TRANSFER             | File transfer                                     |
+-------+---------------------------+---------------------------------------------------+


