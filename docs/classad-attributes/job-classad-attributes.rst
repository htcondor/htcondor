      

Job ClassAd Attributes
======================

 ``Absent``:
    Boolean set to true ``True`` if the ad is absent.
 ``AcctGroup``:
    The accounting group name, as set in the submit description file via
    the **accounting\_group** command. This attribute is only present if
    an accounting group was requested by the submission. See
    section \ `3.6.7 <UserPrioritiesandNegotiation.html#x34-2390003.6.7>`__
    for more information about accounting groups.
 ``AcctGroupUser``:
    The user name associated with the accounting group. This attribute
    is only present if an accounting group was requested by the
    submission.
 ``AllRemoteHosts``:
    String containing a comma-separated list of all the remote machines
    running a parallel or mpi universe job.
 ``Args``:
    A string representing the command line arguments passed to the job,
    when those arguments are specified using the old syntax, as
    specified in section \ `12 <Condorsubmit.html#x149-108400012>`__.
 ``Arguments``:
    A string representing the command line arguments passed to the job,
    when those arguments are specified using the new syntax, as
    specified in section \ `12 <Condorsubmit.html#x149-108400012>`__.
 ``BatchQueue``:
    For grid universe jobs destined for PBS, LSF or SGE, the name of the
    queue in the remote batch system.
 ``BlockReadKbytes``:
    The integer number of KiB read from disk for this job.
 ``BlockReads``:
    The integer number of disk blocks read for this job.
 ``BlockWriteKbytes``:
    The integer number of KiB written to disk for this job.
 ``BlockWrites``:
    The integer number of blocks written to disk for this job.
 ``BoincAuthenticatorFile``:
    Used for grid type boinc jobs; a string taken from the definition of
    the submit description file command **boinc\_authenticator\_file**.
    Defines the path and file name of the file containing the
    authenticator string to use to authenticate to the BOINC service.
 ``CkptArch``:
    String describing the architecture of the machine this job executed
    on at the time it last produced a checkpoint. If the job has never
    produced a checkpoint, this attribute is ``undefined``.
 ``CkptOpSys``:
    String describing the operating system of the machine this job
    executed on at the time it last produced a checkpoint. If the job
    has never produced a checkpoint, this attribute is ``undefined``.
 ``ClusterId``:
    Integer cluster identifier for this job. A cluster is a group of
    jobs that were submitted together. Each job has its own unique job
    identifier within the cluster, but shares a common cluster
    identifier. The value changes each time a job or set of jobs are
    queued for execution under HTCondor.
 ``Cmd``:
    The path to and the file name of the job to be executed.
 ``CommittedTime``:
    The number of seconds of wall clock time that the job has been
    allocated a machine, excluding the time spent on run attempts that
    were evicted without a checkpoint. Like ``RemoteWallClockTime``,
    this includes time the job spent in a suspended state, so the total
    committed wall time spent running is

    ::

        CommittedTime - CommittedSuspensionTime

 ``CommittedSlotTime``:
    This attribute is identical to ``CommittedTime`` except that the
    time is multiplied by the ``SlotWeight`` of the machine(s) that ran
    the job. This relies on ``SlotWeight`` being listed in
    ``SYSTEM_JOB_MACHINE_ATTRS`` .
 ``CommittedSuspensionTime``:
    A running total of the number of seconds the job has spent in
    suspension during time in which the job was not evicted without a
    checkpoint. This number is updated when the job is checkpointed and
    when it exits.
 ``CompletionDate``:
    The time when the job completed, or the value 0 if the job has not
    yet completed. Measured in the number of seconds since the epoch
    (00:00:00 UTC, Jan 1, 1970).
 ``ConcurrencyLimits``:
    A string list, delimited by commas and space characters. The items
    in the list identify named resources that the job requires. The
    value can be a ClassAd expression which, when evaluated in the
    context of the job ClassAd and a matching machine ClassAd, results
    in a string list.
 ``CumulativeSlotTime``:
    This attribute is identical to ``RemoteWallClockTime`` except that
    the time is multiplied by the ``SlotWeight`` of the machine(s) that
    ran the job. This relies on ``SlotWeight`` being listed in
    ``SYSTEM_JOB_MACHINE_ATTRS`` .
 ``CumulativeSuspensionTime``:
    A running total of the number of seconds the job has spent in
    suspension for the life of the job.
 ``CumulativeTransferTime``:
    The total time, in seconds, that condor has spent transferring the
    input and output sandboxes for the life of the job.
 ``CurrentHosts``:
    The number of hosts in the claimed state, due to this job.
 ``DAGManJobId``:
    For a DAGMan node job only, the ``ClusterId`` job ClassAd attribute
    of the *condor\_dagman* job which is the parent of this node job.
    For nested DAGs, this attribute holds only the ``ClusterId`` of the
    job’s immediate parent.
 ``DAGParentNodeNames``:
    For a DAGMan node job only, a comma separated list of each *JobName*
    which is a parent node of this job’s node. This attribute is passed
    through to the job via the *condor\_submit* command line, if it does
    not exceed the line length defined with ``_POSIX_ARG_MAX``. For
    example, if a node job has two parents with *JobName*\ s B and C,
    the *condor\_submit* command line will contain

    ::

          -append +DAGParentNodeNames=B,C

 ``DAGManNodesLog``:
    For a DAGMan node job only, gives the path to an event log used
    exclusively by DAGMan to monitor the state of the DAG’s jobs. Events
    are written to this log file in addition to any log file specified
    in the job’s submit description file.
 ``DAGManNodesMask``:
    For a DAGMan node job only, a comma-separated list of the event
    codes that should be written to the log specified by
    ``DAGManNodesLog``, known as the auxiliary log. All events not
    specified in the ``DAGManNodesMask`` string are not written to the
    auxiliary event log. The value of this attribute is determined by
    DAGMan, and it is passed to the job via the *condor\_submit* command
    line. By default, the following events are written to the auxiliary
    job log:

    -  ``Submit``, event code is 0
    -  ``Execute``, event code is 1
    -  ``Executable error``, event code is 2
    -  ``Job evicted``, event code is 4
    -  ``Job terminated``, event code is 5
    -  ``Shadow exception``, event code is 7
    -  ``Job aborted``, event code is 9
    -  ``Job suspended``, event code is 10
    -  ``Job unsuspended``, event code is 11
    -  ``Job held``, event code is 12
    -  ``Job released``, event code is 13
    -  ``Post script terminated``, event code is 16
    -  ``Globus submit``, event code is 17
    -  ``Grid submit``, event code is 27

    If ``DAGManNodesLog`` is not defined, it has no effect. The value of
    ``DAGManNodesMask`` does not affect events recorded in the job event
    log file referred to by ``UserLog``.

 ``DelegateJobGSICredentialsLifetime``:
    An integer that specifies the maximum number of seconds for which
    delegated proxies should be valid. The default behavior is
    determined by the configuration setting
    ``DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`` , which defaults to one
    day. A value of 0 indicates that the delegated proxy should be valid
    for as long as allowed by the credential used to create the proxy.
    This setting currently only applies to proxies delegated for
    non-grid jobs and HTCondor-C jobs. It does not currently apply to
    globus grid jobs, which always behave as though this setting were 0.
    This setting has no effect if the configuration setting
    ``DELEGATE_JOB_GSI_CREDENTIALS`` is false, because in that case the
    job proxy is copied rather than delegated.
 ``DiskUsage``:
    Amount of disk space (KiB) in the HTCondor execute directory on the
    execute machine that this job has used. An initial value may be set
    at the job’s request, placing into the job’s submit description file
    a setting such as

    ::

          # 1 megabyte initial value 
          +DiskUsage = 1024

    **vm** universe jobs will default to an initial value of the disk
    image size. If not initialized by the job, non-**vm** universe jobs
    will default to an initial value of the sum of the job’s executable
    and all input files.

 ``EC2AccessKeyId``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_access\_key\_id**.
    Defines the path and file name of the file containing the EC2 Query
    API’s access key.
 ``EC2AmiID``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_ami\_id**. Identifies the
    machine image of the instance.
 ``EC2BlockDeviceMapping``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_block\_device\_mapping**.
    Defines the map from block device names to kernel device names for
    the instance.
 ``EC2ElasticIp``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_elastic\_ip**. Specifies
    an Elastic IP address to associate with the instance.
 ``EC2IamProfileArn``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_iam\_profile\_arn**.
    Specifies the IAM (instance) profile to associate with this
    instance.
 ``EC2IamProfileName``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_iam\_profile\_name**.
    Specifies the IAM (instance) profile to associate with this
    instance.
 ``EC2InstanceName``:
    Used for grid type ec2 jobs; a string set for the job once the
    instance starts running, as assigned by the EC2 service, that
    represents the unique ID assigned to the instance by the EC2
    service.
 ``EC2InstanceName``:
    Used for grid type ec2 jobs; a string set for the job once the
    instance starts running, as assigned by the EC2 service, that
    represents the unique ID assigned to the instance by the EC2
    service.
 ``EC2InstanceType``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_instance\_type**.
    Specifies a service-specific instance type.
 ``EC2KeyPair``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_keypair**. Defines the
    key pair associated with the EC2 instance.
 ``EC2ParameterNames``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_parameter\_names**.
    Contains a space or comma separated list of the names of additional
    parameters to pass when instantiating an instance.
 ``EC2SpotPrice``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_spot\_price**. Defines
    the maximum amount per hour a job submitter is willing to pay to run
    this job.
 ``EC2SpotRequestID``:
    Used for grid type ec2 jobs; identifies the spot request HTCondor
    made on behalf of this job.
 ``EC2StatusReasonCode``:
    Used for grid type ec2 jobs; reports the reason for the most recent
    EC2-level state transition. Can be used to determine if a spot
    request was terminated due to a rise in the spot price.
 ``EC2TagNames``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_tag\_names**. Defines the
    set, and case, of tags associated with the EC2 instance.
 ``EC2KeyPairFile``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_keypair\_file**. Defines
    the path and file name of the file into which to write the SSH key
    used to access the image, once it is running.
 ``EC2RemoteVirtualMachineName``:
    Used for grid type ec2 jobs; a string set for the job once the
    instance starts running, as assigned by the EC2 service, that
    represents the host name upon which the instance runs, such that the
    user can communicate with the running instance.
 ``EC2SecretAccessKey``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_secret\_access\_key**.
    Defines that path and file name of the file containing the EC2 Query
    API’s secret access key.
 ``EC2SecurityGroups``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_security\_groups**.
    Defines the list of EC2 security groups which should be associated
    with the job.
 ``EC2SecurityIDs``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_security\_ids**. Defines
    the list of EC2 security group IDs which should be associated with
    the job.
 ``EC2UserData``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_user\_data**. Defines a
    block of data that can be accessed by the virtual machine.
 ``EC2UserDataFile``:
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command **ec2\_user\_data\_file**.
    Specifies a path and file name of a file containing data that can be
    accessed by the virtual machine.
 ``EmailAttributes``:
    A string containing a comma-separated list of job ClassAd
    attributes. For each attribute name in the list, its value will be
    included in the e-mail notification upon job completion.
 ``EncryptExecuteDirectory``:
    A boolean value taken from the submit description file command
    **encrypt\_execute\_directory**. It specifies if HTCondor should
    encrypt the remote scratch directory on the machine where the job
    executes.
 ``EnteredCurrentStatus``:
    An integer containing the epoch time of when the job entered into
    its current status So for example, if the job is on hold, the
    ClassAd expression

    ::

            time() - EnteredCurrentStatus

    will equal the number of seconds that the job has been on hold.

 ``Env``:
    A string representing the environment variables passed to the job,
    when those arguments are specified using the old syntax, as
    specified in section \ `12 <Condorsubmit.html#x149-108400012>`__.
 ``Environment``:
    A string representing the environment variables passed to the job,
    when those arguments are specified using the new syntax, as
    specified in section \ `12 <Condorsubmit.html#x149-108400012>`__.
 ``ExecutableSize``:
    Size of the executable in KiB.
 ``ExitBySignal``:
    An attribute that is ``True`` when a user job exits via a signal and
    ``False`` otherwise. For some grid universe jobs, how the job exited
    is unavailable. In this case, ``ExitBySignal`` is set to ``False``.
 ``ExitCode``:
    When a user job exits by means other than a signal, this is the exit
    return code of the user job. For some grid universe jobs, how the
    job exited is unavailable. In this case, ``ExitCode`` is set to 0.
 ``ExitSignal``:
    When a user job exits by means of an unhandled signal, this
    attribute takes on the numeric value of the signal. For some grid
    universe jobs, how the job exited is unavailable. In this case,
    ``ExitSignal`` will be undefined.
 ``ExitStatus``:
    The way that HTCondor previously dealt with a job’s exit status.
    This attribute should no longer be used. It is not always accurate
    in heterogeneous pools, or if the job exited with a signal. Instead,
    see the attributes: ``ExitBySignal``, ``ExitCode``, and
    ``ExitSignal``.
 ``GceAuthFile``:
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command **gce\_auth\_file**. Defines the
    path and file name of the file containing authorization credentials
    to use the GCE service.
 ``GceImage``:
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command **gce\_image**. Identifies the
    machine image of the instance.
 ``GceJsonFile``:
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command **gce\_json\_file**. Specifies
    the path and file name of a file containing a set of JSON object
    members that should be added to the instance description submitted
    to the GCE service.
 ``GceMachineType``:
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command **gce\_machine\_type**.
    Specifies the hardware profile that should be used for a GCE
    instance.
 ``GceMetadata``:
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command **gce\_metadata**. Defines a set
    of name/value pairs that can be accessed by the virtual machine.
 ``GceMetadataFile``:
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command **gce\_metadata\_file**.
    Specifies a path and file name of a file containing a set of
    name/value pairs that can be accessed by the virtual machine.
 ``GcePreemptible``:
    Used for grid type gce jobs; a boolean taken from the definition of
    the submit description file command **gce\_preemptible**. Specifies
    whether the virtual machine instance created in GCE should be
    preemptible.
 ``GlobalJobId``:
    A string intended to be a unique job identifier within a pool. It
    currently contains the *condor\_schedd* daemon ``Name`` attribute, a
    job identifier composed of attributes ``ClusterId`` and ``ProcId``
    separated by a period, and the job’s submission time in seconds
    since 1970-01-01 00:00:00 UTC, separated by # characters. The value
    submit.example.com#152.3#1358363336 is an example.
 ``GridJobStatus``:
    A string containing the job’s status as reported by the remote job
    management system.
 ``GridResource``:
    A string defined by the right hand side of the the submit
    description file command **grid\_resource**. It specifies the target
    grid type, plus additional parameters specific to the grid type.
 ``HoldKillSig``:
    Currently only for scheduler and local universe jobs, a string
    containing a name of a signal to be sent to the job if the job is
    put on hold.
 ``HoldReason``:
    A string containing a human-readable message about why a job is on
    hold. This is the message that will be displayed in response to the
    command condor\_q -hold. It can be used to determine if a job should
    be released or not.
 ``HoldReasonCode``:
    An integer value that represents the reason that a job was put on
    hold.

    Integer Code

    Reason for Hold

    HoldReasonSubCode

    1

    The user put the job on hold with *condor\_hold*.

    2

    Globus middleware reported an error.

    The GRAM error number.

    3

    The ``PERIODIC_HOLD`` expression evaluated to ``True``. Or,
    ``ON_EXIT_HOLD`` was true

    User Specified

    4

    The credentials for the job are invalid.

    5

    A job policy expression evaluated to ``Undefined``.

    6

    The *condor\_starter* failed to start the executable.

    The Unix errno number.

    7

    The standard output file for the job could not be opened.

    The Unix errno number.

    8

    The standard input file for the job could not be opened.

    The Unix errno number.

    9

    The standard output stream for the job could not be opened.

    The Unix errno number.

    10

    The standard input stream for the job could not be opened.

    The Unix errno number.

    11

    An internal HTCondor protocol error was encountered when
    transferring files.

    12

    The *condor\_starter* or *condor\_shadow* failed to receive or write
    job files.

    The Unix errno number.

    13

    The *condor\_starter* or *condor\_shadow* failed to read or send job
    files.

    The Unix errno number.

    14

    The initial working directory of the job cannot be accessed.

    The Unix errno number.

    15

    The user requested the job be submitted on hold.

    16

    Input files are being spooled.

    17

    A standard universe job is not compatible with the *condor\_shadow*
    version available on the submitting machine.

    18

    An internal HTCondor protocol error was encountered when
    transferring files.

    19

    ``<Keyword>_HOOK_PREPARE_JOB`` was defined but could not be executed
    or returned failure.

    20

    The job missed its deferred execution time and therefore failed to
    run.

    21

    The job was put on hold because ``WANT_HOLD`` in the machine policy
    was true.

    22

    Unable to initialize job event log.

    23

    Failed to access user account.

    24

    No compatible shadow.

    25

    Invalid cron settings.

    26

    ``SYSTEM_PERIODIC_HOLD`` evaluated to true.

    27

    The system periodic job policy evaluated to undefined.

    28

    Failed while using glexec to set up the job’s working directory
    (chown sandbox to the user).

    30

    Failed while using glexec to prepare output for transfer (chown
    sandbox to condor).

    32

    The maximum total input file transfer size was exceeded. (See
    ``MAX_TRANSFER_INPUT_MB`` .)

    33

    The maximum total output file transfer size was exceeded. (See
    ``MAX_TRANSFER_OUTPUT_MB`` .)

    34

    Memory usage exceeds a memory limit.

    35

    Specified Docker image was invalid.

    36

    Job failed when sent the checkpoint signal it requested.

    37

    User error in the EC2 universe:

    Public key file not defined.

    1

    Private key file not defined.

    2

    Grid resource string missing EC2 service URL.

    4

    Failed to authenticate.

    9

    Can’t use existing SSH keypair with the given server’s type.

    10

    You, or somebody like you, cancelled this request.

    20

    38

    Internal error in the EC2 universe:

    Grid resource type not EC2.

    3

    Grid resource type not set.

    5

    Grid job ID is not for EC2.

    7

    Unexpected remote job status.

    21

    39

    Adminstrator error in the EC2 universe:

    EC2\_GAHP not defined.

    6

    40

    Connection problem in the EC2 universe

    …while creating an SSH keypair.

    11

    …while starting an on-demand instance.

    12

    …while requesting a spot instance.

    17

    41

    Server error in the EC2 universe:

    Abnormal instance termination reason.

    13

    Unrecognized instance termination reason.

    14

    Resource was down for too long.

    22

    42

    Instance potentially lost due to an error in the EC2 universe:

    Connection error while terminating an instance.

    15

    Failed to terminate instance too many times.

    16

    Connection error while terminating a spot request.

    17

    Failed to terminated a spot request too many times.

    18

    Spot instance request purged before instance ID acquired.

    19

    43

    Pre script failed.

    44

    Post script failed.

 ``HoldReasonSubCode``:
    An integer value that represents further information to go along
    with the ``HoldReasonCode``, for some values of ``HoldReasonCode``.
    See ``HoldReasonCode`` for the values.
 ``HookKeyword``:
    A string that uniquely identifies a set of job hooks, and added to
    the ClassAd once a job is fetched.
 ``ImageSize``:
    Maximum observed memory image size (i.e. virtual memory) of the job
    in KiB. The initial value is equal to the size of the executable for
    non-vm universe jobs, and 0 for vm universe jobs. When the job
    writes a checkpoint, the ``ImageSize`` attribute is set to the size
    of the checkpoint file (since the checkpoint file contains the job’s
    memory image). A vanilla universe job’s ``ImageSize`` is recomputed
    internally every 15 seconds. How quickly this updated information
    becomes visible to *condor\_q* is controlled by
    ``SHADOW_QUEUE_UPDATE_INTERVAL`` and ``STARTER_UPDATE_INTERVAL``.

    Under Linux, ``ProportionalSetSize`` is a better indicator of memory
    usage for jobs with significant sharing of memory between processes,
    because ``ImageSize`` is simply the sum of virtual memory sizes
    across all of the processes in the job, which may count the same
    memory pages more than once.

 ``IOWait``:
    I/O wait time of the job recorded by the cgroup controller in
    seconds.
 ``IwdFlushNFSCache``:
    A boolean expression that controls whether or not HTCondor attempts
    to flush a submit machine’s NFS cache, in order to refresh an
    HTCondor job’s initial working directory. The value will be
    ``True``, unless a job explicitly adds this attribute, setting it to
    ``False``.
 ``JobAdInformationAttrs``:
    A comma-separated list of attribute names. The named attributes and
    their values are written in the job event log whenever any event is
    being written to the log. This is the same as the configuration
    setting ``EVENT_LOG_INFORMATION_ATTRS`` (see
    page \ `617 <ConfigurationMacros.html#x33-1890003.5.2>`__) but it
    applies to the job event log instead of the system event log.
 ``JobDescription``:
    A string that may be defined for a job by setting **description** in
    the submit description file. When set, tools which display the
    executable such as *condor\_q* will instead use this string. For
    interactive jobs that do not have a submit description file, this
    string will default to ``"Interactive job"``.
 ``JobCurrentStartDate``:
    Time at which the job most recently began running. Measured in the
    number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).
 ``JobCurrentStartExecutingDate``:
    Time at which the job most recently finished transferring its input
    sandbox and began executing. Measured in the number of seconds since
    the epoch (00:00:00 UTC, Jan 1, 1970)
 ``JobCurrentStartTransferOutputDate``:
    Time at which the job most recently finished executing and began
    transferring its output sandbox. Measured in the number of seconds
    since the epoch (00:00:00 UTC, Jan 1, 1970)
 ``JobLeaseDuration``:
    The number of seconds set for a job lease, the amount of time that a
    job may continue running on a remote resource, despite its
    submitting machine’s lack of response. See
    section \ `2.14.4 <SpecialEnvironmentConsiderations.html#x26-1440002.14.4>`__
    for details on job leases.
 ``JobMaxVacateTime``:
    An integer expression that specifies the time in seconds requested
    by the job for being allowed to gracefully shut down.
 ``JobNotification``:
    An integer indicating what events should be emailed to the user. The
    integer values correspond to the user choices for the submit command
    **notification**.

    --------------

    Value

    Notification value

    0

    Never

    1

    Always

    2

    Complete

    3

    Error

    --------------

    --------------

    --------------

 ``JobPrio``:
    Integer priority for this job, set by *condor\_submit* or
    *condor\_prio*. The default value is 0. The higher the number, the
    greater (better) the priority.
 ``JobRunCount``:
    This attribute is retained for backwards compatibility. It may go
    away in the future. It is equivalent to ``NumShadowStarts`` for all
    universes except **scheduler**. For the **scheduler** universe, this
    attribute is equivalent to ``NumJobStarts``.
 ``JobStartDate``:
    Time at which the job first began running. Measured in the number of
    seconds since the epoch (00:00:00 UTC, Jan 1, 1970). Due to a long
    standing bug in the 8.6 series and earlier, the job classad that is
    internal to the *condor\_startd* and *condor\_starter* sets this to
    the time that the job most recently began executing. This bug is
    scheduled to be fixed in the 8.7 series.
 ``JobStatus``:
    Integer which indicates the current status of the job.

    --------------

    Value

    Status

    1

    Idle

    2

    Running

    3

    Removed

    4

    Completed

    5

    Held

    6

    Transferring Output

    7

    Suspended

    --------------

    --------------

    --------------

 ``JobUniverse``:
    Integer which indicates the job universe.

    --------------

    Value

    Universe

    1

    standard

    5

    vanilla, docker

    7

    scheduler

    8

    MPI

    9

    grid

    10

    java

    11

    parallel

    12

    local

    13

    vm

    --------------

    --------------

    --------------

 ``KeepClaimIdle``:
    An integer value that represents the number of seconds that the
    *condor\_schedd* will continue to keep a claim, in the Claimed Idle
    state, after the job with this attribute defined completes, and
    there are no other jobs ready to run from this user. This attribute
    may improve the performance of linear DAGs, in the case when a
    dependent job can not be scheduled until its parent has completed.
    Extending the claim on the machine may permit the dependent job to
    be scheduled with less delay than with waiting for the
    *condor\_negotiator* to match with a new machine.
 ``KillSig``:
    The Unix signal number that the job wishes to be sent before being
    forcibly killed. It is relevant only for jobs running on Unix
    machines.
 ``KillSigTimeout``:
    This attribute is replaced by the functionality in
    ``JobMaxVacateTime`` as of HTCondor version 7.7.3. The number of
    seconds that the job (other than the standard universe) requests the
    *condor\_starter* wait after sending the signal defined as
    ``KillSig`` and before forcibly removing the job. The actual amount
    of time will be the minimum of this value and the execute machine’s
    configuration variable ``KILLING_TIMEOUT`` .
 ``LastCheckpointPlatform``:
    An opaque string which is the ``CheckpointPlatform`` identifier from
    the last machine where this standard universe job had successfully
    produced a checkpoint.
 ``LastCkptServer``:
    Host name of the last checkpoint server used by this job. When a
    pool is using multiple checkpoint servers, this tells the job where
    to find its checkpoint file.
 ``LastCkptTime``:
    Time at which the job last performed a successful checkpoint.
    Measured in the number of seconds since the epoch (00:00:00 UTC, Jan
    1, 1970).
 ``LastMatchTime``:
    An integer containing the epoch time when the job was last
    successfully matched with a resource (gatekeeper) Ad.
 ``LastRejMatchReason``:
    If, at any point in the past, this job failed to match with a
    resource ad, this attribute will contain a string with a
    human-readable message about why the match failed.
 ``LastRejMatchTime``:
    An integer containing the epoch time when HTCondor-G last tried to
    find a match for the job, but failed to do so.
 ``LastRemotePool``:
    The name of the *condor\_collector* of the pool in which a job ran
    via flocking in the most recent run attempt. This attribute is not
    defined if the job did not run via flocking.
 ``LastSuspensionTime``:
    Time at which the job last performed a successful suspension.
    Measured in the number of seconds since the epoch (00:00:00 UTC, Jan
    1, 1970).
 ``LastVacateTime``:
    Time at which the job was last evicted from a remote workstation.
    Measured in the number of seconds since the epoch (00:00:00 UTC, Jan
    1, 1970).
 ``LeaveJobInQueue``:
    A boolean expression that defaults to ``False``, causing the job to
    be removed from the queue upon completion. An exception is if the
    job is submitted using ``condor_submit -spool``. For this case, the
    default expression causes the job to be kept in the queue for 10
    days after completion.
 ``LocalSysCpu``:
    An accumulated number of seconds of system CPU time that the job
    caused to the machine upon which the job was submitted.
 ``LocalUserCpu``:
    An accumulated number of seconds of user CPU time that the job
    caused to the machine upon which the job was submitted.
 ``MachineAttr<X><N>``:
    Machine attribute of name ``<X>`` that is placed into this job
    ClassAd, as specified by the configuration variable
    ``SYSTEM_JOB_MACHINE_ATTRS``. With the potential for multiple run
    attempts, ``<N>`` represents an integer value providing historical
    values of this machine attribute for multiple runs. The most recent
    run will have a value of ``<N>`` equal to ``0``. The next most
    recent run will have a value of ``<N>`` equal to ``1``.
 ``MaxHosts``:
    The maximum number of hosts that this job would like to claim. As
    long as ``CurrentHosts`` is the same as ``MaxHosts``, no more hosts
    are negotiated for.
 ``MaxJobRetirementTime``:
    Maximum time in seconds to let this job run uninterrupted before
    kicking it off when it is being preempted. This can only decrease
    the amount of time from what the corresponding startd expression
    allows.
 ``MaxTransferInputMB``:
    This integer expression specifies the maximum allowed total size in
    Mbytes of the input files that are transferred for a job. This
    expression does not apply to grid universe, standard universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the system setting ``MAX_TRANSFER_INPUT_MB`` is
    used. If the observed size of all input files at submit time is
    larger than the limit, the job will be immediately placed on hold
    with a ``HoldReasonCode`` value of 32. If the job passes this
    initial test, but the size of the input files increases or the limit
    decreases so that the limit is violated, the job will be placed on
    hold at the time when the file transfer is attempted.
 ``MaxTransferOutputMB``:
    This integer expression specifies the maximum allowed total size in
    Mbytes of the output files that are transferred for a job. This
    expression does not apply to grid universe, standard universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the system setting ``MAX_TRANSFER_OUTPUT_MB`` is
    used. If the total size of the job’s output files to be transferred
    is larger than the limit, the job will be placed on hold with a
    ``HoldReasonCode`` value of 33. The output will be transferred up to
    the point when the limit is hit, so some files may be fully
    transferred, some partially, and some not at all.
 ``MemoryUsage``:
    An integer expression in units of Mbytes that represents the peak
    memory usage for the job. Its purpose is to be compared with the
    value defined by a job with the **request\_memory** submit command,
    for purposes of policy evaluation.
 ``MinHosts``:
    The minimum number of hosts that must be in the claimed state for
    this job, before the job may enter the running state.
 ``NextJobStartDelay``:
    An integer number of seconds delay time after this job starts until
    the next job is started. The value is limited by the configuration
    variable ``MAX_NEXT_JOB_START_DELAY`` .
 ``NiceUser``:
    Boolean value which when ``True`` indicates that this job is a nice
    job, raising its user priority value, thus causing it to run on a
    machine only when no other HTCondor jobs want the machine.
 ``Nonessential``:
    A boolean value only relevant to grid universe jobs, which when
    ``True`` tells HTCondor to simply abort (remove) any problematic
    job, instead of putting the job on hold. It is the equivalent of
    doing *condor\_rm* followed by *condor\_rm* **-forcex** any time the
    job would have otherwise gone on hold. If not explicitly set to
    ``True``, the default value will be ``False``.
 ``NTDomain``:
    A string that identifies the NT domain under which a job’s owner
    authenticates on a platform running Windows.
 ``NumCkpts``:
    A count of the number of checkpoints written by this job during its
    lifetime.
 ``NumGlobusSubmits``:
    An integer that is incremented each time the *condor\_gridmanager*
    receives confirmation of a successful job submission into Globus.
 ``NumJobCompletions``:
    An integer, initialized to zero, that is incremented by the
    *condor\_shadow* each time the job’s executable exits of its own
    accord, with or without errors, and successfully completes file
    transfer (if requested). Jobs which have done so normally enter the
    completed state; this attribute is therefore normally only of use
    when, for example, ``on_exit_remove`` or ``on_exit_hold`` is set.
 ``NumJobMatches``:
    An integer that is incremented by the *condor\_schedd* each time the
    job is matched with a resource ad by the negotiator.
 ``NumJobReconnects``:
    An integer count of the number of times a job successfully
    reconnected after being disconnected. This occurs when the
    *condor\_shadow* and *condor\_starter* lose contact, for example
    because of transient network failures or a *condor\_shadow* or
    *condor\_schedd* restart. This attribute is only defined for jobs
    that can reconnected: those in the **vanilla** and **java**
    universes.
 ``NumJobStarts``:
    An integer count of the number of times the job started executing.
    This is not (yet) defined for **standard** universe jobs.
 ``NumPids``:
    A count of the number of child processes that this job has.
 ``NumRestarts``:
    A count of the number of restarts from a checkpoint attempted by
    this job during its lifetime.
 ``NumShadowExceptions``:
    An integer count of the number of times the *condor\_shadow* daemon
    had a fatal error for a given job.
 ``NumShadowStarts``:
    An integer count of the number of times a *condor\_shadow* daemon
    was started for a given job. This attribute is not defined for
    **scheduler** universe jobs, since they do not have a
    *condor\_shadow* daemon associated with them. For **local** universe
    jobs, this attribute is defined, even though the process that
    manages the job is technically a *condor\_starter* rather than a
    *condor\_shadow*. This keeps the management of the local universe
    and other universes as similar as possible. **Note that this
    attribute is incremented every time the job is matched, even if the
    match is rejected by the execute machine; in other words, the value
    of this attribute may be greater than the number of times the job
    actually ran.**
 ``NumSystemHolds``:
    An integer that is incremented each time HTCondor-G places a job on
    hold due to some sort of error condition. This counter is useful,
    since HTCondor-G will always place a job on hold when it gives up on
    some error condition. Note that if the user places the job on hold
    using the *condor\_hold* command, this attribute is not incremented.
 ``OtherJobRemoveRequirements``:
    A string that defines a list of jobs. When the job with this
    attribute defined is removed, all other jobs defined by the list are
    also removed. The string is an expression that defines a constraint
    equivalent to the one implied by the command

    ::

          condor_rm -constraint <constraint>

    This attribute is used for jobs managed with *condor\_dagman* to
    ensure that node jobs of the DAG are removed when the
    *condor\_dagman* job itself is removed. Note that the list of jobs
    defined by this attribute must not form a cyclic removal of jobs, or
    the *condor\_schedd* will go into an infinite loop when any of the
    jobs is removed.

 ``OutputDestination``:
    A URL, as defined by submit command **output\_destination**.
 ``Owner``:
    String describing the user who submitted this job.
 ``ParallelShutdownPolicy``:
    A string that is only relevant to parallel universe jobs. Without
    this attribute defined, the default policy applied to parallel
    universe jobs is to consider the whole job completed when the first
    node exits, killing processes running on all remaining nodes. If
    defined to the following strings, HTCondor’s behavior changes:

     ``"WAIT_FOR_ALL"``
        HTCondor will wait until every node in the parallel job has
        completed to consider the job finished.

 ``PostArgs``:
    Defines the command-line arguments for the post command using the
    old argument syntax, as specified in
    section \ `12 <Condorsubmit.html#x149-108400012>`__. If both
    ``PostArgs`` and ``PostArguments`` exists, the former is ignored.
 ``PostArguments``:
    Defines the command-line arguments for the post command using the
    new argument syntax, as specified in
    section \ `12 <Condorsubmit.html#x149-108400012>`__, excepting that
    double quotes must be escaped with a backslash instead of another
    double quote. If both ``PostArgs`` and ``PostArguments`` exists, the
    former is ignored.
 ``PostCmd``:
    A job in the vanilla, Docker, Java, or virtual machine universes may
    specify a command to run after the **Executable** has exited, but
    before file transfer is started. Unlike a DAGMan POST script
    command, this command is run on the execute machine; however, it is
    not run in the same environment as the **Executable**. Instead, its
    environment is set by ``PostEnv`` or ``PostEnvironment``. Like the
    DAGMan POST script command, this command is not run in the same
    universe as the **Executable**; in particular, this command is not
    run in a Docker container, nor in a virtual machine, nor in Java.
    This command is also not run with any of vanilla universe’s features
    active, including (but not limited to): cgroups, PID namespaces,
    bind mounts, CPU affinity, Singularity, or job wrappers. This
    command is not automatically transferred with the job, so if you’re
    using file transfer, you must add it to the
    **transfer\_input\_files** list.

    If the specified command is in the job’s execute directory, or any
    sub-directory, you should not set **vm\_no\_output\_vm**, as that
    will delete all the files in the job’s execute directory before this
    command has a chance to run. If you don’t want any output back from
    your VM universe job, but you do want to run a post command, do not
    set **vm\_no\_output\_vm** and instead delete the job’s execute
    directory in your post command.

 ``PostCmdExitBySignal``:
    If ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` were set,
    and the post command has run, this attribute will true if the the
    post command exited on a signal and false if it did not. It is
    otherwise unset.
 ``PostCmdExitCode``:
    If ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` were set,
    the post command has run, and the post command did not exit on a
    signal, then this attribute will be set to the exit code. It is
    otherwise unset.
 ``PostCmdExitSignal``:
    If ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` were set,
    the post command has run, and the post command exited on a signal,
    then this attribute will be set to that signal. It is otherwise
    unset.
 ``PostEnv``:
    Defines the environment for the Postscript using the Old environment
    syntax. If both ``PostEnv`` and ``PostEnvironment`` exist, the
    former is ignored.
 ``PostEnvironment``:
    Defines the environment for the Postscript using the New environment
    syntax. If both ``PostEnv`` and ``PostEnvironment`` exist, the
    former is ignored.
 ``PreArgs``:
    Defines the command-line arguments for the pre command using the old
    argument syntax, as specified in
    section \ `12 <Condorsubmit.html#x149-108400012>`__. If both
    ``PreArgs`` and ``PreArguments`` exists, the former is ignored.
 ``PreArguments``:
    Defines the command-line arguments for the pre command using the new
    argument syntax, as specified in
    section \ `12 <Condorsubmit.html#x149-108400012>`__, excepting that
    double quotes must be escape with a backslash instead of another
    double quote. If both ``PreArgs`` and ``PreArguments`` exists, the
    former is ignored.
 ``PreCmd``:
    A job in the vanilla, Docker, Java, or virtual machine universes may
    specify a command to run after file transfer (if any) completes but
    before the **Executable** is started. Unlike a DAGMan PRE script
    command, this command is run on the execute machine; however, it is
    not run in the same environment as the **Executable**. Instead, its
    environment is set by ``PreEnv`` or ``PreEnvironment``. Like the
    DAGMan POST script command, this command is not run in the same
    universe as the **Executable**; in particular, this command is not
    run in a Docker container, nor in a virtual machine, nor in Java.
    This command is also not run with any of vanilla universe’s features
    active, including (but not limited to): cgroups, PID namespaces,
    bind mounts, CPU affinity, Singularity, or job wrappers. This
    command is not automatically transferred with the job, so if you’re
    using file transfer, you must add it to the
    **transfer\_input\_files** list.
 ``PreCmdExitBySignal``:
    If ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` were set, and
    the pre command has run, this attribute will true if the the pre
    command exited on a signal and false if it did not. It is otherwise
    unset.
 ``PreCmdExitCode``:
    If ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` were set, the
    pre command has run, and the pre command did not exit on a signal,
    then this attribute will be set to the exit code. It is otherwise
    unset.
 ``PreCmdExitSignal``:
    If ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` were set, the
    pre command has run, and the pre command exited on a signal, then
    this attribute will be set to that signal. It is otherwise unset.
 ``PreEnv``:
    Defines the environment for the prescript using the Old environment
    syntax. If both ``PreEnv`` and ``PreEnvironment`` exist, the former
    is ignored.
 ``PreEnvironment``:
    Defines the environment for the prescript using the New environment
    syntax. If both ``PreEnv`` and ``PreEnvironment`` exist, the former
    is ignored.
 ``PreJobPrio1``:
    An integer value representing a user’s priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered first: before ``PreJobPrio2``,
    before ``JobPrio``, before ``PostJobPrio1``, before
    ``PostJobPrio2``, and before ``QDate``.
 ``PreJobPrio2``:
    An integer value representing a user’s priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after ``PreJobPrio1``, but before
    ``JobPrio``, before ``PostJobPrio1``, before ``PostJobPrio2``, and
    before ``QDate``.
 ``PostJobPrio1``:
    An integer value representing a user’s priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after ``PreJobPrio1``, after
    ``PreJobPrio1``, and after ``JobPrio``, but before ``PostJobPrio2``,
    and before ``QDate``.
 ``PostJobPrio2``:
    An integer value representing a user’s priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after ``PreJobPrio1``, after
    ``PreJobPrio1``, after ``JobPrio``, and after ``PostJobPrio1``, but
    before ``QDate``.
 ``PreserveRelativeExecutable``:
    When ``True``, the *condor\_starter* will not prepend ``Iwd`` to
    ``Cmd``, when ``Cmd`` is a relative path name and
    ``TransferExecutable`` is ``False``. The default value is ``False``.
    This attribute is primarily of interest for users of
    ``USER_JOB_WRAPPER`` for the purpose of allowing an executable’s
    location to be resolved by the user’s path in the job wrapper.
 ``ProcId``:
    Integer process identifier for this job. Within a cluster of many
    jobs, each job has the same ``ClusterId``, but will have a unique
    ``ProcId``. Within a cluster, assignment of a ``ProcId`` value will
    start with the value 0. The job (process) identifier described here
    is unrelated to operating system PIDs.
 ``ProportionalSetSizeKb``:
    On Linux execute machines with kernel version more recent than
    2.6.27, this is the maximum observed proportional set size (PSS) in
    KiB, summed across all processes in the job. If the execute machine
    does not support monitoring of PSS or PSS has not yet been measured,
    this attribute will be undefined. PSS differs from ``ImageSize`` in
    how memory shared between processes is accounted. The PSS for one
    process is the sum of that process’ memory pages divided by the
    number of processes sharing each of the pages. ``ImageSize`` is the
    same, except there is no division by the number of processes sharing
    the pages.
 ``QDate``:
    Time at which the job was submitted to the job queue. Measured in
    the number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).
 ``RecentBlockReadKbytes``:
    The integer number of KiB read from disk for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.
 ``RecentBlockReads``:
    The integer number of disk blocks read for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.
 ``RecentBlockWriteKbytes``:
    The integer number of KiB written to disk for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.
 ``RecentBlockWrites``:
    The integer number of blocks written to disk for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.
 ``ReleaseReason``:
    A string containing a human-readable message about why the job was
    released from hold.
 ``RemoteIwd``:
    The path to the directory in which a job is to be executed on a
    remote machine.
 ``RemotePool``:
    The name of the *condor\_collector* of the pool in which a job is
    running via flocking. This attribute is not defined if the job is
    not running via flocking.
 ``RemoteSysCpu``:
    The total number of seconds of system CPU time (the time spent at
    system calls) the job used on remote machines. This does not count
    time spent on run attempts that were evicted without a checkpoint.
 ``CumulativeRemoteSysCpu``:
    The total number of seconds of system CPU time the job used on
    remote machines, summed over all execution attempts.
 ``RemoteUserCpu``:
    The total number of seconds of user CPU time the job used on remote
    machines. This does not count time spent on run attempts that were
    evicted without a checkpoint. A job in the virtual machine universe
    will only report this attribute if run on a KVM hypervisor.
 ``CumulativeRemoteUserCpu``:
    The total number of seconds of user CPU time the job used on remote
    machines, summed over all execution attempts.
 ``RemoteWallClockTime``:
    Cumulative number of seconds the job has been allocated a machine.
    This also includes time spent in suspension (if any), so the total
    real time spent running is

    ::

        RemoteWallClockTime - CumulativeSuspensionTime

    Note that this number does not get reset to zero when a job is
    forced to migrate from one machine to another. ``CommittedTime``, on
    the other hand, is just like ``RemoteWallClockTime`` except it does
    get reset to 0 whenever the job is evicted without a checkpoint.

 ``RemoveKillSig``:
    Currently only for scheduler universe jobs, a string containing a
    name of a signal to be sent to the job if the job is removed.
 ``RequestCpus``:
    The number of CPUs requested for this job. If dynamic
    *condor\_startd* provisioning is enabled, it is the minimum number
    of CPUs that are needed in the created dynamic slot.
 ``RequestDisk``:
    The amount of disk space in KiB requested for this job. If dynamic
    *condor\_startd* provisioning is enabled, it is the minimum amount
    of disk space needed in the created dynamic slot.
 ``RequestedChroot``:
    A full path to the directory that the job requests the
    *condor\_starter* use as an argument to chroot().
 ``RequestMemory``:
    The amount of memory space in MiB requested for this job. If dynamic
    *condor\_startd* provisioning is enabled, it is the minimum amount
    of memory needed in the created dynamic slot. If not set by the job,
    its definition is specified by configuration variable
    ``JOB_DEFAULT_REQUESTMEMORY`` .
 ``Requirements``:
    A classad expression evaluated by the *condor\_negotiator*,
    *condor\_schedd*, and {Condorstartd in the context of slot ad. If
    true, this job is eligible to run on that slot. If the job
    requirements does not mention the (startd) attribute ``OPSYS`` , the
    schedd will append a clause to Requirements forcing the job to match
    the same ``OPSYS`` as the submit machine. The schedd appends a
    simliar clause to match the ``ARCH`` . The schedd parameter
    ``APPEND_REQUIREMENTS`` , will, if set, append that value to every
    job’s requirements expression.
 ``ResidentSetSize``:
    Maximum observed physical memory in use by the job in KiB while
    running.
 ``StackSize``:
    Utilized for Linux jobs only, the number of bytes allocated for
    stack space for this job. This number of bytes replaces the default
    allocation of 512 Mbytes.
 ``StageOutFinish``:
    An attribute representing a Unix epoch time that is defined for a
    job that is spooled to a remote site using ``condor_submit -spool``
    or HTCondor-C and causes HTCondor to hold the output in the spool
    while the job waits in the queue in the ``Completed`` state. This
    attribute is defined when retrieval of the output finishes.
 ``StageOutStart``:
    An attribute representing a Unix epoch time that is defined for a
    job that is spooled to a remote site using ``condor_submit -spool``
    or HTCondor-C and causes HTCondor to hold the output in the spool
    while the job waits in the queue in the ``Completed`` state. This
    attribute is defined when retrieval of the output begins.
 ``StreamErr``:
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, and ``TransferErr`` is ``True``, then
    standard error is streamed back to the submit machine, instead of
    doing the transfer (as a whole) after the job completes. If
    ``False``, then standard error is transferred back to the submit
    machine (as a whole) after the job completes. If ``TransferErr`` is
    ``False``, then this job attribute is ignored.
 ``StreamOut``:
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, and ``TransferOut`` is ``True``, then job
    output is streamed back to the submit machine, instead of doing the
    transfer (as a whole) after the job completes. If ``False``, then
    job output is transferred back to the submit machine (as a whole)
    after the job completes. If ``TransferOut`` is ``False``, then this
    job attribute is ignored.
 ``SubmitterAutoregroup``:
    A boolean attribute defined by the *condor\_negotiator* when it
    makes a match. It will be ``True`` if the resource was claimed via
    negotiation when the configuration variable ``GROUP_AUTOREGROUP``
    was ``True``. It will be ``False`` otherwise.
 ``SubmitterGlobalJobId``:
    When HTCondor-C submits a job to a remote *condor\_schedd*, it sets
    this attribute in the remote job ad to match the ``GlobalJobId``
    attribute of the original, local job.
 ``SubmitterGroup``:
    The accounting group name defined by the *condor\_negotiator* when
    it makes a match.
 ``SubmitterNegotiatingGroup``:
    The accounting group name under which the resource negotiated when
    it was claimed, as set by the *condor\_negotiator*.
 ``SuccessPreExitBySignal``:
    Specifies if a succesful pre command must exit with a signal.
 ``SuccessPreExitCode``:
    Specifies the code with which the pre command must exit to be
    considered successful. Pre commands which are not successful cause
    the job to go on hold with ``ExitCode`` set to ``PreCmdExitCode``.
    The exit status of a pre command without one of
    ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` defined is
    ignored.
 ``SuccessPreExitSignal``:
    Specifies the signal on which the pre command must exit be
    considered successful. Pre commands which are not successful cause
    the job to go on hold with ``ExitSignal`` set to
    ``PreCmdExitSignal``. The exit status of a pre command without one
    of ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` defined is
    ignored.
 ``SuccessPostExitBySignal``:
    Specifies if a succesful post command must exit with a signal.
 ``SuccessPostExitCode``:
    Specifies the code with which the post command must exit to be
    considered successful. Post commands which are not successful cause
    the job to go on hold with ``ExitCode`` set to ``PostCmdExitCode``.
    The exit status of a post command without one of
    ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` defined is
    ignored.
 ``SuccessPostExitSignal``:
    Specifies the signal on which the post command must exit be
    considered successful. Post commands which are not successful cause
    the job to go on hold with ``ExitSignal`` set to
    ``PostCmdExitSignal``. The exit status of a post command without one
    of ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` defined is
    ignored.
 ``TotalSuspensions``:
    A count of the number of times this job has been suspended during
    its lifetime.
 ``TransferErr``:
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the error output from the job is
    transferred from the remote machine back to the submit machine. The
    name of the file after transfer is the file referred to by job
    attribute ``Err``. If ``False``, no transfer takes place (remote to
    submit machine), and the name of the file is the file referred to by
    job attribute ``Err``.
 ``TransferExecutable``:
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the job executable is transferred
    from the submit machine to the remote machine. The name of the file
    (on the submit machine) that is transferred is given by the job
    attribute ``Cmd``. If ``False``, no transfer takes place, and the
    name of the file used (on the remote machine) will be as given in
    the job attribute ``Cmd``.
 ``TransferIn``:
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the job input is transferred from the
    submit machine to the remote machine. The name of the file that is
    transferred is given by the job attribute ``In``. If ``False``, then
    the job’s input is taken from a file on the remote machine
    (pre-staged), and the name of the file is given by the job attribute
    ``In``.
 ``TransferInputSizeMB``:
    The total size in Mbytes of input files to be transferred for the
    job. Files transferred via file transfer plug-ins are not included.
    This attribute is automatically set by *condor\_submit*; jobs
    submitted via other submission methods, such as SOAP, may not define
    this attribute.
 ``TransferOut``:
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the output from the job is
    transferred from the remote machine back to the submit machine. The
    name of the file after transfer is the file referred to by job
    attribute ``Out``. If ``False``, no transfer takes place (remote to
    submit machine), and the name of the file is the file referred to by
    job attribute ``Out``.
 ``TransferringInput``:
    A boolean value that indicates whether the job is currently
    transferring input files. The value is ``Undefined`` if the job is
    not scheduled to run or has not yet attempted to start transferring
    input. When this value is ``True``, to see whether the transfer is
    active or queued, check ``TransferQueued``.
 ``TransferringOutput``:
    A boolean value that indicates whether the job is currently
    transferring output files. The value is ``Undefined`` if the job is
    not scheduled to run or has not yet attempted to start transferring
    output. When this value is ``True``, to see whether the transfer is
    active or queued, check ``TransferQueued``.
 ``TransferQueued``:
    A boolean value that indicates whether the job is currently waiting
    to transfer files because of limits placed by
    ``MAX_CONCURRENT_DOWNLOADS`` or ``MAX_CONCURRENT_UPLOADS`` .

 ``UserLog``:
    The full path and file name on the submit machine of the log file of
    job events.
 ``WantGracefulRemoval``:
    A boolean expression that, when ``True``, specifies that a graceful
    shutdown of the job should be done when the job is removed or put on
    hold.
 ``WindowsBuildNumber``:
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a build number for a
    Windows operating system. This attribute only exists for jobs
    submitted from Windows machines.
 ``WindowsMajorVersion``:
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a major version number
    (currently 5 or 6) for a Windows operating system. This attribute
    only exists for jobs submitted from Windows machines.
 ``WindowsMinorVersion``:
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a minor version number
    (currently 0, 1, or 2) for a Windows operating system. This
    attribute only exists for jobs submitted from Windows machines.
 ``X509UserProxy``:
    The full path and file name of the file containing the X.509 user
    proxy.
 ``X509UserProxyEmail``:
    For a job with an X.509 proxy credential, this is the email address
    extracted from the proxy.
 ``X509UserProxyExpiration``:
    For a job that defines the submit description file command
    **x509userproxy**, this is the time at which the indicated X.509
    proxy credential will expire, measured in the number of seconds
    since the epoch (00:00:00 UTC, Jan 1, 1970).
 ``X509UserProxyFirstFQAN``:
    For a vanilla or grid universe job that defines the submit
    description file command **x509userproxy**, this is the VOMS Fully
    Qualified Attribute Name (FQAN) of the primary role of the
    credential. A credential may have multiple roles defined, but by
    convention the one listed first is the primary role.
 ``X509UserProxyFQAN``:
    For a vanilla or grid universe job that defines the submit
    description file command **x509userproxy**, this is a serialized
    list of the DN and all FQAN. A comma is used as a separator, and any
    existing commas in the DN or FQAN are replaced with the string
    ``&comma;``. Likewise, any ampersands in the DN or FQAN are replaced
    with ``&amp;``.
 ``X509UserProxySubject``:
    For a vanilla or grid universe job that defines the submit
    description file command **x509userproxy**, this attribute contains
    the Distinguished Name (DN) of the credential used to submit the
    job.
 ``X509UserProxyVOName``:
    For a vanilla or grid universe job that defines the submit
    description file command **x509userproxy**, this is the name of the
    VOMS virtual organization (VO) that the user’s credential is part
    of.

The following job ClassAd attributes are relevant only for **vm**
universe jobs.

 ``VM_MACAddr``:
    The MAC address of the virtual machine’s network interface, in the
    standard format of six groups of two hexadecimal digits separated by
    colons. This attribute is currently limited to apply only to Xen
    virtual machines.

The following job ClassAd attributes appear in the job ClassAd only for
the *condor\_dagman* job submitted under DAGMan. They represent status
information for the DAG.

 ``DAG_InRecovery``:
    The value 1 if the DAG is in recovery mode, and The value 0
    otherwise.
 ``DAG_NodesDone``:
    The number of DAG nodes that have finished successfully. This means
    that the entire node has finished, not only an actual HTCondor job
    or jobs.
 ``DAG_NodesFailed``:
    The number of DAG nodes that have failed. This value includes all
    retries, if there are any.
 ``DAG_NodesPostrun``:
    The number of DAG nodes for which a POST script is running or has
    been deferred because of a POST script throttle setting.
 ``DAG_NodesPrerun``:
    The number of DAG nodes for which a PRE script is running or has
    been deferred because of a PRE script throttle setting.
 ``DAG_NodesQueued``:
    The number of DAG nodes for which the actual HTCondor job or jobs
    are queued. The queued jobs may be in any state.
 ``DAG_NodesReady``:
    The number of DAG nodes that are ready to run, but which have not
    yet started running.
 ``DAG_NodesTotal``:
    The total number of nodes in the DAG, including the FINAL node, if
    there is a FINAL node.
 ``DAG_NodesUnready``:
    The number of DAG nodes that are not ready to run. This is a node in
    which one or more of the parent nodes has not yet finished.
 ``DAG_Status``:
    The overall status of the DAG, with the same values as the macro
    ``$DAG_STATUS`` used in DAGMan FINAL nodes.

    --------------

    Value

    Status

    0

    OK

    1

    error; an error condition different than those listed here

    2

    one or more nodes in the DAG have failed

    3

    the DAG has been aborted by an ABORT-DAG-ON specification

    4

    removed; the DAG has been removed by *condor\_rm*

    5

    a cycle was found in the DAG

    6

    the DAG has been suspended (see
    section \ `2.10.8 <DAGManApplications.html#x22-890002.10.8>`__)

    --------------

    --------------

    --------------

The following job ClassAd attributes do not appear in the job ClassAd as
kept by the *condor\_schedd* daemon. They appear in the job ClassAd
written to the job’s execute directory while the job is running.

 ``CpusProvisioned``:
    The number of Cpus allocated to the job. With statically-allocated
    slots, it is the number of Cpus allocated to the slot. With
    dynamically-allocated slots, it is based upon the job attribute
    ``RequestCpus``, but may be larger due to the minimum given to a
    dynamic slot.
 ``DiskProvisioned``:
    The amount of disk space in KiB allocated to the job. With
    statically-allocated slots, it is the amount of disk space allocated
    to the slot. With dynamically-allocated slots, it is based upon the
    job attribute ``RequestDisk``, but may be larger due to the minimum
    given to a dynamic slot.
 ``MemoryProvisioned``:
    The amount of memory in MiB allocated to the job. With
    statically-allocated slots, it is the amount of memory space
    allocated to the slot. With dynamically-allocated slots, it is based
    upon the job attribute ``RequestMemory``, but may be larger due to
    the minimum given to a dynamic slot.
 ``<Name>Provisioned``:
    The amount of the custom resource identified by ``<Name>`` allocated
    to the job. For jobs using GPUs, ``<Name>`` will be ``GPUs``. With
    statically-allocated slots, it is the amount of the resource
    allocated to the slot. With dynamically-allocated slots, it is based
    upon the job attribute ``Request<Name>``, but may be larger due to
    the minimum given to a dynamic slot.

      
