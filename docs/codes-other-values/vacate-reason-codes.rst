Vacate Reason Codes
===================


    +---------------------------------------+-------------------------------------+--------------------------+
    | | Integer VacateReasonCode            | | Reason for Vacate                 | | VacateReasonSubCode    |
    | | [Label]                             |                                     |                          |
    +=======================================+=====================================+==========================+
    | | 1000                                | :ad-attr:`PeriodicVacate` evaluated |                          |
    | | [JobPolicyVacate]                   | to ``True``.                        |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1001                                | :macro:`SYSTEM_PERIODIC_VACATE`     |                          |
    | | [SystemPolicyVacate]                | evaluated to ``True``.              |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1002                                | A Shadow Exception event occurred.  |                          |
    | | [ShadowException]                   |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1003                                | A setup step failed.                |                          |
    | | [JobNotStarted]                     |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1004                                | The user requested the job be       |                          |
    | | [UserVacateJob]                     | vacated.                            |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1005                                | An unspecified error occurred.      |                          |
    | | [JobShouldRequeue]                  |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1006                                | The shadow failed to activate the   |                          |
    | | [FailedToActivateClaim]             | claim                               |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1007                                | The starter encountered an error.   |                          |
    | | [StarterError]                      |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1008                                | The shadow failed to reconnect      |                          |
    | | [ReconnectFailed]                   | after a network failure.            |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1009                                | The AP requested the job to be      |                          |
    | | [ClaimDeactivated]                  | vacated.                            |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1010                                | The administrator requested the job |                          |
    | | [StartdVacateCommand]               | to be vacated.                      |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1011                                | The EP's PREEMPT expression         |                          |
    | | [StartdPreemptExpression]           | evaluated to True.                  |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1012                                | The startd died due to an internal  |                          |
    | | [StartdException]                   | error.                              |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1013                                | The startd was shut down.           |                          |
    | | [StartdShutdown]                    |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1014                                | The slot was drained.               |                          |
    | | [StartdDraining]                    |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1015                                | The slot was coalesced with other   |                          |
    | | [StartdCoalesce]                    | slots by condor_now.                |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1016                                | The startd entered hibernation.     |                          |
    | | [StartdHibernate]                   |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1017                                | The AP released the claim.          |                          |
    | | [StartdReleaseCommand]              |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1018                                | The slot was claimed for a job with |                          |
    | | [StartdPreemptingClaimRank]         | a higher startd Rank.               |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1019                                | The slot was claimed for a job with |                          |
    | | [StartdPreemptingClaimUserPrio]     | better user priority.               |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1020                                | The virtual machine software at the |                          |
    | | [VMError]                           | EP had an error.                    |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1021                                | The container software at the EP    |                          |
    | | [ContainerError]                    | had an error.                       |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1022                                | The AP vacated the job.             |                          |
    | | [ScheddVacate]                      |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1023                                | The job was removed.                |                          |
    | | [JobRemoved]                        |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1024                                | An error occurred with the scratch  |                          |
    | | [ScratchDirError]                   | directory on the EP.                |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1025                                | A self-checkpoint job would have    |                          |
    | | [SuccessfulCheckpoint]              | restarted but could not reactivate  |                          |
    | |                                     | its claim.                          |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1026                                | Activation request had missing or   |                          |
    | | [ActivationRefusedBadRequest]       | invalid attributes                  |                          |
    | |                                     | its claim.                          |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1027                                | Activation request did not match    |                          |
    | | [ActivationRefusedNoMatch]          | slot requirements                   |                          |
    | |                                     | its claim.                          |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1028                                | Activation request refused because  |                          |
    | | [ActivationRefusedStillCleaning]    | Starter is still cleaning up after  |                          |
    | |                                     | a job                               |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1029                                | Activation request refused because  |                          |
    | | [ActivationRefusedWorklifeExpired]  | the claim worklife has expired      |                          |
    | |                                     |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1030                                | Activation request refused because  |                          |
    | | [ActivationRefusedPreempted]        | claim is being preempted            |                          |
    | |                                     |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1031                                | Activation request refused because  |                          |
    | | [ActivationRefusedBroken]           | slot is in the broken state         |                          |
    | |                                     |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1032                                | Activation request refused because  |                          |
    | | [ActivationRefusedNotIdle]          | slot is not idle                    |                          |
    | |                                     |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1033                                | Activation request refused because  |                          |
    | | [ActivationRefusedUnclaimed]        | slot is not claimed                 |                          |
    | |                                     |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1034                                | Activation request refused because  |                          |
    | | [ActivationRefusedClaimNotFound]    | claim id used for the request was   |                          |
    | |                                     | not found                           |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1035                                | Activation request refused because  |                          |
    | | [ActivationRefusedOldClaim]         | claim id used for the request was   |                          |
    | |                                     | is no longer valid                  |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
    | | 1036                                | Activation request refused because  |                          |
    | | [ActivationRefusedUnhealthy]        | a slot health check failed          |                          |
    | |                                     |                                     |                          |
    +---------------------------------------+-------------------------------------+--------------------------+
