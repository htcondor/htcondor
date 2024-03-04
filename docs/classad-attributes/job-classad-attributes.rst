Job ClassAd Attributes
======================

Both active HTCondor jobs (those in a `condor_schedd`) and historical jobs
(those in the history file), are described by classads.  Active jobs can be
queried and displayed with the `condor_q` command, and historical jobs
are queried with the `condor_history` command, as in the examples below.
Note that not all job attributes are described here, some are for internal
HTCondor use, and are subject to change.  Also, not all jobs contain
all attributes.

.. code-block:: console

    $ condor_history -l username
    $ condor_q -l


:classad-attribute-def:`Absent`
    Boolean set to true ``True`` if the ad is absent.

:classad-attribute-def:`AcctGroup`
    The accounting group name, as set in the submit description file via
    the
    :subcom:`accounting_group[and attribute AcctGroup]`
    command. This attribute is only present if an accounting group was
    requested by the submission. See the :doc:`/admin-manual/cm-configuration` section
    for more information about accounting groups.

:classad-attribute-def:`AcctGroupUser`
    The user name associated with the accounting group. This attribute
    is only present if an accounting group was requested by the
    submission.

:classad-attribute-def:`ActivationDuration`
    Formally, the length of time in seconds from when the shadow sends a
    claim activation to when the shadow receives a claim deactivation.

    Informally, this is how much time HTCondor's fair-share mechanism
    will charge the job for, plus one round-trip over the network.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute-def:`ActivationExecutionDuration`
    Formally, the length of time in seconds from when the shadow received
    notification that the job had been spawned to when the shadow received
    notification that the spawned process has exited.

    Informally, this is the duration limited by :ad-attr:`AllowedExecuteDuration`.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute-def:`ActivationSetupDuration`
    Formally, the length of time in seconds from when the shadow sends a
    claim activation to when the shadow it notified that the job was
    spawned.

    Informally, this is how long it took the starter to prepare to execute
    the job.  That includes file transfer, so the difference between this
    duration and the duration of input file transfer is (roughly) the
    execute-side overhead of preparing to start the job.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute-def:`ActivationTeardownDuration`
    Formally, the length of time in seconds from when the shadow received
    notification that the spawned process exited to when the shadow received
    a claim deactivation.


    Informally, this is how long it took the starter to finish up after the
    job.  That includes file transfer, so the difference between this duration
    and the duration of output file transfer is (roughly) the execute-side
    overhead of handling job termination.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute-def:`AllowedExecuteDuration`
    The longest time for which a job may be executing.  Jobs which exceed
    this duration will go on hold.  This time does not include file-transfer
    time.  Jobs which self-checkpoint have this long to write out each
    checkpoint.

    This attribute is intended to help minimize the time wasted by jobs
    which may erroneously run forever.

:classad-attribute-def:`AllowedJobDuration`
    The longest time for which a job may continuously be in the running state.
    Jobs which exceed this duration will go on hold.  Exiting the running
    state resets the job duration measured by this attribute.

    This attribute is intended to help minimize the time wasted by jobs
    which may erroneously run forever.

:classad-attribute-def:`AllRemoteHosts`
    String containing a comma-separated list of all the remote machines
    running a parallel or mpi universe job.

:classad-attribute-def:`Args`
    A string representing the command line arguments passed to the job,
    when those arguments are specified using the old syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute-def:`Arguments`
    A string representing the command line arguments passed to the job,
    when those arguments are specified using the new syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute-def:`AuthTokenSubject`
    A string recording the subject in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute-def:`AuthTokenIssuer`
    A string recording the issuer in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute-def:`AuthTokenGroups`
    A string recording the groups in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute-def:`AuthTokenScopes`
    A string recording the scopes in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute-def:`AuthTokenId`
    A string recording the unique identifier of the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute-def:`BatchExtraSubmitArgs`
    For :subcom:`batch[and attribute BatchExtraSubmitArgs]` grid universe jobs, additional command-line arguments
    to be given to the target batch system's job submission command.

:classad-attribute-def:`BatchProject`
    For **batch** grid universe jobs, the name of the
    project/account/allocation that should be charged for the job's
    resource usage.

:classad-attribute-def:`BatchQueue`
    For **batch** grid universe jobs, the name of the
    queue in the remote batch system.

:classad-attribute-def:`BatchRuntime`
    For **batch** grid universe jobs, a limit in seconds on the job's
    execution time, enforced by the remote batch system.

:classad-attribute-def:`BlockReadKbytes`
    The integer number of KiB read from disk for this job.

:classad-attribute-def:`BlockReads`
    The integer number of disk blocks read for this job.

:classad-attribute-def:`BlockWriteKbytes`
    The integer number of KiB written to disk for this job.

:classad-attribute-def:`BlockWrites`
    The integer number of blocks written to disk for this job.

:classad-attribute-def:`CheckpointDestination`
    A URL, as defined by submit command **checkpoint_destination**.

:classad-attribute-def:`CloudLabelNames`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    :subcom:`cloud_label_names[and attribute CloudLabelNames]`
    Defines the set of labels associated with the GCE instance.

:classad-attribute-def:`ClusterId`
    Integer cluster identifier for this job. A cluster is a group of
    jobs that were submitted together. Each job has its own unique job
    identifier within the cluster, but shares a common cluster
    identifier. The value changes each time a job or set of jobs are
    queued for execution under HTCondor.

:classad-attribute-def:`Cmd`
    The path to and the file name of the job to be executed.

:classad-attribute-def:`CommittedTime`
    The number of seconds of wall clock time that the job has been
    allocated a machine, excluding the time spent on run attempts that
    were evicted. Like :ad-attr:`RemoteWallClockTime`,
    this includes time the job spent in a suspended state, so the total
    committed wall time spent running is

    .. code-block:: condor-classad-expr

        CommittedTime - CommittedSuspensionTime

:index:`SYSTEM_JOB_MACHINE_ATTRS`

:classad-attribute-def:`CommittedSlotTime`
    This attribute is identical to :ad-attr:`CommittedTime` except that the
    time is multiplied by the :ad-attr:`SlotWeight` of the machine(s) that ran
    the job. This relies on :ad-attr:`SlotWeight` being listed in
    :macro:`SYSTEM_JOB_MACHINE_ATTRS`

:classad-attribute-def:`CommittedSuspensionTime`
    A running total of the number of seconds the job has spent in
    suspension during time in which the job was not evicted.
    This number is updated when the job exits.

:classad-attribute-def:`CompletionDate`
    The time when the job completed, or undefined if the job has not
    yet completed. Measured in the number of seconds since the epoch
    (00:00:00 UTC, Jan 1, 1970). Note that older versions of HTCondor
    initialized :ad-attr:`CompletionDate` to the integer 0, so job ads from
    older versions of HTCondor might have a 0 CompletionDate for
    jobs which haven't completed.

:classad-attribute-def:`ConcurrencyLimits`
    A string list, delimited by commas and space characters. The items
    in the list identify named resources that the job requires. The
    value can be a ClassAd expression which, when evaluated in the
    context of the job ClassAd and a matching machine ClassAd, results
    in a string list.

:classad-attribute-def:`CondorPlatform`
    A string that describes the operating system version that the 
    `condor_submit` command that submitted this job was built for.  Note
    this may be different that the operating system that is actually running.

:classad-attribute-def:`CondorVersion`
    A string that describes the HTCondor version of the `condor_submit`
    command that created this job.  Note this may be different than the
    version of the HTCondor daemon that runs the job.

:classad-attribute-def:`ContainerImageSource`
    For Container universe jobs, the string that names the container image source
    Is "local" for non-transfered images or "cedar" for transfered files.  "docker"
    or "http" might be other common values.

:classad-attribute-def:`ContainerTargetDir`
    For Container universe jobs, a filename that becomes the working directory of
    the job.  Mapped to the scratch directory.

:index:`SYSTEM_JOB_MACHINE_ATTRS`

:classad-attribute-def:`CumulativeSlotTime`
    This attribute is identical to :ad-attr:`RemoteWallClockTime` except that
    the time is multiplied by the :ad-attr:`SlotWeight` of the machine(s) that
    ran the job. This relies on :ad-attr:`SlotWeight` being listed in
    :macro:`SYSTEM_JOB_MACHINE_ATTRS`

:classad-attribute-def:`CumulativeSuspensionTime`
    A running total of the number of seconds the job has spent in
    suspension for the life of the job.

:classad-attribute-def:`CumulativeTransferTime`
    The total time, in seconds, that condor has spent transferring the
    input and output sandboxes for the life of the job.

:classad-attribute-def:`CurrentHosts`
    The number of hosts in the claimed state, due to this job.

:classad-attribute-def:`DAGManJobId`
    For a DAGMan node job only, the :ad-attr:`ClusterId` job ClassAd attribute
    of the :tool:`condor_dagman` job which is the parent of this node job.
    For nested DAGs, this attribute holds only the :ad-attr:`ClusterId` of the
    job's immediate parent.

:classad-attribute-def:`DAGParentNodeNames`
    For a DAGMan node job only, a comma separated list of each *JobName*
    which is a parent node of this job's node. This attribute is passed
    through to the job via the :tool:`condor_submit` command line, if it does
    not exceed the line length defined with ``_POSIX_ARG_MAX``. For
    example, if a node job has two parents with *JobName* s B and C,
    the :tool:`condor_submit` command line will contain

    .. code-block:: text

          -append +DAGParentNodeNames="B,C"

:classad-attribute-def:`DAGManNodesLog`
    For a DAGMan node job only, gives the path to an event log used
    exclusively by DAGMan to monitor the state of the DAG's jobs. Events
    are written to this log file in addition to any log file specified
    in the job's submit description file.

:classad-attribute-def:`DAGNodeName`
    Name of the DAG node that this job is associated with.

:classad-attribute-def:`DAGManNodesMask`
    For a DAGMan node job only, a comma-separated list of the event
    codes that should be written to the log specified by
    :ad-attr:`DAGManNodesLog`, known as the auxiliary log. All events not
    specified in the :ad-attr:`DAGManNodesMask` string are not written to the
    auxiliary event log. The value of this attribute is determined by
    DAGMan, and it is passed to the job via the :tool:`condor_submit` command
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
    -  ``Grid submit``, event code is 27

    If :ad-attr:`DAGManNodesLog` is not defined, it has no effect. The value of
    :ad-attr:`DAGManNodesMask` does not affect events recorded in the job event
    log file referred to by :ad-attr:`UserLog`.

:classad-attribute-def:`DAGManNodeRetry`
    For a DAGMan node job only, the current retry attempt number for the node
    that this job belongs. This attribute is only included if specified by
    :macro:`DAGMAN_NODE_RECORD_INFO` configuration option.

:classad-attribute-def:`DeferralPrepTime`
    An integer representing the number of seconds before the jobs :ad-attr:`DeferralTime`
    to which the job may be matched with a machine.

:classad-attribute-def:`DeferralTime`
    A Unix Epoch timestamp that represents the exact time HTCondor should
    attempt to begin executing the job.

:classad-attribute-def:`DeferralWindow`
    An integer representing the number of seconds after the jobs :ad-attr:`DeferralTime`
    to allow the job to arrive at the execute machine before automatically being
    evicted due to missing its :ad-attr:`DeferralTime`.

:index:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`

:classad-attribute-def:`DelegateJobGSICredentialsLifetime`
    An integer that specifies the maximum number of seconds for which
    delegated proxies should be valid. The default behavior is
    determined by the configuration setting
    :macro:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME` which defaults
    to one day. A value of 0 indicates that the delegated proxy should
    be valid for as long as allowed by the credential used to create the
    proxy. This setting currently only applies to proxies delegated for
    non-grid jobs and HTCondor-C jobs.
    This setting has no effect if the configuration setting
    :macro:`DELEGATE_JOB_GSI_CREDENTIALS` is false, because in
    that case the job proxy is copied rather than delegated.

:classad-attribute-def:`DiskUsage`
    Amount of disk space (KiB) in the HTCondor execute directory on the
    execute machine that this job has used. An initial value may be set
    at the job's request, placing into the job's submit description file
    a setting such as

    .. code-block:: condor-submit

          # 1 megabyte initial value
          +DiskUsage = 1024

    **vm** universe jobs will default to an initial value of the disk
    image size. If not initialized by the job, non-**vm** universe jobs
    will default to an initial value of the sum of the job's executable
    and all input files.

:classad-attribute-def:`DockerImage`
    For Docker and Container universe jobs, a string that names the docker image to run
    inside the container.

:classad-attribute-def:`EC2AccessKeyId`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_access_key_id[and attribute EC2AccessKeyId]`.
    Defines the path and file name of the file containing the EC2 Query
    API's access key. 
    
:classad-attribute-def:`EC2AmiID`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_ami_id[and attribute EC2AmiID]`.
    Identifies the machine image of the instance.

:classad-attribute-def:`EC2BlockDeviceMapping`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_block_device_mapping[and attribute EC2BlockDeviceMapping]`.
    Defines the map from block device names to kernel device names for
    the instance. 
    
:classad-attribute-def:`EC2ElasticIp`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_elastic_ip[and attribute EC2ElasticIp]`.
    Specifies an Elastic IP address to associate with the instance.

:classad-attribute-def:`EC2IamProfileArn`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_iam_profile_arn[and attribute EC2IamProfileArn]`.
    Specifies the IAM (instance) profile to associate with this
    instance. 

:classad-attribute-def:`EC2IamProfileName`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_iam_profile_name[and attribute EC2IamProfileName]`.
    Specifies the IAM (instance) profile to associate with this
    instance.

:classad-attribute-def:`EC2InstanceName`
    Used for grid type ec2 jobs; a string set for the job once the
    instance starts running, as assigned by the EC2 service, that
    represents the unique ID assigned to the instance by the EC2
    service.

:classad-attribute-def:`EC2InstanceType`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_instance_type[and attribute EC2InstanceType]`.
    Specifies a service-specific instance type.

:classad-attribute-def:`EC2KeyPair`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_keypair[and attribute EC2KeyPair]`.
    Defines the key pair associated with the EC2 instance.

:classad-attribute-def:`EC2ParameterNames`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_parameter_names[and attribute EC2ParameterNames]`.
    Contains a space or comma separated list of the names of additional
    parameters to pass when instantiating an instance.

:classad-attribute-def:`EC2SpotPrice`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_spot_price[and attribute EC2SpotPrice]`.
    Defines the maximum amount per hour a job submitter is willing to
    pay to run this job.

:classad-attribute-def:`EC2SpotRequestID`
    Used for grid type ec2 jobs; identifies the spot request HTCondor
    made on behalf of this job.

:classad-attribute-def:`EC2StatusReasonCode`
    Used for grid type ec2 jobs; reports the reason for the most recent
    EC2-level state transition. Can be used to determine if a spot
    request was terminated due to a rise in the spot price.

:classad-attribute-def:`EC2TagNames`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_tag_names[and attribute EC2TagNames]`.
    Defines the set, and case, of tags associated with the EC2 instance.

:classad-attribute-def:`EC2KeyPairFile`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_keypair_file[and attribute EC2KeyPairFile]`.
    Defines the path and file name of the file into which to write the
    SSH key used to access the image, once it is running.

:classad-attribute-def:`EC2RemoteVirtualMachineName`
    Used for grid type ec2 jobs; a string set for the job once the
    instance starts running, as assigned by the EC2 service, that
    represents the host name upon which the instance runs, such that the
    user can communicate with the running instance.

:classad-attribute-def:`EC2SecretAccessKey`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_secret_access_key[and attribute EC2SecretAccessKey]`.
    Defines that path and file name of the file containing the EC2 Query
    API's secret access key.

:classad-attribute-def:`EC2SecurityGroups`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_security_groups[and attribute EC2SecurityGroups]`.
    Defines the list of EC2 security groups which should be associated
    with the job.

:classad-attribute-def:`EC2SecurityIDs`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_security_ids[and attribute EC2SecurityIDs]`.
    Defines the list of EC2 security group IDs which should be
    associated with the job.

:classad-attribute-def:`EC2UserData`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_user_data[and attribute EC2UserData]`.
    Defines a block of data that can be accessed by the virtual machine.

:classad-attribute-def:`EC2UserDataFile`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    :subcom:`ec2_user_data_file[and attribute EC2UserDataFile]`.
    Specifies a path and file name of a file containing data that can be
    accessed by the virtual machine.

:classad-attribute-def:`EmailAttributes`
    A string containing a comma-separated list of job ClassAd
    attributes. For each attribute name in the list, its value will be
    included in the e-mail notification upon job completion.

:classad-attribute-def:`EncryptExecuteDirectory`
    A boolean value taken from the submit description file command
    :subcom:`encrypt_execute_directory[and attribute EncryptExecuteDirectory]`.
    It specifies if HTCondor should encrypt the remote scratch directory
    on the machine where the job executes.

:classad-attribute-def:`EnteredCurrentStatus`
    An integer containing the epoch time of when the job entered into
    its current status So for example, if the job is on hold, the
    ClassAd expression

    .. code-block:: condor-classad-expr

            time() - EnteredCurrentStatus

    will equal the number of seconds that the job has been on hold.

:classad-attribute-def:`Env`
    A string representing the environment variables passed to the job,
    when those arguments are specified using the old syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute-def:`Environment`
    A string representing the environment variables passed to the job,
    when those arguments are specified using the new syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute-def:`EraseOutputAndErrorOnRestart`
    A boolean.  If missing or true, HTCondor will erase (truncate) the error
    and output logs when the job restarts.  If this attribute is false, and
    ``when_to_transfer_output`` is ``ON_EXIT_OR_EVICT``, HTCondor will instead
    append to those files.

:classad-attribute-def:`ExecutableSize`
    Size of the executable in KiB.

:classad-attribute-def:`ExitBySignal`
    An attribute that is ``True`` when a user job exits via a signal and
    ``False`` otherwise. For some grid universe jobs, how the job exited
    is unavailable. In this case, :ad-attr:`ExitBySignal` is set to ``False``.

:classad-attribute-def:`ExitCode`
    When a user job exits by means other than a signal, this is the exit
    return code of the user job. For some grid universe jobs, how the
    job exited is unavailable. In this case, :ad-attr:`ExitCode` is set to 0.

:classad-attribute-def:`ExitSignal`
    When a user job exits by means of an unhandled signal, this
    attribute takes on the numeric value of the signal. For some grid
    universe jobs, how the job exited is unavailable. In this case,
    :ad-attr:`ExitSignal` will be undefined.

:classad-attribute-def:`ExitStatus`
    The way that HTCondor previously dealt with a job's exit status.
    This attribute should no longer be used. It is not always accurate
    in heterogeneous pools, or if the job exited with a signal. Instead,
    see the attributes: :ad-attr:`ExitBySignal`, :ad-attr:`ExitCode`, and
    :ad-attr:`ExitSignal`.
    
:classad-attribute-def:`GceAuthFile`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    :subcom:`gce_auth_file[and attribute GceAuthFile]`.
    Defines the path and file name of the file containing authorization
    credentials to use the GCE service.

:classad-attribute-def:`GceImage`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    :subcom:`gce_image[and attribute GceImage]`.
    Identifies the machine image of the instance.

:classad-attribute-def:`GceJsonFile`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    :subcom:`gce_json_file[and attribute GceJsonFile]`.
    Specifies the path and file name of a file containing a set of JSON
    object members that should be added to the instance description
    submitted to the GCE service.

:classad-attribute-def:`GceMachineType`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    :subcom:`gce_machine_type[and attribute GceMachineType]`.
    Specifies the hardware profile that should be used for a GCE
    instance.
    
:classad-attribute-def:`GceMetadata`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    :subcom:`gce_metadata[and attribute GceMetadata]`.
    Defines a set of name/value pairs that can be accessed by the
    virtual machine.

:classad-attribute-def:`GceMetadataFile`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    :subcom:`gce_metadata_file[and attribute GceMetadataFile]`.
    Specifies a path and file name of a file containing a set of
    name/value pairs that can be accessed by the virtual machine.

:classad-attribute-def:`GcePreemptible`
    Used for grid type gce jobs; a boolean taken from the definition of
    the submit description file command
    :subcom:`gce_preemptible[and attribute GcePreemptible]`.
    Specifies whether the virtual machine instance created in GCE should
    be preemptible.

:classad-attribute-def:`GlobalJobId`
    A string intended to be a unique job identifier within a pool. It
    currently contains the *condor_schedd* daemon ``Name`` attribute, a
    job identifier composed of attributes :ad-attr:`ClusterId` and :ad-attr:`ProcId`
    separated by a period, and the job's submission time in seconds
    since 1970-01-01 00:00:00 UTC, separated by # characters. The value
    submit.example.com#152.3#1358363336 is an example.  While HTCondor
    guarantees this string will be globally unique, the contents are subject
    to change, and users should not parse out components of this string.

:classad-attribute-def:`GPUsMaxCapability`
    A floating point value indicating the maximum ``Capability`` value of a GPU
    permitted by this job.  This attribute is referenced by the ``RequireGPUs``
    job attribute in order to constrain which slots containing GPUs a job is matched to.
    Set this attribute in a job by using the submit command :subcom:`gpus_maximum_capability`

:classad-attribute-def:`GPUsMinCapability`
    A floating point value indicating the minimum ``Capability`` value of a GPU
    needed by this job.  This attribute is referenced by the ``RequireGPUs``
    job attribute in order to constrain which slots containing GPUs a job is matched to.
    Set this attribute in a job by using the submit command :subcom:`gpus_minimum_capability`

:classad-attribute-def:`GPUsMinMemory`
    A integer value in megabytes indicating the minimum ``GlobalMemoryMB`` amount a GPU
    must have to run this job.  This attribute is referenced by the ``RequireGPUs``
    job attribute in order to constrain which slots containing GPUs a job is matched to.
    Set this attribute in a job by using the submit description file command :subcom:`gpus_minimum_memory`

:classad-attribute-def:`GPUsMinRuntime`
    A integer encoded version value which is compared to the ``MaxSupportedVersion`` value of a GPU
    to determine if the runtime needed by the job is supported.  The value should be encoded as
    MajorVersion*1000 + MinorVersion*10.  This attribute is referenced by the ``RequireGPUs``
    job attribute in order to constrain which slots containing GPUs a job is matched to.
    Set this attribute in a job by using the submit description file command :subcom:`gpus_minimum_runtime`

:classad-attribute-def:`GridJobStatus`
    A string containing the job's status as reported by the remote job
    management system.

:classad-attribute-def:`GridResource`
    A string defined by the right hand side of the submit
    description file command
    :subcom:`grid_resource[and attribute GridResource]`.
    It specifies the target grid type, plus additional parameters
    specific to the grid type.

:classad-attribute-def:`GridResourceUnavailableTime`
    Time at which the remote job management system became unavailable.
    Measured in the number of seconds since the epoch (00:00:00 UTC,
    Jan 1, 1970).

:classad-attribute-def:`HoldKillSig`
    Currently only for scheduler and local universe jobs, a string
    containing a name of a signal to be sent to the job if the job is
    put on hold.

:classad-attribute-def:`HoldReason`
    A string containing a human-readable message about why a job is on
    hold. This is the message that will be displayed in response to the
    command ``condor_q -hold``. It can be used to determine if a job should
    be released or not.

:classad-attribute-def:`HoldReasonCode`
    An integer value that represents the reason that a job was put on
    hold.  The below table defines all possible values used by 
    attributes :ad-attr:`HoldReasonCode`, :ad-attr:`NumHoldsByReason`, and :ad-attr:`HoldReasonSubCode`. 

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

    Note for hold codes 12 [TransferOutputError] and 13 [TransferInputError]:
    file transfer may invoke file-transfer plug-ins.  If it does, the hold
    subcodes may additionally be 62 (ETIME), if the file-transfer plug-in
    timed out; or the exit code of the plug-in shifted left by eight bits,
    otherwise.

:classad-attribute-def:`HoldReasonSubCode`
    An integer value that represents further information to go along
    with the :ad-attr:`HoldReasonCode`, for some values of :ad-attr:`HoldReasonCode`.
    See :ad-attr:`HoldReasonCode` for a table of possible values.

:classad-attribute-def:`HookKeyword`
    A string that uniquely identifies a set of job hooks, and added to
    the ClassAd once a job is fetched.

:classad-attribute-def:`ImageSize`
    Maximum observed memory image size (i.e. virtual memory) of the job
    in KiB. The initial value is equal to the size of the executable for
    non-vm universe jobs, and 0 for vm universe jobs.
    A vanilla universe job's :ad-attr:`ImageSize` is recomputed
    internally every 15 seconds. How quickly this updated information
    becomes visible to :tool:`condor_q` is controlled by
    :macro:`SHADOW_QUEUE_UPDATE_INTERVAL` and :macro:`STARTER_UPDATE_INTERVAL`.

    Under Linux, ``ProportionalSetSize`` is a better indicator of memory
    usage for jobs with significant sharing of memory between processes,
    because :ad-attr:`ImageSize` is simply the sum of virtual memory sizes
    across all of the processes in the job, which may count the same
    memory pages more than once.

:classad-attribute-def:`IOWait`
    I/O wait time of the job recorded by the cgroup controller in
    seconds.

:classad-attribute-def:`IwdFlushNFSCache`
    A boolean expression that controls whether or not HTCondor attempts
    to flush a access point's NFS cache, in order to refresh an
    HTCondor job's initial working directory. The value will be
    ``True``, unless a job explicitly adds this attribute, setting it to
    ``False``.

:classad-attribute-def:`JobAdInformationAttrs`
    A comma-separated list of attribute names. The named attributes and
    their values are written in the job event log whenever any event is
    being written to the log. This is the same as the configuration
    setting ``EVENT_LOG_INFORMATION_ATTRS`` (see
    :ref:`admin-manual/configuration-macros:daemon logging configuration file
    entries`) but it applies to the job event log instead of the system event log.

:classad-attribute-def:`JobBatchName`
    If a job is given a batch name with the -batch-name option to `condor_submit`, this 
    string valued attribute will contain the batch name.

:classad-attribute-def:`JobCurrentFinishTransferInputDate`
    Time at which the job most recently finished transferring its input
    sandbox. Measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970)

:classad-attribute-def:`JobCurrentFinishTransferOutputDate`
    Time at which the job most recently finished transferring its output
    sandbox. Measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970)

:classad-attribute-def:`JobCurrentReconnectAttempt`
    If a job is currently in disconnected state, and the AP is attempting
    to reconnect to an EP, this attribute is set to the retry number.
    Upon successful reconnection, or if the job has never been disconnected
    this attribute is undefined. Note the singular value of "attempt".

:classad-attribute-def:`JobCurrentStartDate`
    Time at which the job most recently began running. Measured in the
    number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`JobCurrentStartExecutingDate`
    Time at which the job most recently finished transferring its input
    sandbox and began executing. Measured in the number of seconds since
    the epoch (00:00:00 UTC, Jan 1, 1970)

:classad-attribute-def:`JobCurrentStartTransferInputDate`
    Time at which the job most recently began transferring its input
    sandbox. Measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970)

:classad-attribute-def:`JobCurrentStartTransferOutputDate`
    Time at which the job most recently finished executing and began
    transferring its output sandbox. Measured in the number of seconds
    since the epoch (00:00:00 UTC, Jan 1, 1970)

:classad-attribute-def:`JobDescription`
    A string that may be defined for a job by setting
    :subcom:`description[and attribute JobDescription]` in the
    submit description file. When set, tools which display the
    executable such as :tool:`condor_q` will instead use this string. For
    interactive jobs that do not have a submit description file, this
    string will default to ``"Interactive job"``.

:classad-attribute-def:`JobDisconnectedDate`
    Time at which the *condor_shadow* and *condor_starter* become disconnected.
    Set to ``Undefined`` when a successful reconnect occurs. Measured in the
    number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`JobLeaseDuration`
    The number of seconds set for a job lease, the amount of time that a
    job may continue running on a remote resource, despite its
    submitting machine's lack of response. See
    :ref:`users-manual/special-environment-considerations:job leases`
    for details on job leases.

:classad-attribute-def:`JobMaxVacateTime`
    An integer expression that specifies the time in seconds requested
    by the job for being allowed to gracefully shut down.

:classad-attribute-def:`JobNotification`
    An integer indicating what events should be emailed to the user. The
    integer values correspond to the user choices for the submit command
    :subcom:`notification[and attribute JobNotification]`.

    +-------+--------------------+
    | Value | Notification Value |
    +=======+====================+
    | 0     | Never              |
    +-------+--------------------+
    | 1     | Always             |
    +-------+--------------------+
    | 2     | Complete           |
    +-------+--------------------+
    | 3     | Error              |
    +-------+--------------------+


:classad-attribute-def:`JobPrio`
    Integer priority for this job, set by :tool:`condor_submit` or
    :tool:`condor_prio`. The default value is 0. The higher the number, the
    greater (better) the priority.

:classad-attribute-def:`JobRunCount`
    This attribute is retained for backwards compatibility. It may go
    away in the future. It is equivalent to :ad-attr:`NumShadowStarts` for all
    universes except **scheduler**. For the **scheduler** universe, this
    attribute is equivalent to :ad-attr:`NumJobStarts`.

:classad-attribute-def:`JobStartDate`
    Time at which the job first began running. Measured in the number of
    seconds since the epoch (00:00:00 UTC, Jan 1, 1970). Due to a long
    standing bug in the 8.6 series and earlier, the job classad that is
    internal to the *condor_startd* and *condor_starter* sets this to
    the time that the job most recently began executing. This bug is
    scheduled to be fixed in the 8.7 series.

:index:`state<single: state; job>`

:classad-attribute-def:`JobStatus`
    Integer which indicates the current status of the job.

    +-------+---------------------+
    | Value | Idle                |
    +=======+=====================+
    | 1     | Idle                |
    +-------+---------------------+
    | 2     | Running             |
    +-------+---------------------+
    | 3     | Removing            |
    +-------+---------------------+
    | 4     | Completed           |
    +-------+---------------------+
    | 5     | Held                |
    +-------+---------------------+
    | 6     | Transferring Output |
    +-------+---------------------+
    | 7     | Suspended           |
    +-------+---------------------+

:classad-attribute-def:`JobSubmitFile`
    String which names the submit file the job came from,
    if any.

:classad-attribute-def:`JobSubmitMethod`
    Integer which indicates how a job was submitted to HTCondor. Users can
    set a custom value for job via Python Bindings API.
 
    +-----------+------------------------+
    | Value     | Method of Submission   |
    +===========+========================+
    | Undefined | Unknown                |
    +-----------+------------------------+
    | 0         | :tool:`condor_submit`  |
    +-----------+------------------------+
    | 1         | DAGMan-Direct          |
    +-----------+------------------------+
    | 2         | Python Bindings        |
    +-----------+------------------------+
    | 3         |*htcondor job submit*   |
    +-----------+------------------------+
    | 4         |*htcondor dag submit*   |
    +-----------+------------------------+
    | 5         |*htcondor jobset submit*|
    +-----------+------------------------+
    | 100+      | Portal/User-set        |
    +-----------+------------------------+


:index:`universe<single: universe; job>`
:index:`standard<pair: standard; universe>`
:index:`pipe<pair: pipe; universe>`
:index:`linda<pair: linda; universe>`
:index:`pvm<pair: pvm; universe>`
:index:`vanilla<pair: vanilla; universe>`
:index:`pvmd<pair: pvmd; universe>`
:index:`scheduler<pair: scheduler; universe>`
:index:`mpi<pair: mpi; universe>`
:index:`grid<pair: grid; universe>`
:index:`parallel<pair: parallel; universe>`
:index:`java<pair: java; universe>`
:index:`local<pair: local; universe>`
:index:`vm<pair: vm; universe>`


:classad-attribute-def:`JobUniverse`
    Integer which indicates the job universe.

    +-------+-----------------+
    | Value | Universe        |
    +=======+=================+
    | 5     | vanilla, docker |
    +-------+-----------------+
    | 7     | scheduler       |
    +-------+-----------------+
    | 8     | MPI             |
    +-------+-----------------+
    | 9     | grid            |
    +-------+-----------------+
    | 10    | java            |
    +-------+-----------------+
    | 11    | parallel        |
    +-------+-----------------+
    | 12    | local           |
    +-------+-----------------+
    | 13    | vm              |
    +-------+-----------------+


:classad-attribute-def:`KeepClaimIdle`
    An integer value that represents the number of seconds that the
    *condor_schedd* will continue to keep a claim, in the Claimed Idle
    state, after the job with this attribute defined completes, and
    there are no other jobs ready to run from this user. This attribute
    may improve the performance of linear DAGs, in the case when a
    dependent job can not be scheduled until its parent has completed.
    Extending the claim on the machine may permit the dependent job to
    be scheduled with less delay than with waiting for the
    *condor_negotiator* to match with a new machine.

:classad-attribute-def:`KillSig`
    The Unix signal number that the job wishes to be sent before being
    forcibly killed. It is relevant only for jobs running on Unix
    machines. 
    
:classad-attribute-def:`KillSigTimeout`
    This attribute is replaced by the functionality in
    :ad-attr:`JobMaxVacateTime` as of HTCondor version 7.7.3. The number of
    seconds that the job requests the
    *condor_starter* wait after sending the signal defined as
    :ad-attr:`KillSig` and before forcibly removing the job. The actual amount
    of time will be the minimum of this value and the execute machine's
    configuration variable :macro:`KILLING_TIMEOUT`

:classad-attribute-def:`LastMatchTime`
    An integer containing the epoch time when the job was last
    successfully matched with a resource (gatekeeper) Ad.

:classad-attribute-def:`LastRejMatchReason`
    If, at any point in the past, this job failed to match with a
    resource ad, this attribute will contain a string with a
    human-readable message about why the match failed.

:classad-attribute-def:`LastRejMatchTime`
    An integer containing the epoch time when HTCondor-G last tried to
    find a match for the job, but failed to do so.

:classad-attribute-def:`LastRemotePool`
    The name of the *condor_collector* of the pool in which a job ran
    via flocking in the most recent run attempt. This attribute is not
    defined if the job did not run via flocking.

:classad-attribute-def:`LastSuspensionTime`
    Time at which the job last performed a successful suspension.
    Measured in the number of seconds since the epoch (00:00:00 UTC, Jan
    1, 1970).
    
:classad-attribute-def:`LastVacateTime`
    Time at which the job was last evicted from a remote workstation.
    Measured in the number of seconds since the epoch (00:00:00 UTC, Jan
    1, 1970).
    
:classad-attribute-def:`LeaveJobInQueue`
    A boolean expression that defaults to ``False``, causing the job to
    be removed from the queue upon completion. An exception is if the
    job is submitted using ``condor_submit -spool``. For this case, the
    default expression causes the job to be kept in the queue for 10
    days after completion.

:classad-attribute-def:`MachineAttr<X><N>`
    Machine attribute of name ``<X>`` that is placed into this job
    ClassAd, as specified by the configuration variable
    :macro:`SYSTEM_JOB_MACHINE_ATTRS`. With the potential for multiple run
    attempts, ``<N>`` represents an integer value providing historical
    values of this machine attribute for multiple runs. The most recent
    run will have a value of ``<N>`` equal to ``0``. The next most
    recent run will have a value of ``<N>`` equal to ``1``.

:classad-attribute-def:`MaxHosts`
    The maximum number of hosts that this job would like to claim. As
    long as :ad-attr:`CurrentHosts` is the same as :ad-attr:`MaxHosts`, no more hosts
    are negotiated for.

:classad-attribute-def:`MaxJobRetirementTime`
    Maximum time in seconds to let this job run uninterrupted before
    kicking it off when it is being preempted. This can only decrease
    the amount of time from what the corresponding startd expression
    allows. 

:index:`MAX_TRANSFER_INPUT_MB`

:classad-attribute-def:`MaxTransferInputMB`
    This integer expression specifies the maximum allowed total size in
    Mbytes of the input files that are transferred for a job. This
    expression does not apply to grid universe or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the system setting :macro:`MAX_TRANSFER_INPUT_MB`
    is used. If the observed size
    of all input files at submit time is larger than the limit, the job
    will be immediately placed on hold with a :ad-attr:`HoldReasonCode` value
    of 32. If the job passes this initial test, but the size of the
    input files increases or the limit decreases so that the limit is
    violated, the job will be placed on hold at the time when the file
    transfer is attempted.

:index:`MAX_TRANSFER_OUTPUT_MB`

:classad-attribute-def:`MaxTransferOutputMB`
    This integer expression specifies the maximum allowed total size in
    Mbytes of the output files that are transferred for a job. This
    expression does not apply to grid universe or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the system setting :macro:`MAX_TRANSFER_OUTPUT_MB`
    is used. If the total size of
    the job's output files to be transferred is larger than the limit,
    the job will be placed on hold with a :ad-attr:`HoldReasonCode` value of
    33. The output will be transferred up to the point when the limit is
    hit, so some files may be fully transferred, some partially, and
    some not at all.

:classad-attribute-def:`MemoryUsage`
    An integer expression in units of Mbytes that represents the peak
    memory usage for the job. Its purpose is to be compared with the
    value defined by a job with the
    :subcom:`request_memory[and attribute MemoryUsage]`
    submit command, for purposes of policy evaluation.

:classad-attribute-def:`MinHosts`
    The minimum number of hosts that must be in the claimed state for
    this job, before the job may enter the running state.

:index:`MAX_NEXT_JOB_START_DELAY`

:classad-attribute-def:`NextJobStartDelay`
    An integer number of seconds delay time after this job starts until
    the next job is started. The value is limited by the configuration
    variable :macro:`MAX_NEXT_JOB_START_DELAY`

:classad-attribute-def:`NiceUser`
    Boolean value which when ``True`` indicates that this job is a nice
    job, raising its user priority value, thus causing it to run on a
    machine only when no other HTCondor jobs want the machine.

:classad-attribute-def:`Nonessential` 
    A boolean value only relevant to grid universe jobs, which when
    ``True`` tells HTCondor to simply abort (remove) any problematic
    job, instead of putting the job on hold. It is the equivalent of
    doing :tool:`condor_rm` followed by :tool:`condor_rm` **-forcex** any time the
    job would have otherwise gone on hold. If not explicitly set to
    ``True``, the default value will be ``False``.

:classad-attribute-def:`NTDomain`
    A string that identifies the NT domain under which a job's owner
    authenticates on a platform running Windows.

:classad-attribute-def:`NumHolds`
    An integer value that will increment every time a job is placed on hold.
    It may be undefined until the job has been held at least once.

:classad-attribute-def:`NumHoldsByReason`
    The value of this attribute is a (nested) classad containing a count of how many times a job has been placed 
    on  hold grouped by the reason the job went on hold.  It may be undefined until the job has been held
    at least once. Each attribute name in this classad is
    a NumHoldByReason label; see the table above under 
    the documentation for job attribute :ad-attr:`HoldReasonCode` for a table of possible values. Each attribute
    value is an integer stating how many times the job went on hold for that specific reason.  An example:

    .. code-block:: condor-classad

        NumHoldsByReason = [ UserRequest = 2; JobPolicy = 110; UnableToOpenInput = 1 ]

:classad-attribute-def:`NumJobCompletions`
    An integer, initialized to zero, that is incremented by the
    *condor_shadow* each time the job's executable exits of its own
    accord, with or without errors, and successfully completes file
    transfer (if requested). Jobs which have done so normally enter the
    completed state; this attribute is therefore normally only of use
    when, for example, ``on_exit_remove`` or ``on_exit_hold`` is set.

:classad-attribute-def:`NumJobMatches`
    An integer that is incremented by the *condor_schedd* each time the
    job is matched with a resource ad by the negotiator.

:classad-attribute-def:`NumJobReconnects`
    An integer count of the number of times a job successfully
    reconnected after being disconnected. This occurs when the
    *condor_shadow* and *condor_starter* lose contact, for example
    because of transient network failures or a *condor_shadow* or
    *condor_schedd* restart. This attribute is only defined for jobs
    that can reconnected: those in the **vanilla** and **java**
    universes.

:classad-attribute-def:`NumJobStarts`
    An integer count of the number of times the job started executing.

:classad-attribute-def:`NumPids`
    A count of the number of child processes that this job has.

:classad-attribute-def:`NumRestarts`
    A count of the number of restarts from a checkpoint attempted by
    this job during its lifetime.  Currently updated only for VM
    universe jobs.

:classad-attribute-def:`NumShadowExceptions`
    An integer count of the number of times the *condor_shadow* daemon
    had a fatal error for a given job.

:classad-attribute-def:`NumShadowStarts`
    An integer count of the number of times a *condor_shadow* daemon
    was started for a given job. This attribute is not defined for
    **scheduler** universe jobs, since they do not have a
    *condor_shadow* daemon associated with them. For **local** universe
    jobs, this attribute is defined, even though the process that
    manages the job is technically a *condor_starter* rather than a
    *condor_shadow*. This keeps the management of the local universe
    and other universes as similar as possible. **Note that this
    attribute is incremented every time the job is matched, even if the
    match is rejected by the execute machine; in other words, the value
    of this attribute may be greater than the number of times the job
    actually ran.**

:classad-attribute-def:`NumSystemHolds`
    An integer that is incremented each time HTCondor-G places a job on
    hold due to some sort of error condition. This counter is useful,
    since HTCondor-G will always place a job on hold when it gives up on
    some error condition. Note that if the user places the job on hold
    using the :tool:`condor_hold` command, this attribute is not incremented.

:classad-attribute-def:`OtherJobRemoveRequirements`
    A string that defines a list of jobs. When the job with this
    attribute defined is removed, all other jobs defined by the list are
    also removed. The string is an expression that defines a constraint
    equivalent to the one implied by the command

    .. code-block:: console

          $ condor_rm -constraint <constraint>

    This attribute is used for jobs managed with :tool:`condor_dagman` to
    ensure that node jobs of the DAG are removed when the
    :tool:`condor_dagman` job itself is removed. Note that the list of jobs
    defined by this attribute must not form a cyclic removal of jobs, or
    the *condor_schedd* will go into an infinite loop when any of the
    jobs is removed.

:classad-attribute-def:`OutputDestination`
    A URL, as defined by submit command **output_destination**.

:classad-attribute-def:`Owner`
    String describing the user who submitted this job.

:classad-attribute-def:`ParallelShutdownPolicy`
    A string that is only relevant to parallel universe jobs. Without
    this attribute defined, the default policy applied to parallel
    universe jobs is to consider the whole job completed when the first
    node exits, killing processes running on all remaining nodes. If
    defined to the following strings, HTCondor's behavior changes:

     ``"WAIT_FOR_ALL"``
        HTCondor will wait until every node in the parallel job has
        completed to consider the job finished.

:index:`Starter pre and post scripts`

:classad-attribute-def:`PostArgs`
    Defines the command-line arguments for the post command using the
    old argument syntax, as specified in :doc:`/man-pages/condor_submit`.
    If both :ad-attr:`PostArgs` and :ad-attr:`PostArguments` exists, the former is ignored.

:classad-attribute-def:`PostArguments`
    Defines the command-line arguments for the post command using the
    new argument syntax, as specified in
    :doc:`/man-pages/condor_submit`, excepting that
    double quotes must be escaped with a backslash instead of another
    double quote. If both :ad-attr:`PostArgs` and :ad-attr:`PostArguments` exists, the
    former is ignored.
    
:classad-attribute-def:`PostCmd`
    A job in the vanilla, Docker, Java, or virtual machine universes may
    specify a command to run after the
    :subcom:`executable[and attribute PostCmd]` has
    exited, but before file transfer is started. Unlike a DAGMan POST
    script command, this command is run on the execute machine; however,
    it is not run in the same environment as the
    :subcom:`executable[and attribute PostCmd]`.
    Instead, its environment is set by :ad-attr:`PostEnv` or
    :ad-attr:`PostEnvironment`. Like the DAGMan POST script command, this
    command is not run in the same universe as the
    :subcom:`executable[and attribute PostCmd]`; in
    particular, this command is not run in a Docker container, nor in a
    virtual machine, nor in Java. This command is also not run with any
    of vanilla universe's features active, including (but not limited
    to): cgroups, PID namespaces, bind mounts, CPU affinity,
    Singularity, or job wrappers. This command is not automatically
    transferred with the job, so if you're using file transfer, you must
    add it to the
    :subcom:`transfer_input_files[and attribute PostCmd]`
    list.

    If the specified command is in the job's execute directory, or any
    sub-directory, you should not set
    :subcom:`vm_no_output_vm[and attribute PostCmd]`,
    as that will delete all the files in the job's execute directory
    before this command has a chance to run. If you don't want any
    output back from your VM universe job, but you do want to run a post
    command, do not set
    :subcom:`vm_no_output_vm[and attribute PostCmd]`
    and instead delete the job's execute directory in your post command.

:classad-attribute-def:`PostCmdExitBySignal`
    If :ad-attr:`SuccessPostExitCode` or :ad-attr:`SuccessPostExitSignal` were set,
    and the post command has run, this attribute will true if the
    post command exited on a signal and false if it did not. It is
    otherwise unset.

:classad-attribute-def:`PostCmdExitCode`
    If :ad-attr:`SuccessPostExitCode` or :ad-attr:`SuccessPostExitSignal` were set,
    the post command has run, and the post command did not exit on a
    signal, then this attribute will be set to the exit code. It is
    otherwise unset.

:classad-attribute-def:`PostCmdExitSignal`
    If :ad-attr:`SuccessPostExitCode` or :ad-attr:`SuccessPostExitSignal` were set,
    the post command has run, and the post command exited on a signal,
    then this attribute will be set to that signal. It is otherwise
    unset.

:classad-attribute-def:`PostEnv`
    Defines the environment for the Postscript using the Old environment
    syntax. If both :ad-attr:`PostEnv` and :ad-attr:`PostEnvironment` exist, the
    former is ignored.

:classad-attribute-def:`PostEnvironment`
    Defines the environment for the Postscript using the New environment
    syntax. If both :ad-attr:`PostEnv` and :ad-attr:`PostEnvironment` exist, the
    former is ignored.

:classad-attribute-def:`PreArgs`
    Defines the command-line arguments for the pre command using the old
    argument syntax, as specified in :doc:`/man-pages/condor_submit`. If both
    :ad-attr:`PreArgs` and :ad-attr:`PreArguments` exists, the former is ignored.

:classad-attribute-def:`PreArguments`
    Defines the command-line arguments for the pre command using the new
    argument syntax, as specified in
    :doc:`/man-pages/condor_submit`, excepting that
    double quotes must be escape with a backslash instead of another
    double quote. If both :ad-attr:`PreArgs` and :ad-attr:`PreArguments` exists, the
    former is ignored.

:classad-attribute-def:`PreCmd`
    A job in the vanilla, Docker, Java, or virtual machine universes may
    specify a command to run after file transfer (if any) completes but
    before the
    :subcom:`executable[and attribute PreCmd]` is
    started. Unlike a DAGMan PRE script command, this command is run on
    the execute machine; however, it is not run in the same environment
    as the :subcom:`executable[and attribute PreCmd]`.
    Instead, its environment is set by :ad-attr:`PreEnv` or :ad-attr:`PreEnvironment`.
    Like the DAGMan POST script command, this command is not run in the
    same universe as the
    :subcom:`executable[and attribute PreCmd]`; in
    particular, this command is not run in a Docker container, nor in a
    virtual machine, nor in Java. This command is also not run with any
    of vanilla universe's features active, including (but not limited
    to): cgroups, PID namespaces, bind mounts, CPU affinity,
    Singularity, or job wrappers. This command is not automatically
    transferred with the job, so if you're using file transfer, you must
    add it to the
    :subcom:`transfer_input_files[and attribute PreCmd]`
    list. 
    
:classad-attribute-def:`PreCmdExitBySignal`
    If :ad-attr:`SuccessPreExitCode` or :ad-attr:`SuccessPreExitSignal` were set, and
    the pre command has run, this attribute will true if the pre
    command exited on a signal and false if it did not. It is otherwise
    unset.
    
:classad-attribute-def:`PreCmdExitCode`
    If :ad-attr:`SuccessPreExitCode` or :ad-attr:`SuccessPreExitSignal` were set, the
    pre command has run, and the pre command did not exit on a signal,
    then this attribute will be set to the exit code. It is otherwise
    unset.
    
:classad-attribute-def:`PreCmdExitSignal`
    If :ad-attr:`SuccessPreExitCode` or :ad-attr:`SuccessPreExitSignal` were set, the
    pre command has run, and the pre command exited on a signal, then
    this attribute will be set to that signal. It is otherwise unset.

:classad-attribute-def:`PreEnv`
    Defines the environment for the prescript using the Old environment
    syntax. If both :ad-attr:`PreEnv` and :ad-attr:`PreEnvironment` exist, the former
    is ignored.
    
:classad-attribute-def:`PreEnvironment`
    Defines the environment for the prescript using the New environment
    syntax. If both :ad-attr:`PreEnv` and :ad-attr:`PreEnvironment` exist, the former
    is ignored.

:classad-attribute-def:`PreJobPrio1`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered first: before :ad-attr:`PreJobPrio2`,
    before :ad-attr:`JobPrio`, before :ad-attr:`PostJobPrio1`, before
    :ad-attr:`PostJobPrio2`, and before :ad-attr:`QDate`.

:classad-attribute-def:`PreJobPrio2`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after :ad-attr:`PreJobPrio1`, but before
    :ad-attr:`JobPrio`, before :ad-attr:`PostJobPrio1`, before :ad-attr:`PostJobPrio2`, and
    before :ad-attr:`QDate`.

:classad-attribute-def:`PostJobPrio1`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after :ad-attr:`PreJobPrio1`, after
    :ad-attr:`PreJobPrio1`, and after :ad-attr:`JobPrio`, but before :ad-attr:`PostJobPrio2`,
    and before :ad-attr:`QDate`.

:classad-attribute-def:`PostJobPrio2`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after :ad-attr:`PreJobPrio1`, after
    :ad-attr:`PreJobPrio1`, after :ad-attr:`JobPrio`, and after :ad-attr:`PostJobPrio1`, but
    before :ad-attr:`QDate`.

:classad-attribute-def:`PreserveRelativeExecutable`
    When ``True``, the *condor_starter* will not prepend ``Iwd`` to
    :ad-attr:`Cmd`, when :ad-attr:`Cmd` is a relative path name and
    :ad-attr:`TransferExecutable` is ``False``. The default value is ``False``.
    This attribute is primarily of interest for users of
    :macro:`USER_JOB_WRAPPER` for the purpose of allowing an executable's
    location to be resolved by the user's path in the job wrapper.

:classad-attribute-def:`PreserveRelativePaths`
    When ``True``, entries in the file transfer lists that are relative
    paths will be transferred to the same relative path on the destination
    machine (instead of the basename).

:classad-attribute-def:`ProcId`
    Integer process identifier for this job. Within a cluster of many
    jobs, each job has the same :ad-attr:`ClusterId`, but will have a unique
    :ad-attr:`ProcId`. Within a cluster, assignment of a :ad-attr:`ProcId` value will
    start with the value 0. The job (process) identifier described here
    is unrelated to operating system PIDs.

:classad-attribute-def:`ProportionalSetSizeKb`
    On Linux execute machines with kernel version more recent than
    2.6.27, this is the maximum observed proportional set size (PSS) in
    KiB, summed across all processes in the job. If the execute machine
    does not support monitoring of PSS or PSS has not yet been measured,
    this attribute will be undefined. PSS differs from :ad-attr:`ImageSize` in
    how memory shared between processes is accounted. The PSS for one
    process is the sum of that process' memory pages divided by the
    number of processes sharing each of the pages. :ad-attr:`ImageSize` is the
    same, except there is no division by the number of processes sharing
    the pages.

:classad-attribute-def:`ProvisionerState`
    The current state of a DAGs :ref:`DAG Provisioner Node` as set by the job
    itself via chirp. This is an enumerated value to inform DAGMan of the
    provisioner node jobs state to act accordingly (i.e. begin workflow).
    Current enumeration values are as follows:

    +--------------------------+------+
    | Provisioning Started     |   1  |
    +--------------------------+------+
    | Provisioning Completed   |   2  |
    +--------------------------+------+
    | De-Provisioning Started  |   3  |
    +--------------------------+------+
    | De-Provisioning Completed|   4  |
    +--------------------------+------+

    .. note::

        HTCondor does not set this value. The job is responsible for setting
        this so DAGMan works correctly.

:classad-attribute-def:`QDate`
    Time at which the job was submitted to the job queue. Measured in
    the number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`RecentBlockReadKbytes`.
    The integer number of KiB read from disk for this job over the
    previous time interval defined by configuration variable
    :macro:`STATISTICS_WINDOW_SECONDS`.

:classad-attribute-def:`RecentBlockReads`.
    The integer number of disk blocks read for this job over the
    previous time interval defined by configuration variable
    :macro:`STATISTICS_WINDOW_SECONDS`.

:classad-attribute-def:`RecentBlockWriteKbytes`.
    The integer number of KiB written to disk for this job over the
    previous time interval defined by configuration variable
    :macro:`STATISTICS_WINDOW_SECONDS`.

:classad-attribute-def:`RecentBlockWrites`.
    The integer number of blocks written to disk for this job over the
    previous time interval defined by configuration variable
    :macro:`STATISTICS_WINDOW_SECONDS`.

:classad-attribute-def:`ReleaseReason`
    A string containing a human-readable message about why the job was
    released from hold.

:classad-attribute-def:`RemoteIwd`
    The path to the directory in which a job is to be executed on a
    remote machine.

:classad-attribute-def:`RemotePool`
    The name of the *condor_collector* of the pool in which a job is
    running via flocking. This attribute is not defined if the job is
    not running via flocking.

:classad-attribute-def:`RemoteSysCpu`
    The total number of seconds of system CPU time (the time spent at
    system calls) the job used on remote machines. This does not count
    time spent on run attempts that were evicted.

:classad-attribute-def:`CumulativeRemoteSysCpu`
    The total number of seconds of system CPU time the job used on
    remote machines, summed over all execution attempts.

:classad-attribute-def:`RemoteUserCpu`
    The total number of seconds of user CPU time the job used on remote
    machines. This does not count time spent on run attempts that were
    evicted. A job in the virtual machine universe
    will only report this attribute if run on a KVM hypervisor.

:classad-attribute-def:`CumulativeRemoteUserCpu`
    The total number of seconds of user CPU time the job used on remote
    machines, summed over all execution attempts.

:classad-attribute-def:`RemoteWallClockTime`
    Cumulative number of seconds the job has been allocated a machine.
    This also includes time spent in suspension (if any), so the total
    real time spent running is

    .. code-block:: condor-classad-expr

        RemoteWallClockTime - CumulativeSuspensionTime

    Note that this number does not get reset to zero when a job is
    forced to migrate from one machine to another. :ad-attr:`CommittedTime`, on
    the other hand, is just like :ad-attr:`RemoteWallClockTime` except it does
    get reset to 0 whenever the job is evicted.

:classad-attribute-def:`LastRemoteWallClockTime`
    Number of seconds the job was allocated a machine for its most recent completed
    execution.  This attribute is set after the job exits or is evicted.
    It will be undefined until the first execution attempt completes or is terminated.
    When a job has been allocated a machine and is still running, the value will be
    undefined or will be the value from the previous execution attempt rather than the
    current one.

:classad-attribute-def:`RemoveKillSig`
    Currently only for scheduler universe jobs, a string containing a
    name of a signal to be sent to the job if the job is removed.

:classad-attribute-def:`RequestCpus`
    The number of CPUs requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum number
    of CPUs that are needed in the created dynamic slot.

:classad-attribute-def:`RequestDisk`
    The amount of disk space in KiB requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum amount
    of disk space needed in the created dynamic slot.

:classad-attribute-def:`RequestGPUs`
    The number of GPUs requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum number
    of GPUs that are needed in the created dynamic slot.

:classad-attribute-def:`RequireGPUs`
    Constraint on the properties of GPUs requested for this job. If dynamic
    *condor_startd* provisioning is enabled, This constraint will be tested
    against the property attributes of the `AvailableGPUs` attribute of the
    partitionable slot when choosing which GPUs for the dynamic slot.

:classad-attribute-def:`RequestedChroot`
    A full path to the directory that the job requests the
    *condor_starter* use as an argument to chroot().

:index:`JOB_DEFAULT_REQUESTMEMORY`

:classad-attribute-def:`RequestMemory`
    The amount of memory space in MiB requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum amount
    of memory needed in the created dynamic slot. If not set by the job,
    its definition is specified by configuration variable
    :macro:`JOB_DEFAULT_REQUESTMEMORY`

:index:`APPEND_REQUIREMENTES`

``Requirements``
    A classad expression evaluated by the *condor_negotiator*,
    *condor_schedd*, and *condor_startd* in the context of slot ad.  If
    true, this job is eligible to run on that slot.  If the job
    requirements does not mention the (startd) attribute :ad-attr:`OpSys`,
    the schedd will append a clause to Requirements forcing the job to
    match the same :ad-attr:`OpSys` as the access point. :index:`OPSYS`
    The schedd appends a similar clause to match the :ad-attr:`Arch`. :index:`ARCH`
    The schedd parameter :macro:`APPEND_REQUIREMENTS`, will, if set, append that
    value to every job's requirements expression.
    
:classad-attribute-def:`ResidentSetSize`
    Maximum observed physical memory in use by the job in KiB while
    running. 

:classad-attribute-def:`ScitokensFile`
    The path and filename containing a SciToken to use for a Condor-C job.

:classad-attribute-def:`ScratchDirFileCount`
    Number of files and directories in the jobs' Scratch directory.  The value is updated
    periodically while the job is running.

:classad-attribute-def:`ServerTime`
    This is the current time, in Unix epoch seconds.
    It is added by the *condor_schedd* to the job ads that it sends in
    reply to a query (e.g. sent to :tool:`condor_q`).
    Since it not present in the job ad in the *condor_schedd*, it
    should not be used in any expressions that will be evaluated by the
    *condor_schedd*.

:classad-attribute-def:`StackSize`
    Utilized for Linux jobs only, the number of bytes allocated for
    stack space for this job. This number of bytes replaces the default
    allocation of 512 Mbytes.

:classad-attribute-def:`StageOutFinish`
    An attribute representing a Unix epoch time that is defined for a
    job that is spooled to a remote site using ``condor_submit -spool``
    or HTCondor-C and causes HTCondor to hold the output in the spool
    while the job waits in the queue in the ``Completed`` state. This
    attribute is defined when retrieval of the output finishes.

:classad-attribute-def:`StageOutStart`
    An attribute representing a Unix epoch time that is defined for a
    job that is spooled to a remote site using ``condor_submit -spool``
    or HTCondor-C and causes HTCondor to hold the output in the spool
    while the job waits in the queue in the ``Completed`` state. This
    attribute is defined when retrieval of the output begins.

:classad-attribute-def:`JobStarterDebug`
    This attribute causes the *condor_starter* to write a job-specific
    copy of its daemon log in the job's scratch directory.
    If the value is `True`, then the the logging level matches that of
    the regular daemon log.
    If the value is a string, then it specifies a different logging
    level following the syntax of :macro:`<SUBSYS>_DEBUG`.

:classad-attribute-def:`JobStarterLog`
    When the *condor_starter* is creating a job-specific copy of its
    dameon log (see :ad-attr:`JobStarterDebug`), this attribute causes
    the log to be transferred to the Access Point with the job's
    output sandbox, and written to the given pathname.

:classad-attribute-def:`StreamErr`
    The default value is ``False``.
    If ``True``, and :ad-attr:`TransferErr` is ``True``, then
    standard error is streamed back to the access point, instead of
    doing the transfer (as a whole) after the job completes. If
    ``False``, then standard error is transferred back to the submit
    machine (as a whole) after the job completes. If :ad-attr:`TransferErr` is
    ``False``, then this job attribute is ignored.

:classad-attribute-def:`StreamOut`
    The default value is ``False``.
    If ``True``, and :ad-attr:`TransferOut` is ``True``, then job
    output is streamed back to the access point, instead of doing the
    transfer (as a whole) after the job completes. If ``False``, then
    job output is transferred back to the access point (as a whole)
    after the job completes. If :ad-attr:`TransferOut` is ``False``, then this
    job attribute is ignored.

:index:`GROUP_AUTOREGROUP` 

:classad-attribute-def:`SubmitterAutoregroup`
    A boolean attribute defined by the *condor_negotiator* when it
    makes a match. It will be ``True`` if the resource was claimed via
    negotiation when the configuration variable :macro:`GROUP_AUTOREGROUP`
    was ``True``. It will be ``False`` otherwise.

:classad-attribute-def:`SubmitterGlobalJobId`
    When HTCondor-C submits a job to a remote *condor_schedd*, it sets
    this attribute in the remote job ad to match the :ad-attr:`GlobalJobId`
    attribute of the original, local job.

:classad-attribute-def:`SubmitterGroup`
    The accounting group name defined by the *condor_negotiator* when
    it makes a match.

:classad-attribute-def:`SubmitterNegotiatingGroup`
    The accounting group name under which the resource negotiated when
    it was claimed, as set by the *condor_negotiator*.

:classad-attribute-def:`SuccessCheckpointExitBySignal`
    Specifies if the ``executable`` exits with a signal after a successful
    self-checkpoint.

:classad-attribute-def:`SuccessCheckpointExitCode`
    Specifies the exit code, if any, with which the ``executable`` exits
    after a successful self-checkpoint.

:classad-attribute-def:`SuccessCheckpointExitSignal`
    Specifies the signal, if any, by which the ``executable`` exits after
    a successful self-checkpoint.

:classad-attribute-def:`SuccessPreExitBySignal`
    Specifies if a successful pre command must exit with a signal.

:classad-attribute-def:`SuccessPreExitCode`
    Specifies the code with which the pre command must exit to be
    considered successful. Pre commands which are not successful cause
    the job to go on hold with :ad-attr:`ExitCode` set to :ad-attr:`PreCmdExitCode`.
    The exit status of a pre command without one of
    :ad-attr:`SuccessPreExitCode` or :ad-attr:`SuccessPreExitSignal` defined is
    ignored.

:classad-attribute-def:`SuccessPreExitSignal`
    Specifies the signal on which the pre command must exit be
    considered successful. Pre commands which are not successful cause
    the job to go on hold with :ad-attr:`ExitSignal` set to
    :ad-attr:`PreCmdExitSignal`. The exit status of a pre command without one
    of :ad-attr:`SuccessPreExitCode` or :ad-attr:`SuccessPreExitSignal` defined is
    ignored.

:classad-attribute-def:`SuccessPostExitBySignal`
    Specifies if a successful post command must exit with a signal.

:classad-attribute-def:`SuccessPostExitCode`
    Specifies the code with which the post command must exit to be
    considered successful. Post commands which are not successful cause
    the job to go on hold with :ad-attr:`ExitCode` set to :ad-attr:`PostCmdExitCode`.
    The exit status of a post command without one of
    :ad-attr:`SuccessPostExitCode` or :ad-attr:`SuccessPostExitSignal` defined is
    ignored.

:classad-attribute-def:`SuccessPostExitSignal`
    Specifies the signal on which the post command must exit be
    considered successful. Post commands which are not successful cause
    the job to go on hold with :ad-attr:`ExitSignal` set to
    :ad-attr:`PostCmdExitSignal`. The exit status of a post command without one
    of :ad-attr:`SuccessPostExitCode` or :ad-attr:`SuccessPostExitSignal` defined is
    ignored.

:classad-attribute-def:`ToE`
    ToE stands for Ticket of Execution, and is itself a nested classad that
    describes how a job was terminated by the execute machine.
    See the :doc:`/users-manual/managing-a-job` section for full details.

:classad-attribute-def:`TotalJobReconnectAttempts`
    The total number of reconnection attempts over the lifetime of the job.
    If there have never been any, this attribute is undefined. Note the
    plural nature of "Attempts".

:classad-attribute-def:`TotalSuspensions`
    A count of the number of times this job has been suspended during
    its lifetime.

:classad-attribute-def:`TransferCheckpoint`
    A string attribute containing a comma separated list of directories
    and/or files that should be transferred from the execute machine to the
    access point's spool when the job successfully checkpoints.

:classad-attribute-def:`TransferContainer`
    A boolean expression that controls whether the HTCondor should transfer the
    container image from the submit node to the worker node.

:classad-attribute-def:`TransferErr`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the error output from the job is
    transferred from the remote machine back to the access point. The
    name of the file after transfer is the file referred to by job
    attribute ``Err``. If ``False``, no transfer takes place (remote to
    access point), and the name of the file is the file referred to by
    job attribute ``Err``.

:classad-attribute-def:`TransferExecutable`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the job executable is transferred
    from the access point to the remote machine. The name of the file
    (on the access point) that is transferred is given by the job
    attribute :ad-attr:`Cmd`. If ``False``, no transfer takes place, and the
    name of the file used (on the remote machine) will be as given in
    the job attribute :ad-attr:`Cmd`.

:classad-attribute-def:`TransferIn`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the job input is transferred from the
    access point to the remote machine. The name of the file that is
    transferred is given by the job attribute ``In``. If ``False``, then
    the job's input is taken from a file on the remote machine
    (pre-staged), and the name of the file is given by the job attribute
    ``In``. 

:classad-attribute-def:`TransferInput`
    A string attribute containing a comma separated list of directories, files and/or URLs
    that should be transferred from the access point to the remote machine when
    input file transfer is enabled.

:classad-attribute-def:`TransferInFinished`
    When the job finished the most recent transfer of its input
    sandbox, measured in seconds from the epoch. (00:00:00 UTC Jan 1,
    1970). 

:classad-attribute-def:`TransferInQueued`
    If the job's most recent transfer of its input sandbox was queued,
    this attribute says when, measured in seconds from the epoch
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute-def:`TransferInStarted`
    : When the job actually started to transfer files, the most recent
    time it transferred its input sandbox, measured in seconds from the
    epoch. This will be later than :ad-attr:`TransferInQueued` (if set).
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute-def:`TransferInputSizeMB`
    The total size in Mbytes of input files to be transferred for the
    job. Files transferred via file transfer plug-ins are not included.
    This attribute is automatically set by :tool:`condor_submit`; jobs
    submitted via other submission methods, such as SOAP, may not define
    this attribute. 

:classad-attribute-def:`TransferInputStats`
    The value of this classad attribute is a nested classad, whose values
    contain several attributes about HTCondor-managed file transfer.
    These refer to the transfer of the sandbox from the AP submit point
    to the worker node, or the EP.

    Each attribute name has a prefix, either "Cedar", for the HTCondor
    built-in file transfer method, or the prefix of the file transfer
    plugin method (such as HTTP).  For each of these types of file transfer
    there is an attribute with that prefix whose body is "FilesCount", 
    the number of files transfered by that method during the last
    transfer, and "FilesCountTotal", the sum of FilesCount over all
    execution attempts.  In addition, for container universe jobs, there
    is a sub-attribute ```ContainerDuration```, the number of seconds
    it took to transfer the container image (if transfered), and
    ```ContainerDurationTotal```, the sum over all execution attempts.

:classad-attribute-def:`TransferOut`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the output from the job is
    transferred from the remote machine back to the access point. The
    name of the file after transfer is the file referred to by job
    attribute ``Out``. If ``False``, no transfer takes place (remote to
    access point), and the name of the file is the file referred to by
    job attribute ``Out``.

:classad-attribute-def:`TransferOutput`
    A string attribute containing a comma separated list of files and/or URLs that should be transferred
    from the remote machine to the access point when output file transfer is enabled.

:classad-attribute-def:`TransferOutFinished`
    When the job finished the most recent transfer of its
    output sandbox, measured in seconds from the epoch. (00:00:00 UTC
    Jan 1, 1970).

:classad-attribute-def:`TransferOutQueued`
    If the job's most recent transfer of its output sandbox was
    queued, this attribute says when, measured in seconds from the epoch
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute-def:`TransferOutputStats`
    The value of this classad attribute is a nested classad, whose values
    mirror those for `:ad-attr:`TransferInputStats``, but for the transfer
    from the EP worker node back to the AP submit point.

:classad-attribute-def:`TransferOutStarted`
    When the job actually started to transfer files, the most recent
    time it transferred its output sandbox, measured in seconds from the
    epoch. This will be later than :ad-attr:`TransferOutQueued` (if set).
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute-def:`TransferringInput`
    A boolean value that indicates whether the job is currently
    transferring input files. The value is ``Undefined`` if the job is
    not scheduled to run or has not yet attempted to start transferring
    input. When this value is ``True``, to see whether the transfer is
    active or queued, check :ad-attr:`TransferQueued`.

:classad-attribute-def:`TransferringOutput`
    A boolean value that indicates whether the job is currently
    transferring output files. The value is ``Undefined`` if the job is
    not scheduled to run or has not yet attempted to start transferring
    output. When this value is ``True``, to see whether the transfer is
    active or queued, check :ad-attr:`TransferQueued`.

:classad-attribute-def:`TransferPlugins`
    A string value containing a semicolon separated list of file transfer plugins
    to be supplied by the job. Each entry in this list will be of the form
    ``TAG1[,TAG2[,...]]=/path/to/plugin`` were `TAG` values are URL prefixes like `HTTP`,
    and ``/path/to/plugin`` is the path that the transfer plugin is to be transferred from.
    The files mentioned in this list will be transferred to the job sandbox before any file
    transfer plugins are invoked. A transfer plugin supplied in this will way will be used
    even if the execute node has a file transfer plugin installed that handles that URL prefix.

:classad-attribute-def:`WantTransferPluginMethods`
    A string value containing a comma separated list of file transfer plugin URL prefixes
    that are needed by the job but not supplied via the :ad-attr:`TransferPlugins` attribute.
    This attribute is intended to provide a convenient way to match against jobs that need
    a certain transfer plugin.

:classad-attribute-def:`TransferQueued`
    A boolean value that indicates whether the job is currently waiting
    to transfer files because of limits placed by
    :macro:`MAX_CONCURRENT_DOWNLOADS` or :macro:`MAX_CONCURRENT_UPLOADS`.

:classad-attribute-def:`UserLog`
    The full path and file name on the access point of the log file of
    job events.

:classad-attribute-def:`WantContainer`
    A boolean that when true, tells HTCondor to run this job in container
    universe.  Note that container universe jobs are a "topping" above vanilla
    universe, and the JobUniverse attribute of container jobs will be 5 (vanilla)

:classad-attribute-def:`WantDocker`
    A boolean that when true, tells HTCondor to run this job in docker
    universe.  Note that docker universe jobs are a "topping" above vanilla
    universe, and the JobUniverse attribute of docker jobs will be 5 (vanilla)

:classad-attribute-def:`WantFTOnCheckpoint`
    A boolean that, when ``True``, specifies that when the ``executable``
    exits as described by :ad-attr:`SuccessCheckpointExitCode`,
    :ad-attr:`SuccessCheckpointExitBySignal`, and :ad-attr:`SuccessCheckpointExitSignal`,
    HTCondor should do (output) file transfer and immediately continue the
    job in the same sandbox by restarting ``executable`` with the same
    arguments as the first time.

:classad-attribute-def:`WantGracefulRemoval`
    A boolean expression that, when ``True``, specifies that a graceful
    shutdown of the job should be done when the job is removed or put on
    hold.

:classad-attribute-def:`WindowsMajorVersion`
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a major version number
    (currently 5 or 6) for a Windows operating system. This attribute
    only exists for jobs submitted from Windows machines.

:classad-attribute-def:`WindowsBuildNumber`
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a build number for a
    Windows operating system. This attribute only exists for jobs
    submitted from Windows machines.

:classad-attribute-def:`WindowsMinorVersion`
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a minor version number
    (currently 0, 1, or 2) for a Windows operating system. This
    attribute only exists for jobs submitted from Windows machines.

:classad-attribute-def:`X509UserProxy`
    The full path and file name of the file containing the X.509 user
    proxy.

:classad-attribute-def:`X509UserProxyEmail`
    For a job with an X.509 proxy credential, this is the email address
    extracted from the proxy.

:classad-attribute-def:`X509UserProxyExpiration`
    For a job that defines the submit description file command
    :subcom:`x509userproxy[and attribute X509UserProxyExpiration]`,
    this is the time at which the indicated X.509 proxy credential will
    expire, measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970).

:classad-attribute-def:`X509UserProxyFirstFQAN`
    For a vanilla or grid universe job that defines the submit
    description file command
    :subcom:`x509userproxy[and attribute X509UserProxyFirstFQAN]`,
    this is the VOMS Fully Qualified Attribute Name (FQAN) of the
    primary role of the credential. A credential may have multiple roles
    defined, but by convention the one listed first is the primary role.

:classad-attribute-def:`X509UserProxyFQAN`
    For a vanilla or grid universe job that defines the submit
    description file command
    :subcom:`x509userproxy[and attribute X509UserProxyFQAN]`,
    this is a serialized list of the DN and all FQAN. A comma is used as
    a separator, and any existing commas in the DN or FQAN are replaced
    with the string ``&comma;``. Likewise, any ampersands in the DN or
    FQAN are replaced with ``&amp;``.

:classad-attribute-def:`X509UserProxySubject`
    For a vanilla or grid universe job that defines the submit
    description file command
    :subcom:`x509userproxy[and attribute X509UserProxySubject]`,
    this attribute contains the Distinguished Name (DN) of the
    credential used to submit the job.

:classad-attribute-def:`X509UserProxyVOName`
    For a vanilla or grid universe job that defines the submit
    description file command
    :subcom:`x509userproxy[and attribute X509UserProxyVOName]`,
    this is the name of the VOMS virtual organization (VO) that the
    user's credential is part of.


The following job ClassAd attributes appear in the job ClassAd only for
declared cron jobs. These represent various allotted job start times that
will be used to calculate the jobs :ad-attr:`DeferralTime`. These attributes can
be represented as an integer, a list of integers, a range of integers, a
step (intervals of a range), or an ``*`` for all allowed values. For more
information visit :ref:`crontab`.

:classad-attribute-def:`CronMinute`
    The minutes in an hour when the cron job is allowed to start running.
    Represented by the numerical values 0 to 59.

:classad-attribute-def:`CronHour`
    The hours in the day when the cron job is allowed to start running.
    Represented by the numerical values 0 to 23.

:classad-attribute-def:`CronDayOfMonth`
    The days of the month when the cron job is allowed to start running.
    Represented by the numerical values 1 to 31.

:classad-attribute-def:`CronMonth`
    The months of the year when the cron job is allowed to start running.
    Represented by numerical values 1 to 12.

:classad-attribute-def:`CronDayOfWeek`
    The days of the week when the cron job is allowed to start running.
    Represented by numerical values 0 to 7. Both 0 and 7 represent Sunday.


The following job ClassAd attributes are relevant only for **vm**
universe jobs.

:classad-attribute-def:`VM_MACAddr`
    The MAC address of the virtual machine's network interface, in the
    standard format of six groups of two hexadecimal digits separated by
    colons. This attribute is currently limited to apply only to Xen
    virtual machines.


The following job ClassAd attributes appear in the job ClassAd only for
the :tool:`condor_dagman` job submitted under DAGMan. They represent status
information for the DAG.

:classad-attribute-def:`DAG_InRecovery`
    The value 1 if the DAG is in recovery mode, and The value 0
    otherwise.
    
:classad-attribute-def:`DAG_NodesDone`
    The number of DAG nodes that have finished successfully. This means
    that the entire node has finished, not only an actual HTCondor job
    or jobs.
    
:classad-attribute-def:`DAG_NodesFailed`
    The number of DAG nodes that have failed. This value includes all
    retries, if there are any.

:classad-attribute-def:`DAG_NodesPostrun`
    The number of DAG nodes for which a POST script is running or has
    been deferred because of a POST script throttle setting.

:classad-attribute-def:`DAG_NodesPrerun`
    The number of DAG nodes for which a PRE script is running or has
    been deferred because of a PRE script throttle setting.

:classad-attribute-def:`DAG_NodesQueued`
    The number of DAG nodes for which the actual HTCondor job or jobs
    are queued. The queued jobs may be in any state.

:classad-attribute-def:`DAG_NodesReady`
    The number of DAG nodes that are ready to run, but which have not
    yet started running.

:classad-attribute-def:`DAG_NodesTotal`
    The total number of nodes in the DAG, including the FINAL node, if
    there is a FINAL node.

:classad-attribute-def:`DAG_NodesUnready`
    The number of DAG nodes that are not ready to run. This is a node in
    which one or more of the parent nodes has not yet finished.

:classad-attribute-def:`DAG_NodesFutile`
    The number of DAG nodes that will never run due to the failure of an
    ancestor node. Where an ancestor is a node that a another node
    depends on either directly or indirectly through a chain of PARENT/CHILD
    relationships.

:classad-attribute-def:`DAG_Status`
    The overall status of the DAG, with the same values as the macro
    ``$DAG_STATUS`` used in DAGMan FINAL nodes.

    +------+--------------------------------------+
    | 0    | OK                                   |
    +------+--------------------------------------+
    | 1    | An error has occured                 |
    +------+--------------------------------------+
    | 2    | One or more nodes in the DAG have    |
    |      | failed                               |
    +------+--------------------------------------+
    | 3    | the DAG has been aborted by an       |
    |      | ABORT-DAG-ON specification           |
    +------+--------------------------------------+
    | 4    | DAG was removed via :tool:`condor_rm`|
    +------+--------------------------------------+
    | 5    | A cycle was detected within the DAG  |
    +------+--------------------------------------+
    | 6    | DAG is halted                        |
    |      | (see :ref:`Suspending a DAG`)        |
    +------+--------------------------------------+

:classad-attribute-def:`DAG_AdUpdateTime`
    A timestamp for when the DAGMan process last sent an update of internal
    information to its job ad.

The following job ClassAd attributes appear in the job ClassAd only for
the :tool:`condor_dagman` job submitted under DAGMan. They represent job process
information about the DAG. These values will reset when a DAG is run via
rescue and be retained when a DAG is run via recovery mode.

:classad-attribute-def:`DAG_JobsSubmitted`
    The total number of job processes submitted by all the nodes in the DAG.

:classad-attribute-def:`DAG_JobsIdle`
    The number of job processes currently idle within the DAG. 

:classad-attribute-def:`DAG_JobsHeld`
    The number of job processes currently held within the DAG.

:classad-attribute-def:`DAG_JobsRunning`
    The number of job processes currently executing within the DAG.

:classad-attribute-def:`DAG_JobsCompleted`
    The total number of job processes within the DAG that have successfully
    completed.

The following job ClassAd attributes appear in the job ClassAd for the
:tool:`condor_dagman` job submitted under DAGMan. These values represent
throttling limits active for the specified DAGMan workflow. Using :tool:`condor_qedit`
to modify these value will take effect in the DAGMan workflow.

:classad-attribute-def:`DAGMan_MaxJobs`
    The maximum number of job clusters DAGMan will have submitted at any
    point of time. This can be viewed as the max number of running Nodes
    in a DAG since each Node has one cluster of jobs associated with it.

:classad-attribute-def:`DAGMan_MaxIdle`
    The maximum number of Idle job procs submitted by DAGMan. If this
    number of passed upon submitting a Node job then DAGMan will pause
    submitting new jobs.

:classad-attribute-def:`DAGMan_MaxPreScripts`
    The maximum number of PRE-Scripts DAGMan will execute at a single point
    in time.

:classad-attribute-def:`DAGMan_MaxPostScripts`
    The maximum number of POST-Scripts DAGMan will execute at a single point
    in time.

The following job ClassAd attributes do not appear in the job ClassAd as
kept by the *condor_schedd* daemon. They appear in the job ClassAd
written to the job's execute directory while the job is running.

:classad-attribute-def:`CpusProvisioned`
    The number of Cpus allocated to the job. With statically-allocated
    slots, it is the number of Cpus allocated to the slot. With
    dynamically-allocated slots, it is based upon the job attribute
    :ad-attr:`RequestCpus`, but may be larger due to the minimum given to a
    dynamic slot.

:classad-attribute-def:`CpusUsage`
    CpusUsage (Note the plural `Cpus`) is a floating point value that 
    represents the number of cpu cores fully used over the lifetime of
    the job.  A cpu-bound, single-threaded job will have a CpusUsage of 1.0.
    A job that is blocked on I/O for half of its life and is cpu bound
    for the other have will have a CpusUsage of 0.5.  A job that uses two
    cores fully will have a CpusUsage of 2.0.  Jobs with unexpectedly low 
    CpusUsage may be showing lowered throughput due to blocking on network or disk.

:classad-attribute-def:`DiskProvisioned`
    The amount of disk space in KiB allocated to the job. With
    statically-allocated slots, it is the amount of disk space allocated
    to the slot. With dynamically-allocated slots, it is based upon the
    job attribute :ad-attr:`RequestDisk`, but may be larger due to the minimum
    given to a dynamic slot.

:classad-attribute-def:`MemoryProvisioned`
    The amount of memory in MiB allocated to the job. With
    statically-allocated slots, it is the amount of memory space
    allocated to the slot. With dynamically-allocated slots, it is based
    upon the job attribute :ad-attr:`RequestMemory`, but may be larger due to
    the minimum given to a dynamic slot.

:classad-attribute-def:`<Name>Provisioned`
    The amount of the custom resource identified by ``<Name>`` allocated
    to the job. For jobs using GPUs, ``<Name>`` will be ``GPUs``. With
    statically-allocated slots, it is the amount of the resource
    allocated to the slot. With dynamically-allocated slots, it is based
    upon the job attribute ``Request<Name>``, but may be larger due to
    the minimum given to a dynamic slot.
