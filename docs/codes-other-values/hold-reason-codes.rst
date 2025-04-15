Hold Reason Codes
=================

    +----------------------------------+-------------------------------------+--------------------------+
    | | Integer HoldReasonCode         | | Reason for Hold                   | | HoldReasonSubCode      |
    | | [NumHoldsByReason Label]       |                                     |                          |
    +==================================+=====================================+==========================+
    | | 1                              | The user put the job on             |                          |
    | | [UserRequest]                  | hold with :tool:`condor_hold`.      |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 3                              | The ``PERIODIC_HOLD``               | User Specified           |
    | | [JobPolicy]                    | expression evaluated to             |                          |
    |                                  | ``True``. Or,                       |                          |
    |                                  | ``ON_EXIT_HOLD`` was                |                          |
    |                                  | true                                |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 4                              | The credentials for the             |                          |
    | | [CorruptedCredential]          | job are invalid.                    |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 5                              | A job policy expression             |                          |
    | | [JobPolicyUndefined]           | evaluated to                        |                          |
    |                                  | ``Undefined``.                      |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 6                              | The *condor_starter*                | The Unix errno number.   |
    | | [FailedToCreateProcess]        | failed to start the                 |                          |
    |                                  | executable.                         |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 7                              | The standard output file            | The Unix errno number.   |
    | | [UnableToOpenOutput]           | for the job could not be            |                          |
    |                                  | opened.                             |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 8                              | The standard input file             | The Unix errno number.   |
    | | [UnableToOpenInput]            | for the job could not be            |                          |
    |                                  | opened.                             |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 9                              | The standard output                 | The Unix errno number.   |
    | | [UnableToOpenOutputStream]     | stream for the job could            |                          |
    |                                  | not be opened.                      |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 10                             | The standard input                  | The Unix errno number.   |
    | | [UnableToOpenInputStream]      | stream for the job could            |                          |
    |                                  | not be opened.                      |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 11                             | An internal HTCondor                |                          |
    | | [InvalidTransferAck]           | protocol error was                  |                          |
    |                                  | encountered when                    |                          |
    |                                  | transferring files.                 |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 12                             | An error occurred while             | The Unix errno number,   |
    | | [TransferOutputError]          | transferring job output files       | or a plug-in error       |
    |                                  | or self-checkpoint files.           | number; see below.       |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 13                             | An error occurred while             | The Unix errno number,   |
    | | [TransferInputError]           | transferring job input files.       | or a plug-in error       |
    |                                  |                                     | number; see below.       |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 14                             | The initial working                 | The Unix errno number.   |
    | | [IwdError]                     | directory of the job                |                          |
    |                                  | cannot be accessed.                 |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 15                             | The user requested the              |                          |
    | | [SubmittedOnHold]              | job be submitted on                 |                          |
    |                                  | hold.                               |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 16                             | Input files are being               |                          |
    | | [SpoolingInput]                | spooled.                            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 17                             | A standard universe job             |                          |
    | | [JobShadowMismatch]            | is not compatible with              |                          |
    |                                  | the *condor_shadow*                 |                          |
    |                                  | version available on the            |                          |
    |                                  | submitting machine.                 |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 18                             | An internal HTCondor                |                          |
    | | [InvalidTransferGoAhead]       | protocol error was                  |                          |
    |                                  | encountered when                    |                          |
    |                                  | transferring files.                 |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 19                             | :macro:`<Keyword>_HOOK_PREPARE_JOB` |                          |
    | | [HookPrepareJobFailure]        | was defined but could               |                          |
    |                                  | not be executed or                  |                          |
    |                                  | returned failure.                   |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 20                             | The job missed its                  |                          |
    | | [MissedDeferredExecutionTime]  | deferred execution time             |                          |
    |                                  | and therefore failed to             |                          |
    |                                  | run.                                |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 21                             | The job was put on hold             |                          |
    | | [StartdHeldJob]                | because :macro:`WANT_HOLD`          |                          |
    |                                  | in the machine policy               |                          |
    |                                  | was true.                           |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 22                             | Unable to initialize job            |                          |
    | | [UnableToInitUserLog]          | event log.                          |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 23                             | Failed to access user               |                          |
    | | [FailedToAccessUserAccount]    | account.                            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 24                             | No compatible shadow.               |                          |
    | | [NoCompatibleShadow]           |                                     |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 25                             | Invalid cron settings.              |                          |
    | | [InvalidCronSettings]          |                                     |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 26                             | :macro:`SYSTEM_PERIODIC_HOLD`       |                          |
    | | [SystemPolicy]                 | evaluated to true.                  |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 27                             | The system periodic job             |                          |
    | | [SystemPolicyUndefined]        | policy evaluated to                 |                          |
    |                                  | undefined.                          |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 32                             | The maximum total input             |                          |
    | | [MaxTransferInputSizeExceeded] | file transfer size was              |                          |
    |                                  | exceeded. (See                      |                          |
    |                                  | :macro:`MAX_TRANSFER_INPUT_MB`      |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 33                             | The maximum total output            |                          |
    | | [MaxTransferOutputSizeExceeded]| file transfer size was              |                          |
    |                                  | exceeded. (See                      |                          |
    |                                  | :macro:`MAX_TRANSFER_OUTPUT_MB`     |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 34                             | Memory usage exceeds a              |                          |
    | | [JobOutOfResources]            | memory limit.                       |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 35                             | Specified Docker image              |                          |
    | | [InvalidDockerImage]           | was invalid.                        |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 36                             | Job failed when sent the            |                          |
    | | [FailedToCheckpoint]           | checkpoint signal it                |                          |
    |                                  | requested.                          |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 37                             | User error in the EC2               |                          |
    | | [EC2UserError]                 | universe:                           |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Public key file not                 | 1                        |
    |                                  | defined.                            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Private key file not                | 2                        |
    |                                  | defined.                            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Grid resource string                | 4                        |
    |                                  | missing EC2 service URL.            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Failed to authenticate.             | 9                        |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Can't use existing SSH              | 10                       |
    |                                  | keypair with the given              |                          |
    |                                  | server's type.                      |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | You, or somebody like               | 20                       |
    |                                  | you, cancelled this                 |                          |
    |                                  | request.                            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 38                             | Internal error in the               |                          |
    | | [EC2InternalError]             | EC2 universe:                       |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Grid resource type not              | 3                        |
    |                                  | EC2.                                |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Grid resource type not              | 5                        |
    |                                  | set.                                |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Grid job ID is not for              | 7                        |
    |                                  | EC2.                                |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Unexpected remote job               | 21                       |
    |                                  | status.                             |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 39                             | Administrator error in              |                          |
    | | [EC2AdminError]                | the EC2 universe:                   |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | EC2_GAHP not defined.               | 6                        |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 40                             | Connection problem in               |                          |
    | | [EC2ConnectionProblem]         | the EC2 universe                    |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | ...while creating an SSH            | 11                       |
    |                                  | keypair.                            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | ...while starting an                | 12                       |
    |                                  | on-demand instance.                 |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | ...while requesting a spot          | 17                       |
    |                                  | instance.                           |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 41                             | Server error in the EC2             |                          |
    | | [EC2ServerError]               | universe:                           |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Abnormal instance                   | 13                       |
    |                                  | termination reason.                 |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Unrecognized instance               | 14                       |
    |                                  | termination reason.                 |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Resource was down for               | 22                       |
    |                                  | too long.                           |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 42                             | Instance potentially                |                          |
    | | [EC2InstancePotentiallyLost]   | lost due to an error in             |                          |
    |                                  | the EC2 universe:                   |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Connection error while              | 15                       |
    |                                  | terminating an instance.            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Failed to terminate                 | 16                       |
    |                                  | instance too many times.            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Connection error while              | 17                       |
    |                                  | terminating a spot                  |                          |
    |                                  | request.                            |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Failed to terminated a              | 18                       |
    |                                  | spot request too many               |                          |
    |                                  | times.                              |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    |                                  | Spot instance request               | 19                       |
    |                                  | purged before instance              |                          |
    |                                  | ID acquired.                        |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 43                             | Pre script failed.                  |                          |
    | | [PreScriptFailed]              |                                     |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    +----------------------------------+-------------------------------------+--------------------------+
    | | 44                             | Post script failed.                 |                          |
    | | [PostScriptFailed]             |                                     |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 45                             | Test of singularity runtime failed  |                          |
    | | [SingularityTestFailed]        | before launching a job              |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 46                             | The job's allowed duration was      |                          |
    | | [JobDurationExceeded]          | exceeded.                           |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 47                             | The job's allowed execution time    |                          |
    | | [JobExecuteExceeded]           | was exceeded.                       |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 48                             | Prepare job shadow hook failed      |                          |
    | | [HookShadowPrepareJobFailure]  | when it was executed;               |                          |
    |                                  | status code indicated job should be |                          |
    |                                  | held.                               |                          |
    +----------------------------------+-------------------------------------+--------------------------+

.. note::

    For hold codes 12 [TransferOutputError] and 13 [TransferInputError]:
    file transfer may invoke file-transfer plug-ins.  If it does, the hold
    subcodes may additionally be 62 (ETIME), if the file-transfer plug-in
    timed out; or the exit code of the plug-in shifted left by eight bits,
    otherwise.
