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


:classad-attribute:`Absent`
    Boolean set to true ``True`` if the ad is absent.

:classad-attribute:`AcctGroup`
    The accounting group name, as set in the submit description file via
    the
    **accounting_group** :index:`accounting_group<single: accounting_group; submit commands>`
    command. This attribute is only present if an accounting group was
    requested by the submission. See the :doc:`/admin-manual/user-priorities-negotiation` section
    for more information about accounting groups.

:classad-attribute:`AcctGroupUser`
    The user name associated with the accounting group. This attribute
    is only present if an accounting group was requested by the
    submission.

:classad-attribute:`ActivationDuration`
    Formally, the length of time in seconds from when the shadow sends a
    claim activation to when the shadow receives a claim deactivation.

    Informally, this is how much time HTCondor's fair-share mechanism
    will charge the job for, plus one round-trip over the network.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute:`ActivationExecutionDuration`
    Formally, the length of time in seconds from when the shadow received
    notification that the job had been spawned to when the shadow received
    notification that the spawned process has exited.

    Informally, this is the duration limited by ``AllowedExecuteDuration``.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute:`ActivationSetupDuration`
    Formally, the length of time in seconds from when the shadow sends a
    claim activation to when the shadow it notified that the job was
    spawned.

    Informally, this is how long it took the starter to prepare to execute
    the job.  That includes file transfer, so the difference between this
    duration and the duration of input file transfer is (roughly) the
    execute-side overhead of preparing to start the job.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute:`ActivationTeardownDuration`
    Formally, the length of time in seconds from when the shadow received
    notification that the spawned process exited to when the shadow received
    a claim deactivation.


    Informally, this is how long it took the starter to finish up after the
    job.  That includes file transfer, so the difference between this duration
    and the duration of output file transfer is (roughly) the execute-side
    overhead of handling job termination.

    This attribute may not be used in startd policy expressions and is
    not computed until complete.

:classad-attribute:`AllowedExecuteDuration`
    The longest time for which a job may be executing.  Jobs which exceed
    this duration will go on hold.  This time does not include file-transfer
    time.  Jobs which self-checkpoint have this long to write out each
    checkpoint.

    This attribute is intended to help minimize the time wasted by jobs
    which may erroneously run forever.

:classad-attribute:`AllowedJobDuration`
    The longest time for which a job may continuously be in the running state.
    Jobs which exceed this duration will go on hold.  Exiting the running
    state resets the job duration measured by this attribute.

    This attribute is intended to help minimize the time wasted by jobs
    which may erroneously run forever.

:classad-attribute:`AllRemoteHosts`
    String containing a comma-separated list of all the remote machines
    running a parallel or mpi universe job.

:classad-attribute:`Args`
    A string representing the command line arguments passed to the job,
    when those arguments are specified using the old syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute:`Arguments`
    A string representing the command line arguments passed to the job,
    when those arguments are specified using the new syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute:`AuthTokenSubject`
    A string recording the subject in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute:`AuthTokenIssuer`
    A string recording the issuer in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute:`AuthTokenGroups`
    A string recording the groups in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute:`AuthTokenScopes`
    A string recording the scopes in the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute:`AuthTokenId`
    A string recording the unique identifier of the authentication token (IDTOKENS or
    SCITOKENS) used to submit the job.

:classad-attribute:`BatchExtraSubmitArgs`
    For **batch** grid universe jobs, additional command-line arguments
    to be given to the target batch system's job submission command.

:classad-attribute:`BatchProject`
    For **batch** grid universe jobs, the name of the
    project/account/allocation that should be charged for the job's
    resource usage.

:classad-attribute:`BatchQueue`
    For **batch** grid universe jobs, the name of the
    queue in the remote batch system.

:classad-attribute:`BatchRuntime`
    For **batch** grid universe jobs, a limit in seconds on the job's
    execution time, enforced by the remote batch system.

:classad-attribute:`BlockReadKbytes`
    The integer number of KiB read from disk for this job.

:classad-attribute:`BlockReads`
    The integer number of disk blocks read for this job.

:classad-attribute:`BlockWriteKbytes`
    The integer number of KiB written to disk for this job.

:classad-attribute:`BlockWrites`
    The integer number of blocks written to disk for this job.

:classad-attribute:`BoincAuthenticatorFile`
    Used for grid type boinc jobs; a string taken from the definition of
    the submit description file command
    **boinc_authenticator_file** :index:`boinc_authenticator_file<single: boinc_authenticator_file; submit commands>`.
    Defines the path and file name of the file containing the
    authenticator string to use to authenticate to the BOINC service.

:classad-attribute:`CkptArch`
    String describing the architecture of the machine this job executed
    on at the time it last produced a checkpoint. If the job has never
    produced a checkpoint, this attribute is ``undefined``.

:classad-attribute:`CkptOpSys`
    String describing the operating system of the machine this job
    executed on at the time it last produced a checkpoint. If the job
    has never produced a checkpoint, this attribute is ``undefined``.

:classad-attribute:`CloudLabelNames`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    **cloud_label_names** :index:`cloud_label_names<single: cloud_label_names; submit commands>`.
    Defines the set of labels associated with the GCE instance.

:classad-attribute:`ClusterId`
    Integer cluster identifier for this job. A cluster is a group of
    jobs that were submitted together. Each job has its own unique job
    identifier within the cluster, but shares a common cluster
    identifier. The value changes each time a job or set of jobs are
    queued for execution under HTCondor.

:classad-attribute:`Cmd`
    The path to and the file name of the job to be executed.

:classad-attribute:`CommittedTime`
    The number of seconds of wall clock time that the job has been
    allocated a machine, excluding the time spent on run attempts that
    were evicted without a checkpoint. Like ``RemoteWallClockTime``,
    this includes time the job spent in a suspended state, so the total
    committed wall time spent running is

    .. code-block:: condor-classad-expr

        CommittedTime - CommittedSuspensionTime

:index:`SYSTEM_JOB_MACHINE_ATTRS`

:classad-attribute:`CommittedSlotTime`
    This attribute is identical to ``CommittedTime`` except that the
    time is multiplied by the ``SlotWeight`` of the machine(s) that ran
    the job. This relies on ``SlotWeight`` being listed in
    ``SYSTEM_JOB_MACHINE_ATTRS``

:classad-attribute:`CommittedSuspensionTime`
    A running total of the number of seconds the job has spent in
    suspension during time in which the job was not evicted without a
    checkpoint. This number is updated when the job is checkpointed and
    when it exits.

:classad-attribute:`CompletionDate`
    The time when the job completed, or the value 0 if the job has not
    yet completed. Measured in the number of seconds since the epoch
    (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`ConcurrencyLimits`
    A string list, delimited by commas and space characters. The items
    in the list identify named resources that the job requires. The
    value can be a ClassAd expression which, when evaluated in the
    context of the job ClassAd and a matching machine ClassAd, results
    in a string list.

:classad-attribute:`CondorPlatform`
    A string that describes the operating system version that the 
    `condor_submit` command that submitted this job was built for.  Note
    this may be different that the operating system that is actually running.

:classad-attribute:`CondorVersion`
    A string that describes the HTCondor version of the `condor_submit`
    command that created this job.  Note this may be different than the
    version of the HTCondor daemon that runs the job.

:classad-attribute:`ContainerImage`
    For Container universe jobs, the string that names the container image to be run
    the job in.

:classad-attribute:`ContainerTargetDir`
    For Container universe jobs, a filename that becomes the working directory of
    the job.  Mapped to the scratch directory.

:index:`SYSTEM_JOB_MACHINE_ATTRS`

:classad-attribute:`CumulativeSlotTime`
    This attribute is identical to ``RemoteWallClockTime`` except that
    the time is multiplied by the ``SlotWeight`` of the machine(s) that
    ran the job. This relies on ``SlotWeight`` being listed in
    ``SYSTEM_JOB_MACHINE_ATTRS``

:classad-attribute:`CumulativeSuspensionTime`
    A running total of the number of seconds the job has spent in
    suspension for the life of the job.

:classad-attribute:`CumulativeTransferTime`
    The total time, in seconds, that condor has spent transferring the
    input and output sandboxes for the life of the job.

:classad-attribute:`CurrentHosts`
    The number of hosts in the claimed state, due to this job.

:classad-attribute:`DAGManJobId`
    For a DAGMan node job only, the ``ClusterId`` job ClassAd attribute
    of the *condor_dagman* job which is the parent of this node job.
    For nested DAGs, this attribute holds only the ``ClusterId`` of the
    job's immediate parent.

:classad-attribute:`DAGParentNodeNames`
    For a DAGMan node job only, a comma separated list of each *JobName*
    which is a parent node of this job's node. This attribute is passed
    through to the job via the *condor_submit* command line, if it does
    not exceed the line length defined with ``_POSIX_ARG_MAX``. For
    example, if a node job has two parents with *JobName* s B and C,
    the *condor_submit* command line will contain

    .. code-block:: text

          -append +DAGParentNodeNames="B,C"


:classad-attribute:`DAGManNodesLog`
    For a DAGMan node job only, gives the path to an event log used
    exclusively by DAGMan to monitor the state of the DAG's jobs. Events
    are written to this log file in addition to any log file specified
    in the job's submit description file.

:classad-attribute:`DAGManNodesMask`
    For a DAGMan node job only, a comma-separated list of the event
    codes that should be written to the log specified by
    ``DAGManNodesLog``, known as the auxiliary log. All events not
    specified in the ``DAGManNodesMask`` string are not written to the
    auxiliary event log. The value of this attribute is determined by
    DAGMan, and it is passed to the job via the *condor_submit* command
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

    If ``DAGManNodesLog`` is not defined, it has no effect. The value of
    ``DAGManNodesMask`` does not affect events recorded in the job event
    log file referred to by ``UserLog``.

:index:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`

:classad-attribute:`DelegateJobGSICredentialsLifetime`
    An integer that specifies the maximum number of seconds for which
    delegated proxies should be valid. The default behavior is
    determined by the configuration setting
    ``DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`` which defaults
    to one day. A value of 0 indicates that the delegated proxy should
    be valid for as long as allowed by the credential used to create the
    proxy. This setting currently only applies to proxies delegated for
    non-grid jobs and HTCondor-C jobs.
    This setting has no effect if the configuration setting
    ``DELEGATE_JOB_GSI_CREDENTIALS``
    :index:`DELEGATE_JOB_GSI_CREDENTIALS` is false, because in
    that case the job proxy is copied rather than delegated.

:classad-attribute:`DiskUsage`
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

:classad-attribute:`DockerImage`
    For Docker and Container universe jobs, a string that names the docker image to run
    inside the container.

:classad-attribute:`EC2AccessKeyId`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_access_key_id** :index:`ec2_access_key_id<single: ec2_access_key_id; submit commands>`.
    Defines the path and file name of the file containing the EC2 Query
    API's access key. 
    
:classad-attribute:`EC2AmiID`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_ami_id** :index:`ec2_ami_id<single: ec2_ami_id; submit commands>`.
    Identifies the machine image of the instance.

:classad-attribute:`EC2BlockDeviceMapping`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_block_device_mapping** :index:`ec2_block_device_mapping<single: ec2_block_device_mapping; submit commands>`.
    Defines the map from block device names to kernel device names for
    the instance. 
    
:classad-attribute:`EC2ElasticIp`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_elastic_ip** :index:`ec2_elastic_ip<single: ec2_elastic_ip; submit commands>`.
    Specifies an Elastic IP address to associate with the instance.

:classad-attribute:`EC2IamProfileArn`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_iam_profile_arn** :index:`ec2_iam_profile_arn<single: ec2_iam_profile_arn; submit commands>`.
    Specifies the IAM (instance) profile to associate with this
    instance. 

:classad-attribute:`EC2IamProfileName`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_iam_profile_name** :index:`ec2_iam_profile_name<single: ec2_iam_profile_name; submit commands>`.
    Specifies the IAM (instance) profile to associate with this
    instance.

:classad-attribute:`EC2InstanceName`
    Used for grid type ec2 jobs; a string set for the job once the
    instance starts running, as assigned by the EC2 service, that
    represents the unique ID assigned to the instance by the EC2
    service.

:classad-attribute:`EC2InstanceType`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_instance_type** :index:`ec2_instance_type<single: ec2_instance_type; submit commands>`.
    Specifies a service-specific instance type.

:classad-attribute:`EC2KeyPair`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_keypair** :index:`ec2_keypair<single: ec2_keypair; submit commands>`.
    Defines the key pair associated with the EC2 instance.

:classad-attribute:`EC2ParameterNames`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_parameter_names** :index:`ec2_parameter_names<single: ec2_parameter_names; submit commands>`.
    Contains a space or comma separated list of the names of additional
    parameters to pass when instantiating an instance.

:classad-attribute:`EC2SpotPrice`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_spot_price** :index:`ec2_spot_price<single: ec2_spot_price; submit commands>`.
    Defines the maximum amount per hour a job submitter is willing to
    pay to run this job.

:classad-attribute:`EC2SpotRequestID`
    Used for grid type ec2 jobs; identifies the spot request HTCondor
    made on behalf of this job.

:classad-attribute:`EC2StatusReasonCode`
    Used for grid type ec2 jobs; reports the reason for the most recent
    EC2-level state transition. Can be used to determine if a spot
    request was terminated due to a rise in the spot price.

:classad-attribute:`EC2TagNames`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_tag_names** :index:`ec2_tag_names<single: ec2_tag_names; submit commands>`.
    Defines the set, and case, of tags associated with the EC2 instance.

:classad-attribute:`EC2KeyPairFile`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_keypair_file** :index:`ec2_keypair_file<single: ec2_keypair_file; submit commands>`.
    Defines the path and file name of the file into which to write the
    SSH key used to access the image, once it is running.

:classad-attribute:`EC2RemoteVirtualMachineName`
    Used for grid type ec2 jobs; a string set for the job once the
    instance starts running, as assigned by the EC2 service, that
    represents the host name upon which the instance runs, such that the
    user can communicate with the running instance.

:classad-attribute:`EC2SecretAccessKey`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_secret_access_key** :index:`ec2_secret_access_key<single: ec2_secret_access_key; submit commands>`.
    Defines that path and file name of the file containing the EC2 Query
    API's secret access key.

:classad-attribute:`EC2SecurityGroups`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_security_groups** :index:`ec2_security_groups<single: ec2_security_groups; submit commands>`.
    Defines the list of EC2 security groups which should be associated
    with the job.

:classad-attribute:`EC2SecurityIDs`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_security_ids** :index:`ec2_security_ids<single: ec2_security_ids; submit commands>`.
    Defines the list of EC2 security group IDs which should be
    associated with the job.

:classad-attribute:`EC2UserData`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_user_data** :index:`ec2_user_data<single: ec2_user_data; submit commands>`.
    Defines a block of data that can be accessed by the virtual machine.

:classad-attribute:`EC2UserDataFile`
    Used for grid type ec2 jobs; a string taken from the definition of
    the submit description file command
    **ec2_user_data_file** :index:`ec2_user_data_file<single: ec2_user_data_file; submit commands>`.
    Specifies a path and file name of a file containing data that can be
    accessed by the virtual machine.

:classad-attribute:`EmailAttributes`
    A string containing a comma-separated list of job ClassAd
    attributes. For each attribute name in the list, its value will be
    included in the e-mail notification upon job completion.

:classad-attribute:`EncryptExecuteDirectory`
    A boolean value taken from the submit description file command
    **encrypt_execute_directory** :index:`encrypt_execute_directory<single: encrypt_execute_directory; submit commands>`.
    It specifies if HTCondor should encrypt the remote scratch directory
    on the machine where the job executes.

:classad-attribute:`EnteredCurrentStatus`
    An integer containing the epoch time of when the job entered into
    its current status So for example, if the job is on hold, the
    ClassAd expression

    .. code-block:: condor-classad-expr

            time() - EnteredCurrentStatus

    will equal the number of seconds that the job has been on hold.

:classad-attribute:`Env`
    A string representing the environment variables passed to the job,
    when those arguments are specified using the old syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute:`Environment`
    A string representing the environment variables passed to the job,
    when those arguments are specified using the new syntax, as
    specified in
    the :doc:`/man-pages/condor_submit` section.

:classad-attribute:`EraseOutputAndErrorOnRestart`
    A boolean.  If missing or true, HTCondor will erase (truncate) the error
    and output logs when the job restarts.  If this attribute is false, and
    ``when_to_transfer_output`` is ``ON_EXIT_OR_EVICT``, HTCondor will instead
    append to those files.

:classad-attribute:`ExecutableSize`
    Size of the executable in KiB.

:classad-attribute:`ExitBySignal`
    An attribute that is ``True`` when a user job exits via a signal and
    ``False`` otherwise. For some grid universe jobs, how the job exited
    is unavailable. In this case, ``ExitBySignal`` is set to ``False``.

:classad-attribute:`ExitCode`
    When a user job exits by means other than a signal, this is the exit
    return code of the user job. For some grid universe jobs, how the
    job exited is unavailable. In this case, ``ExitCode`` is set to 0.

:classad-attribute:`ExitSignal`
    When a user job exits by means of an unhandled signal, this
    attribute takes on the numeric value of the signal. For some grid
    universe jobs, how the job exited is unavailable. In this case,
    ``ExitSignal`` will be undefined.

:classad-attribute:`ExitStatus`
    The way that HTCondor previously dealt with a job's exit status.
    This attribute should no longer be used. It is not always accurate
    in heterogeneous pools, or if the job exited with a signal. Instead,
    see the attributes: ``ExitBySignal``, ``ExitCode``, and
    ``ExitSignal``.
    
:classad-attribute:`GceAuthFile`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    **gce_auth_file** :index:`gce_auth_file<single: gce_auth_file; submit commands>`.
    Defines the path and file name of the file containing authorization
    credentials to use the GCE service.

:classad-attribute:`GceImage`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    **gce_image** :index:`gce_image<single: gce_image; submit commands>`.
    Identifies the machine image of the instance.

:classad-attribute:`GceJsonFile`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    **gce_json_file** :index:`gce_json_file<single: gce_json_file; submit commands>`.
    Specifies the path and file name of a file containing a set of JSON
    object members that should be added to the instance description
    submitted to the GCE service.

:classad-attribute:`GceMachineType`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    **gce_machine_type** :index:`gce_machine_type<single: gce_machine_type; submit commands>`.
    Specifies the hardware profile that should be used for a GCE
    instance.
    
:classad-attribute:`GceMetadata`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    **gce_metadata** :index:`gce_metadata<single: gce_metadata; submit commands>`.
    Defines a set of name/value pairs that can be accessed by the
    virtual machine.

:classad-attribute:`GceMetadataFile`
    Used for grid type gce jobs; a string taken from the definition of
    the submit description file command
    **gce_metadata_file** :index:`gce_metadata_file<single: gce_metadata_file; submit commands>`.
    Specifies a path and file name of a file containing a set of
    name/value pairs that can be accessed by the virtual machine.

:classad-attribute:`GcePreemptible`
    Used for grid type gce jobs; a boolean taken from the definition of
    the submit description file command
    **gce_preemptible** :index:`gce_preemptible<single: gce_preemptible; submit commands>`.
    Specifies whether the virtual machine instance created in GCE should
    be preemptible.

:classad-attribute:`GlobalJobId`
    A string intended to be a unique job identifier within a pool. It
    currently contains the *condor_schedd* daemon ``Name`` attribute, a
    job identifier composed of attributes ``ClusterId`` and ``ProcId``
    separated by a period, and the job's submission time in seconds
    since 1970-01-01 00:00:00 UTC, separated by # characters. The value
    submit.example.com#152.3#1358363336 is an example.  While HTCondor
    guaratees this string will be globally unique, the contents are subject
    to change, and users should not parse out components of this string.

:classad-attribute:`GridJobStatus`
    A string containing the job's status as reported by the remote job
    management system.

:classad-attribute:`GridResource`
    A string defined by the right hand side of the the submit
    description file command
    **grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`.
    It specifies the target grid type, plus additional parameters
    specific to the grid type.

:classad-attribute:`HoldKillSig`
    Currently only for scheduler and local universe jobs, a string
    containing a name of a signal to be sent to the job if the job is
    put on hold.

:classad-attribute:`HoldReason`
    A string containing a human-readable message about why a job is on
    hold. This is the message that will be displayed in response to the
    command ``condor_q -hold``. It can be used to determine if a job should
    be released or not.

:classad-attribute:`HoldReasonCode`
    An integer value that represents the reason that a job was put on
    hold.  The below table defines all possible values used by 
    attributes ``HoldReasonCode``, ``NumHoldsByReason``, and ``HoldReasonSubCode``. 

    +----------------------------------+-------------------------------------+--------------------------+
    | | Integer HoldReasonCode         | | Reason for Hold                   | | HoldReasonSubCode      |
    | | [NumHoldsByReason Label]       |                                     |                          |
    +==================================+=====================================+==========================+
    | | 1                              | The user put the job on             |                          |
    | | [UserRequest]                  | hold with *condor_hold*.            |                          |
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
    | | 12                             | The *condor_starter* or             | The Unix errno number.   |
    | | [DownloadFileError]            | *condor_shadow* failed              |                          |
    |                                  | to receive or write job             |                          |
    |                                  | files.                              |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 13                             | The *condor_starter* or             | The Unix errno number.   |
    | | [UploadFileError]              | *condor_shadow* failed              |                          |
    |                                  | to read or send job                 |                          |
    |                                  | files.                              |                          |
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
    | | 19                             | ``<Keyword>_HOOK_PREPARE_JOB``      |                          |
    | | [HookPrepareJobFailure]        | :index:`<Keyword>_HOOK_PREPARE_JOB` |                          |
    |                                  | was defined but could               |                          |
    |                                  | not be executed or                  |                          |
    |                                  | returned failure.                   |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 20                             | The job missed its                  |                          |
    | | [MissedDeferredExecutionTime]  | deferred execution time             |                          |
    |                                  | and therefore failed to             |                          |
    |                                  | run.                                |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 21                             | The job was put on hold             |                          |
    | | [StartdHeldJob]                | because ``WANT_HOLD``               |                          |
    |                                  | :index:`WANT_HOLD`                  |                          |
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
    | | 26                             | ``SYSTEM_PERIODIC_HOLD``            |                          |
    | | [SystemPolicy]                 | :index:`SYSTEM_PERIODIC_HOLD`       |                          |
    |                                  | evaluated to true.                  |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 27                             | The system periodic job             |                          |
    | | [SystemPolicyUndefined]        | policy evaluated to                 |                          |
    |                                  | undefined.                          |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 32                             | The maximum total input             |                          |
    | | [MaxTransferInputSizeExceeded] | file transfer size was              |                          |
    |                                  | exceeded. (See                      |                          |
    |                                  | ``MAX_TRANSFER_INPUT_MB``           |                          |
    |                                  | :index:`MAX_TRANSFER_INPUT_MB`      |                          |
    +----------------------------------+-------------------------------------+--------------------------+
    | | 33                             | The maximum total output            |                          |
    | | [MaxTransferOutputSizeExceeded]| file transfer size was              |                          |
    |                                  | exceeded. (See                      |                          |
    |                                  | ``MAX_TRANSFER_OUTPUT_MB``          |                          |
    |                                  | :index:`MAX_TRANSFER_OUTPUT_MB`     |                          |
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
    | | 39                             | Adminstrator error in               |                          |
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
    | | [JobExecutionTimeExceeded]     | was exceeded.                       |                          |
    +----------------------------------+-------------------------------------+--------------------------+


:classad-attribute:`HoldReasonSubCode`
    An integer value that represents further information to go along
    with the ``HoldReasonCode``, for some values of ``HoldReasonCode``.
    See ``HoldReasonCode`` for a table of possible values.

:classad-attribute:`HookKeyword`
    A string that uniquely identifies a set of job hooks, and added to
    the ClassAd once a job is fetched.

:classad-attribute:`ImageSize`
    Maximum observed memory image size (i.e. virtual memory) of the job
    in KiB. The initial value is equal to the size of the executable for
    non-vm universe jobs, and 0 for vm universe jobs. When the job
    writes a checkpoint, the ``ImageSize`` attribute is set to the size
    of the checkpoint file (since the checkpoint file contains the job's
    memory image). A vanilla universe job's ``ImageSize`` is recomputed
    internally every 15 seconds. How quickly this updated information
    becomes visible to *condor_q* is controlled by
    ``SHADOW_QUEUE_UPDATE_INTERVAL`` and ``STARTER_UPDATE_INTERVAL``.

    Under Linux, ``ProportionalSetSize`` is a better indicator of memory
    usage for jobs with significant sharing of memory between processes,
    because ``ImageSize`` is simply the sum of virtual memory sizes
    across all of the processes in the job, which may count the same
    memory pages more than once.

:classad-attribute:`IOWait`
    I/O wait time of the job recorded by the cgroup controller in
    seconds.

:classad-attribute:`IwdFlushNFSCache`
    A boolean expression that controls whether or not HTCondor attempts
    to flush a submit machine's NFS cache, in order to refresh an
    HTCondor job's initial working directory. The value will be
    ``True``, unless a job explicitly adds this attribute, setting it to
    ``False``.

:classad-attribute:`JobAdInformationAttrs`
    A comma-separated list of attribute names. The named attributes and
    their values are written in the job event log whenever any event is
    being written to the log. This is the same as the configuration
    setting ``EVENT_LOG_INFORMATION_ATTRS`` (see
    :ref:`admin-manual/configuration-macros:daemon logging configuration file
    entries`) but it applies to the job event log instead of the system event log.

:classad-attribute:`JobBatchName`
    If a job is given a batch name with the -batch-name option to `condor_submit`, this 
    string valued attribute will contain the batch name.

:classad-attribute:`JobCurrentFinishTransferInputDate`
    Time at which the job most recently finished transferring its input
    sandbox. Measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970)

:classad-attribute:`JobCurrentFinishTransferOutputDate`
    Time at which the job most recently finished transferring its output
    sandbox. Measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970)

:classad-attribute:`JobCurrentStartDate`
    Time at which the job most recently began running. Measured in the
    number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`JobCurrentStartExecutingDate`
    Time at which the job most recently finished transferring its input
    sandbox and began executing. Measured in the number of seconds since
    the epoch (00:00:00 UTC, Jan 1, 1970)

:classad-attribute:`JobCurrentStartTransferInputDate`
    Time at which the job most recently began transferring its input
    sandbox. Measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970)

:classad-attribute:`JobCurrentStartTransferOutputDate`
    Time at which the job most recently finished executing and began
    transferring its output sandbox. Measured in the number of seconds
    since the epoch (00:00:00 UTC, Jan 1, 1970)

:classad-attribute:`JobDescription`
    A string that may be defined for a job by setting
    **description** :index:`description<single: description; submit commands>` in the
    submit description file. When set, tools which display the
    executable such as *condor_q* will instead use this string. For
    interactive jobs that do not have a submit description file, this
    string will default to ``"Interactive job"``.

:classad-attribute:`JobDisconnectedDate`
    Time at which the *condor_shadow* and *condor_starter* become disconnected.
    Set to ``Undefined`` when a succcessful reconnect occurs. Measured in the
    number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`JobLeaseDuration`
    The number of seconds set for a job lease, the amount of time that a
    job may continue running on a remote resource, despite its
    submitting machine's lack of response. See
    :ref:`users-manual/special-environment-considerations:job leases`
    for details on job leases.

:classad-attribute:`JobMaxVacateTime`
    An integer expression that specifies the time in seconds requested
    by the job for being allowed to gracefully shut down.

:classad-attribute:`JobNotification`
    An integer indicating what events should be emailed to the user. The
    integer values correspond to the user choices for the submit command
    **notification** :index:`notification<single: notification; submit commands>`.

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


:classad-attribute:`JobPrio`
    Integer priority for this job, set by *condor_submit* or
    *condor_prio*. The default value is 0. The higher the number, the
    greater (better) the priority.

:classad-attribute:`JobRunCount`
    This attribute is retained for backwards compatibility. It may go
    away in the future. It is equivalent to ``NumShadowStarts`` for all
    universes except **scheduler**. For the **scheduler** universe, this
    attribute is equivalent to ``NumJobStarts``.

:classad-attribute:`JobStartDate`
    Time at which the job first began running. Measured in the number of
    seconds since the epoch (00:00:00 UTC, Jan 1, 1970). Due to a long
    standing bug in the 8.6 series and earlier, the job classad that is
    internal to the *condor_startd* and *condor_starter* sets this to
    the time that the job most recently began executing. This bug is
    scheduled to be fixed in the 8.7 series.

:index:`state<single: state; job>`

:classad-attribute:`JobStatus`
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

:classad-attribute:`JobSubmitMethod`
    Integer which indicates how a job was submitted to HTCondor. Users can
    set a custom value for job via Python Bindings API.
 
    +-----------+------------------------+
    | Value     | Method of Submission   |
    +===========+========================+
    | Undefined | Unknown                |
    +-----------+------------------------+
    | 0         | *condor_submit*        |
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


:index:`JobUniverse<single: JobUniverse; ClassAd job attribute>`
:index:`job ClassAd attribute<single: job ClassAd attribute; JobUniverse>`
:index:`universe<single: universe; job>`
:index:`standard = 1 (no longer used)<single: standard = 1; job ClassAd attribute definitions>`
:index:`pipe = 2 (no longer used)<single: pipe = 2 (no longer used); job ClassAd attribute definitions>`
:index:`linda = 3 (no longer used)<single: linda = 3 (no longer used); job ClassAd attribute definitions>`
:index:`pvm = 4 (no longer used)<single: pvm = 4 (no longer used); job ClassAd attribute definitions>`
:index:`vanilla = 5, docker = 5<single: vanilla = 5, docker = 5; job ClassAd attribute definitions>`
:index:`pvmd = 6 (no longer used)<single: pvmd = 6 (no longer used); job ClassAd attribute definitions>`
:index:`scheduler = 7<single: scheduler = 7; job ClassAd attribute definitions>`
:index:`mpi = 8<single: mpi = 8; job ClassAd attribute definitions>`
:index:`grid = 9<single: grid = 9; job ClassAd attribute definitions>`
:index:`parallel = 10<single: parallel = 10; job ClassAd attribute definitions>`
:index:`java = 11<single: java = 11; job ClassAd attribute definitions>`
:index:`local = 12<single: local = 12; job ClassAd attribute definitions>`
:index:`vm = 13<single: vm = 13; job ClassAd attribute definitions>`


``JobUniverse``
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


:classad-attribute:`KeepClaimIdle`
    An integer value that represents the number of seconds that the
    *condor_schedd* will continue to keep a claim, in the Claimed Idle
    state, after the job with this attribute defined completes, and
    there are no other jobs ready to run from this user. This attribute
    may improve the performance of linear DAGs, in the case when a
    dependent job can not be scheduled until its parent has completed.
    Extending the claim on the machine may permit the dependent job to
    be scheduled with less delay than with waiting for the
    *condor_negotiator* to match with a new machine.

:classad-attribute:`KillSig`
    The Unix signal number that the job wishes to be sent before being
    forcibly killed. It is relevant only for jobs running on Unix
    machines. 
    
:classad-attribute:`KillSigTimeout`
    This attribute is replaced by the functionality in
    ``JobMaxVacateTime`` as of HTCondor version 7.7.3. The number of
    seconds that the job requests the
    *condor_starter* wait after sending the signal defined as
    ``KillSig`` and before forcibly removing the job. The actual amount
    of time will be the minimum of this value and the execute machine's
    configuration variable ``KILLING_TIMEOUT``

:classad-attribute:`LastMatchTime``
    An integer containing the epoch time when the job was last
    successfully matched with a resource (gatekeeper) Ad.

:classad-attribute:`LastRejMatchReason`
    If, at any point in the past, this job failed to match with a
    resource ad, this attribute will contain a string with a
    human-readable message about why the match failed.

:classad-attribute:`LastRejMatchTime`
    An integer containing the epoch time when HTCondor-G last tried to
    find a match for the job, but failed to do so.

:classad-attribute:`LastRemotePool`
    The name of the *condor_collector* of the pool in which a job ran
    via flocking in the most recent run attempt. This attribute is not
    defined if the job did not run via flocking.

:classad-attribute:`LastSuspensionTime`
    Time at which the job last performed a successful suspension.
    Measured in the number of seconds since the epoch (00:00:00 UTC, Jan
    1, 1970).
    
:classad-attribute:`LastVacateTime`
    Time at which the job was last evicted from a remote workstation.
    Measured in the number of seconds since the epoch (00:00:00 UTC, Jan
    1, 1970).
    
:classad-attribute:`LeaveJobInQueue`
    A boolean expression that defaults to ``False``, causing the job to
    be removed from the queue upon completion. An exception is if the
    job is submitted using ``condor_submit -spool``. For this case, the
    default expression causes the job to be kept in the queue for 10
    days after completion.

:classad-attribute:`MachineAttr<X><N>`
    Machine attribute of name ``<X>`` that is placed into this job
    ClassAd, as specified by the configuration variable
    ``SYSTEM_JOB_MACHINE_ATTRS``. With the potential for multiple run
    attempts, ``<N>`` represents an integer value providing historical
    values of this machine attribute for multiple runs. The most recent
    run will have a value of ``<N>`` equal to ``0``. The next most
    recent run will have a value of ``<N>`` equal to ``1``.

:classad-attribute:`MaxHosts`
    The maximum number of hosts that this job would like to claim. As
    long as ``CurrentHosts`` is the same as ``MaxHosts``, no more hosts
    are negotiated for.

:classad-attribute:`MaxJobRetirementTime`
    Maximum time in seconds to let this job run uninterrupted before
    kicking it off when it is being preempted. This can only decrease
    the amount of time from what the corresponding startd expression
    allows. 

:index:`MAX_TRANSFER_INPUT_MB`

:classad-attribute:`MaxTransferInputMB`
    This integer expression specifies the maximum allowed total size in
    Mbytes of the input files that are transferred for a job. This
    expression does not apply to grid universe or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the system setting ``MAX_TRANSFER_INPUT_MB``
    is used. If the observed size
    of all input files at submit time is larger than the limit, the job
    will be immediately placed on hold with a ``HoldReasonCode`` value
    of 32. If the job passes this initial test, but the size of the
    input files increases or the limit decreases so that the limit is
    violated, the job will be placed on hold at the time when the file
    transfer is attempted.

:index:`MAX_TRANSFER_OUTPUT_MB`

:classad-attribute:`MaxTransferOutputMB`
    This integer expression specifies the maximum allowed total size in
    Mbytes of the output files that are transferred for a job. This
    expression does not apply to grid universe or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the system setting ``MAX_TRANSFER_OUTPUT_MB``
    is used. If the total size of
    the job's output files to be transferred is larger than the limit,
    the job will be placed on hold with a ``HoldReasonCode`` value of
    33. The output will be transferred up to the point when the limit is
    hit, so some files may be fully transferred, some partially, and
    some not at all.

:classad-attribute:`MemoryUsage`
    An integer expression in units of Mbytes that represents the peak
    memory usage for the job. Its purpose is to be compared with the
    value defined by a job with the
    **request_memory** :index:`request_memory<single: request_memory; submit commands>`
    submit command, for purposes of policy evaluation.

:classad-attribute:`MinHosts`
    The minimum number of hosts that must be in the claimed state for
    this job, before the job may enter the running state.

:index:`MAX_NEXT_JOB_START_DELAY`

:classad-attribute:`NextJobStartDelay`
    An integer number of seconds delay time after this job starts until
    the next job is started. The value is limited by the configuration
    variable ``MAX_NEXT_JOB_START_DELAY``

:classad-attribute:`NiceUser`
    Boolean value which when ``True`` indicates that this job is a nice
    job, raising its user priority value, thus causing it to run on a
    machine only when no other HTCondor jobs want the machine.

:classad-attribute:`Nonessential` 
    A boolean value only relevant to grid universe jobs, which when
    ``True`` tells HTCondor to simply abort (remove) any problematic
    job, instead of putting the job on hold. It is the equivalent of
    doing *condor_rm* followed by *condor_rm* **-forcex** any time the
    job would have otherwise gone on hold. If not explicitly set to
    ``True``, the default value will be ``False``.

:classad-attribute:`NTDomain`
    A string that identifies the NT domain under which a job's owner
    authenticates on a platform running Windows.

:classad-attribute:`NumCkpts`
    A count of the number of checkpoints written by this job during its
    lifetime.
    
:classad-attribute:`NumHolds`
    An integer value that will increment every time a job is placed on hold.
    It may be undefined until the job has been held at least once.

:classad-attribute:`NumHoldsByReason`
    The value of this attribute is a (nested) classad containing a count of how many times a job has been placed 
    on  hold grouped by the reason the job went on hold.  It may be undefined until the job has been held
    at least once. Each attribute name in this classad is
    a NumHoldByReason label; see the table above under 
    the documentation for job attribute ``HoldReasonCode`` for a table of possible values. Each attribute
    value is an integer stating how many times the job went on hold for that specific reason.  An example:

    .. code-block:: condor-classad

        NumHoldsByReason = [ UserRequest = 2; JobPolicy = 110; UnableToOpenInput = 1 ]

:classad-attribute:`NumJobCompletions`
    An integer, initialized to zero, that is incremented by the
    *condor_shadow* each time the job's executable exits of its own
    accord, with or without errors, and successfully completes file
    transfer (if requested). Jobs which have done so normally enter the
    completed state; this attribute is therefore normally only of use
    when, for example, ``on_exit_remove`` or ``on_exit_hold`` is set.

:classad-attribute:`NumJobMatches`
    An integer that is incremented by the *condor_schedd* each time the
    job is matched with a resource ad by the negotiator.

:classad-attribute:`NumJobReconnects`
    An integer count of the number of times a job successfully
    reconnected after being disconnected. This occurs when the
    *condor_shadow* and *condor_starter* lose contact, for example
    because of transient network failures or a *condor_shadow* or
    *condor_schedd* restart. This attribute is only defined for jobs
    that can reconnected: those in the **vanilla** and **java**
    universes.
    
:classad-attribute:`NumJobStarts`
    An integer count of the number of times the job started executing.

:classad-attribute:`NumPids`
    A count of the number of child processes that this job has.

:classad-attribute:`NumRestarts`
    A count of the number of restarts from a checkpoint attempted by
    this job during its lifetime.

:classad-attribute:`NumShadowExceptions`
    An integer count of the number of times the *condor_shadow* daemon
    had a fatal error for a given job.

:classad-attribute:`NumShadowStarts`
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

:classad-attribute:`NumSystemHolds`
    An integer that is incremented each time HTCondor-G places a job on
    hold due to some sort of error condition. This counter is useful,
    since HTCondor-G will always place a job on hold when it gives up on
    some error condition. Note that if the user places the job on hold
    using the *condor_hold* command, this attribute is not incremented.

:classad-attribute:`OtherJobRemoveRequirements`
    A string that defines a list of jobs. When the job with this
    attribute defined is removed, all other jobs defined by the list are
    also removed. The string is an expression that defines a constraint
    equivalent to the one implied by the command

    .. code-block:: console

          $ condor_rm -constraint <constraint>

    This attribute is used for jobs managed with *condor_dagman* to
    ensure that node jobs of the DAG are removed when the
    *condor_dagman* job itself is removed. Note that the list of jobs
    defined by this attribute must not form a cyclic removal of jobs, or
    the *condor_schedd* will go into an infinite loop when any of the
    jobs is removed.

:classad-attribute:`OutputDestination`
    A URL, as defined by submit command **output_destination**.

:classad-attribute:`Owner`
    String describing the user who submitted this job.

:classad-attribute:`ParallelShutdownPolicy`
    A string that is only relevant to parallel universe jobs. Without
    this attribute defined, the default policy applied to parallel
    universe jobs is to consider the whole job completed when the first
    node exits, killing processes running on all remaining nodes. If
    defined to the following strings, HTCondor's behavior changes:

     ``"WAIT_FOR_ALL"``
        HTCondor will wait until every node in the parallel job has
        completed to consider the job finished.

:index:`Starter pre and post scripts`

:classad-attribute:`PostArgs`
    Defines the command-line arguments for the post command using the
    old argument syntax, as specified in :doc:`/man-pages/condor_submit`.
    If both ``PostArgs`` and ``PostArguments`` exists, the former is ignored.

:classad-attribute:`PostArguments`
    Defines the command-line arguments for the post command using the
    new argument syntax, as specified in
    :doc:`/man-pages/condor_submit`, excepting that
    double quotes must be escaped with a backslash instead of another
    double quote. If both ``PostArgs`` and ``PostArguments`` exists, the
    former is ignored.
    
:classad-attribute:`PostCmd`
    A job in the vanilla, Docker, Java, or virtual machine universes may
    specify a command to run after the
    **Executable** :index:`Executable<single: Executable; submit commands>` has
    exited, but before file transfer is started. Unlike a DAGMan POST
    script command, this command is run on the execute machine; however,
    it is not run in the same environment as the
    **Executable** :index:`Executable<single: Executable; submit commands>`.
    Instead, its environment is set by ``PostEnv`` or
    ``PostEnvironment``. Like the DAGMan POST script command, this
    command is not run in the same universe as the
    **Executable** :index:`Executable<single: Executable; submit commands>`; in
    particular, this command is not run in a Docker container, nor in a
    virtual machine, nor in Java. This command is also not run with any
    of vanilla universe's features active, including (but not limited
    to): cgroups, PID namespaces, bind mounts, CPU affinity,
    Singularity, or job wrappers. This command is not automatically
    transferred with the job, so if you're using file transfer, you must
    add it to the
    **transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
    list.

    If the specified command is in the job's execute directory, or any
    sub-directory, you should not set
    **vm_no_output_vm** :index:`vm_no_output_vm<single: vm_no_output_vm; submit commands>`,
    as that will delete all the files in the job's execute directory
    before this command has a chance to run. If you don't want any
    output back from your VM universe job, but you do want to run a post
    command, do not set
    **vm_no_output_vm** :index:`vm_no_output_vm<single: vm_no_output_vm; submit commands>`
    and instead delete the job's execute directory in your post command.

:classad-attribute:`PostCmdExitBySignal`
    If ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` were set,
    and the post command has run, this attribute will true if the the
    post command exited on a signal and false if it did not. It is
    otherwise unset.

:classad-attribute:`PostCmdExitCode`
    If ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` were set,
    the post command has run, and the post command did not exit on a
    signal, then this attribute will be set to the exit code. It is
    otherwise unset.

:classad-attribute:`PostCmdExitSignal`
    If ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` were set,
    the post command has run, and the post command exited on a signal,
    then this attribute will be set to that signal. It is otherwise
    unset.

:classad-attribute:`PostEnv`
    Defines the environment for the Postscript using the Old environment
    syntax. If both ``PostEnv`` and ``PostEnvironment`` exist, the
    former is ignored.

:classad-attribute:`PostEnvironment`
    Defines the environment for the Postscript using the New environment
    syntax. If both ``PostEnv`` and ``PostEnvironment`` exist, the
    former is ignored.

:classad-attribute:`PreArgs`
    Defines the command-line arguments for the pre command using the old
    argument syntax, as specified in :doc:`/man-pages/condor_submit`. If both
    ``PreArgs`` and ``PreArguments`` exists, the former is ignored.

:classad-attribute:`PreArguments`
    Defines the command-line arguments for the pre command using the new
    argument syntax, as specified in
    :doc:`/man-pages/condor_submit`, excepting that
    double quotes must be escape with a backslash instead of another
    double quote. If both ``PreArgs`` and ``PreArguments`` exists, the
    former is ignored.

:classad-attribute:`PreCmd`
    A job in the vanilla, Docker, Java, or virtual machine universes may
    specify a command to run after file transfer (if any) completes but
    before the
    **Executable** :index:`Executable<single: Executable; submit commands>` is
    started. Unlike a DAGMan PRE script command, this command is run on
    the execute machine; however, it is not run in the same environment
    as the **Executable** :index:`Executable<single: Executable; submit commands>`.
    Instead, its environment is set by ``PreEnv`` or ``PreEnvironment``.
    Like the DAGMan POST script command, this command is not run in the
    same universe as the
    **Executable** :index:`Executable<single: Executable; submit commands>`; in
    particular, this command is not run in a Docker container, nor in a
    virtual machine, nor in Java. This command is also not run with any
    of vanilla universe's features active, including (but not limited
    to): cgroups, PID namespaces, bind mounts, CPU affinity,
    Singularity, or job wrappers. This command is not automatically
    transferred with the job, so if you're using file transfer, you must
    add it to the
    **transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
    list. 
    
:classad-attribute:`PreCmdExitBySignal`
    If ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` were set, and
    the pre command has run, this attribute will true if the the pre
    command exited on a signal and false if it did not. It is otherwise
    unset.
    
:classad-attribute:`PreCmdExitCode`
    If ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` were set, the
    pre command has run, and the pre command did not exit on a signal,
    then this attribute will be set to the exit code. It is otherwise
    unset.
    
:classad-attribute:`PreCmdExitSignal`
    If ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` were set, the
    pre command has run, and the pre command exited on a signal, then
    this attribute will be set to that signal. It is otherwise unset.

:classad-attribute:`PreEnv`
    Defines the environment for the prescript using the Old environment
    syntax. If both ``PreEnv`` and ``PreEnvironment`` exist, the former
    is ignored.
    
:classad-attribute:`PreEnvironment`
    Defines the environment for the prescript using the New environment
    syntax. If both ``PreEnv`` and ``PreEnvironment`` exist, the former
    is ignored.

:classad-attribute:`PreJobPrio1`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered first: before ``PreJobPrio2``,
    before ``JobPrio``, before ``PostJobPrio1``, before
    ``PostJobPrio2``, and before ``QDate``.

:classad-attribute:`PreJobPrio2`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after ``PreJobPrio1``, but before
    ``JobPrio``, before ``PostJobPrio1``, before ``PostJobPrio2``, and
    before ``QDate``.

:classad-attribute:`PostJobPrio1`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after ``PreJobPrio1``, after
    ``PreJobPrio1``, and after ``JobPrio``, but before ``PostJobPrio2``,
    and before ``QDate``.

:classad-attribute:`PostJobPrio2`
    An integer value representing a user's priority to affect of choice
    of jobs to run. A larger value gives higher priority. When not
    explicitly set for a job, 0 is used for comparison purposes. This
    attribute, when set, is considered after ``PreJobPrio1``, after
    ``PreJobPrio1``, after ``JobPrio``, and after ``PostJobPrio1``, but
    before ``QDate``.

:classad-attribute:`PreserveRelativeExecutable`
    When ``True``, the *condor_starter* will not prepend ``Iwd`` to
    ``Cmd``, when ``Cmd`` is a relative path name and
    ``TransferExecutable`` is ``False``. The default value is ``False``.
    This attribute is primarily of interest for users of
    ``USER_JOB_WRAPPER`` for the purpose of allowing an executable's
    location to be resolved by the user's path in the job wrapper.

:classad-attribute:`PreserveRelativePaths`
    When ``True``, entries in the file transfer lists that are relative
    paths will be transferred to the same relative path on the destination
    machine (instead of the basename).

:classad-attribute:`ProcId`
    Integer process identifier for this job. Within a cluster of many
    jobs, each job has the same ``ClusterId``, but will have a unique
    ``ProcId``. Within a cluster, assignment of a ``ProcId`` value will
    start with the value 0. The job (process) identifier described here
    is unrelated to operating system PIDs.

:classad-attribute:`ProportionalSetSizeKb`
    On Linux execute machines with kernel version more recent than
    2.6.27, this is the maximum observed proportional set size (PSS) in
    KiB, summed across all processes in the job. If the execute machine
    does not support monitoring of PSS or PSS has not yet been measured,
    this attribute will be undefined. PSS differs from ``ImageSize`` in
    how memory shared between processes is accounted. The PSS for one
    process is the sum of that process' memory pages divided by the
    number of processes sharing each of the pages. ``ImageSize`` is the
    same, except there is no division by the number of processes sharing
    the pages.

:classad-attribute:`QDate`
    Time at which the job was submitted to the job queue. Measured in
    the number of seconds since the epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`RecentBlockReadKbytes`.
    The integer number of KiB read from disk for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.

:classad-attribute:`RecentBlockReads`.
    The integer number of disk blocks read for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.

:classad-attribute:`RecentBlockWriteKbytes`.
    The integer number of KiB written to disk for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.

:classad-attribute:`RecentBlockWrites`.
    The integer number of blocks written to disk for this job over the
    previous time interval defined by configuration variable
    ``STATISTICS_WINDOW_SECONDS``.

:classad-attribute:`ReleaseReason`
    A string containing a human-readable message about why the job was
    released from hold.

:classad-attribute:`RemoteIwd`
    The path to the directory in which a job is to be executed on a
    remote machine.

:classad-attribute:`RemotePool`
    The name of the *condor_collector* of the pool in which a job is
    running via flocking. This attribute is not defined if the job is
    not running via flocking.

:classad-attribute:`RemoteSysCpu`
    The total number of seconds of system CPU time (the time spent at
    system calls) the job used on remote machines. This does not count
    time spent on run attempts that were evicted without a checkpoint.

:classad-attribute:`CumulativeRemoteSysCpu`
    The total number of seconds of system CPU time the job used on
    remote machines, summed over all execution attempts.

:classad-attribute:`RemoteUserCpu`
    The total number of seconds of user CPU time the job used on remote
    machines. This does not count time spent on run attempts that were
    evicted without a checkpoint. A job in the virtual machine universe
    will only report this attribute if run on a KVM hypervisor.

:classad-attribute:`CumulativeRemoteUserCpu`
    The total number of seconds of user CPU time the job used on remote
    machines, summed over all execution attempts.

:classad-attribute:`RemoteWallClockTime`
    Cumulative number of seconds the job has been allocated a machine.
    This also includes time spent in suspension (if any), so the total
    real time spent running is

    .. code-block:: condor-classad-expr

        RemoteWallClockTime - CumulativeSuspensionTime

    Note that this number does not get reset to zero when a job is
    forced to migrate from one machine to another. ``CommittedTime``, on
    the other hand, is just like ``RemoteWallClockTime`` except it does
    get reset to 0 whenever the job is evicted without a checkpoint.

:classad-attribute:`LastRemoteWallClockTime`
    Number of seconds the job was allocated a machine for its most recent completed
    execution.  This attribute is set after the job exits or is evicted.
    It will be undefined until the first execution attempt completes or is terminated.
    When a job has been allocated a machine and is still running, the value will be
    undefined or will be the value from the previous execution attempt rather than the
    current one.

:classad-attribute:`RemoveKillSig`
    Currently only for scheduler universe jobs, a string containing a
    name of a signal to be sent to the job if the job is removed.

:classad-attribute:`RequestCpus`
    The number of CPUs requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum number
    of CPUs that are needed in the created dynamic slot.

:classad-attribute:`RequestDisk`
    The amount of disk space in KiB requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum amount
    of disk space needed in the created dynamic slot.

:classad-attribute:`RequestGPUs`
    The number of GPUs requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum number
    of GPUs that are needed in the created dynamic slot.

:classad-attribute:`RequireGPUs`
    Constraint on the properites of GPUs requested for this job. If dynamic
    *condor_startd* provisioning is enabled, This constraint will be tested
    against the property attributes of the `AvailableGPUs` attribute of the
    partitionable slot when choosing which GPUs for the dynamic slot.

:classad-attribute:`RequestedChroot`
    A full path to the directory that the job requests the
    *condor_starter* use as an argument to chroot().

:index:`JOB_DEFAULT_REQUESTMEMORY`

:classad-attribute:`RequestMemory`
    The amount of memory space in MiB requested for this job. If dynamic
    *condor_startd* provisioning is enabled, it is the minimum amount
    of memory needed in the created dynamic slot. If not set by the job,
    its definition is specified by configuration variable
    ``JOB_DEFAULT_REQUESTMEMORY``

:index:`APPEND_REQUIREMENTES`

``Requirements``
    A classad expression evaluated by the *condor_negotiator*,
    *condor_schedd*, and *condor_startd* in the context of slot ad.  If
    true, this job is eligible to run on that slot.  If the job
    requirements does not mention the (startd) attribute ``OPSYS``,
    the schedd will append a clause to Requirements forcing the job to
    match the same ``OPSYS`` as the submit machine. :index:`OPSYS`
    The schedd appends a simliar clause to match the ``ARCH``. :index:`ARCH`
    The schedd parameter ``APPEND_REQUIREMENTS``, will, if set, append that
    value to every job's requirements expression.
    
:classad-attribute:`ResidentSetSize`
    Maximum observed physical memory in use by the job in KiB while
    running. 

:classad-attribute:`ScitokensFile`
    The path and filename containing a SciToken to use for a Condor-C job.

:classad-attribute:`ScratchDirFileCount`
    Number of files and directories in the jobs' Scratch directory.  The value is updated
    periodically while the job is running.

:classad-attribute:`ServerTime`
    This is the current time, in Unix epoch seconds.
    It is added by the *condor_schedd* to the job ads that it sends in
    reply to a query (e.g. sent to *condor_q*).
    Since it it not present in the job ad in the *condor_schedd*, it
    should not be used in any expressions that will be evaluated by the
    *condor_schedd*.

:classad-attribute:`StackSize`
    Utilized for Linux jobs only, the number of bytes allocated for
    stack space for this job. This number of bytes replaces the default
    allocation of 512 Mbytes.

:classad-attribute:`StageOutFinish`
    An attribute representing a Unix epoch time that is defined for a
    job that is spooled to a remote site using ``condor_submit -spool``
    or HTCondor-C and causes HTCondor to hold the output in the spool
    while the job waits in the queue in the ``Completed`` state. This
    attribute is defined when retrieval of the output finishes.

:classad-attribute:`StageOutStart`
    An attribute representing a Unix epoch time that is defined for a
    job that is spooled to a remote site using ``condor_submit -spool``
    or HTCondor-C and causes HTCondor to hold the output in the spool
    while the job waits in the queue in the ``Completed`` state. This
    attribute is defined when retrieval of the output begins.

:classad-attribute:`StreamErr`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, and ``TransferErr`` is ``True``, then
    standard error is streamed back to the submit machine, instead of
    doing the transfer (as a whole) after the job completes. If
    ``False``, then standard error is transferred back to the submit
    machine (as a whole) after the job completes. If ``TransferErr`` is
    ``False``, then this job attribute is ignored.

:classad-attribute:`StreamOut`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, and ``TransferOut`` is ``True``, then job
    output is streamed back to the submit machine, instead of doing the
    transfer (as a whole) after the job completes. If ``False``, then
    job output is transferred back to the submit machine (as a whole)
    after the job completes. If ``TransferOut`` is ``False``, then this
    job attribute is ignored.

:index:`GROUP_AUTOREGROUP` 

:classad-attribute:`SubmitterAutoregroup`
    A boolean attribute defined by the *condor_negotiator* when it
    makes a match. It will be ``True`` if the resource was claimed via
    negotiation when the configuration variable ``GROUP_AUTOREGROUP``
    was ``True``. It will be ``False`` otherwise.

:classad-attribute:`SubmitterGlobalJobId`
    When HTCondor-C submits a job to a remote *condor_schedd*, it sets
    this attribute in the remote job ad to match the ``GlobalJobId``
    attribute of the original, local job.

:classad-attribute:`SubmitterGroup`
    The accounting group name defined by the *condor_negotiator* when
    it makes a match.

:classad-attribute:`SubmitterNegotiatingGroup`
    The accounting group name under which the resource negotiated when
    it was claimed, as set by the *condor_negotiator*.

:classad-attribute:`SuccessCheckpointExitBySignal`
    Specifies if the ``executable`` exits with a signal after a successful
    self-checkpoint.

:classad-attribute:`SuccessCheckpointExitCode`
    Specifies the exit code, if any, with which the ``executable`` exits
    after a successful self-checkpoint.

:classad-attribute:`SuccessCheckpointExitSignal`
    Specifies the signal, if any, by which the ``executable`` exits after
    a successful self-checkpoint.

:classad-attribute:`SuccessPreExitBySignal`
    Specifies if a succesful pre command must exit with a signal.

:classad-attribute:`SuccessPreExitCode`
    Specifies the code with which the pre command must exit to be
    considered successful. Pre commands which are not successful cause
    the job to go on hold with ``ExitCode`` set to ``PreCmdExitCode``.
    The exit status of a pre command without one of
    ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` defined is
    ignored.

:classad-attribute:`SuccessPreExitSignal`
    Specifies the signal on which the pre command must exit be
    considered successful. Pre commands which are not successful cause
    the job to go on hold with ``ExitSignal`` set to
    ``PreCmdExitSignal``. The exit status of a pre command without one
    of ``SuccessPreExitCode`` or ``SuccessPreExitSignal`` defined is
    ignored.

:classad-attribute:`SuccessPostExitBySignal`
    Specifies if a succesful post command must exit with a signal.

:classad-attribute:`SuccessPostExitCode`
    Specifies the code with which the post command must exit to be
    considered successful. Post commands which are not successful cause
    the job to go on hold with ``ExitCode`` set to ``PostCmdExitCode``.
    The exit status of a post command without one of
    ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` defined is
    ignored.

:classad-attribute:`SuccessPostExitSignal`
    Specifies the signal on which the post command must exit be
    considered successful. Post commands which are not successful cause
    the job to go on hold with ``ExitSignal`` set to
    ``PostCmdExitSignal``. The exit status of a post command without one
    of ``SuccessPostExitCode`` or ``SuccessPostExitSignal`` defined is
    ignored.

:classad-attribute:`ToE`
    ToE stands for Ticket of Execution, and is itself a nested classad that
    describes how a job was terminated by the execute machine.
    See the :doc:`/users-manual/managing-a-job` section for full details.

:classad-attribute:`TotalSuspensions`
    A count of the number of times this job has been suspended during
    its lifetime. 
    
:classad-attribute:`TransferCheckpoint`
    A string attribute containing a comma separated list of directories
    and/or files that should be transferred from the execute machine to the
    submit machine's spool when the job successfully checkpoints.

:classad-attribute:`TransferContainer`
    A boolean expresion that controls whether the HTCondor should transfer the
    container image from the submit node to the worker node.

:classad-attribute:`TransferErr`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the error output from the job is
    transferred from the remote machine back to the submit machine. The
    name of the file after transfer is the file referred to by job
    attribute ``Err``. If ``False``, no transfer takes place (remote to
    submit machine), and the name of the file is the file referred to by
    job attribute ``Err``.

:classad-attribute:`TransferExecutable`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the job executable is transferred
    from the submit machine to the remote machine. The name of the file
    (on the submit machine) that is transferred is given by the job
    attribute ``Cmd``. If ``False``, no transfer takes place, and the
    name of the file used (on the remote machine) will be as given in
    the job attribute ``Cmd``.

:classad-attribute:`TransferIn`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the job input is transferred from the
    submit machine to the remote machine. The name of the file that is
    transferred is given by the job attribute ``In``. If ``False``, then
    the job's input is taken from a file on the remote machine
    (pre-staged), and the name of the file is given by the job attribute
    ``In``. 

:classad-attribute:`TransferInput`
    A string attribute containing a comma separated list of directories, files and/or URLs
    that should be transferred from the submit machine to the remote machine when
    input file transfer is enabled.

:classad-attribute:`TransferInFinished`
    When the job finished the most recent recent transfer of its input
    sandbox, measured in seconds from the epoch. (00:00:00 UTC Jan 1,
    1970). 

:classad-attribute:`TransferInQueued`
    If the job's most recent transfer of its input sandbox was queued,
    this attribute says when, measured in seconds from the epoch
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute:`TransferInStarted`
    : When the job actually started to transfer files, the most recent
    time it transferred its input sandbox, measured in seconds from the
    epoch. This will be later than ``TransferInQueued`` (if set).
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute:`TransferInputSizeMB`
    The total size in Mbytes of input files to be transferred for the
    job. Files transferred via file transfer plug-ins are not included.
    This attribute is automatically set by *condor_submit*; jobs
    submitted via other submission methods, such as SOAP, may not define
    this attribute. 

:classad-attribute:`TransferOut`
    An attribute utilized only for grid universe jobs. The default value
    is ``True``. If ``True``, then the output from the job is
    transferred from the remote machine back to the submit machine. The
    name of the file after transfer is the file referred to by job
    attribute ``Out``. If ``False``, no transfer takes place (remote to
    submit machine), and the name of the file is the file referred to by
    job attribute ``Out``.

:classad-attribute:`TransferOutput`
    A string attribute containing a comma separated list of files and/or URLs that should be transferred
    from the remote machine to the submit machine when output file transfer is enabled.

:classad-attribute:`TransferOutFinished`
    When the job finished the most recent recent transfer of its
    output sandbox, measured in seconds from the epoch. (00:00:00 UTC
    Jan 1, 1970).

:classad-attribute:`TransferOutQueued`
    If the job's most recent transfer of its output sandbox was
    queued, this attribute says when, measured in seconds from the epoch
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute:`TransferOutStarted`
    When the job actually started to transfer files, the most recent
    time it transferred its output sandbox, measured in seconds from the
    epoch. This will be later than ``TransferOutQueued`` (if set).
    (00:00:00 UTC Jan 1, 1970).

:classad-attribute:`TransferringInput`
    A boolean value that indicates whether the job is currently
    transferring input files. The value is ``Undefined`` if the job is
    not scheduled to run or has not yet attempted to start transferring
    input. When this value is ``True``, to see whether the transfer is
    active or queued, check ``TransferQueued``.

:classad-attribute:`TransferringOutput`
    A boolean value that indicates whether the job is currently
    transferring output files. The value is ``Undefined`` if the job is
    not scheduled to run or has not yet attempted to start transferring
    output. When this value is ``True``, to see whether the transfer is
    active or queued, check ``TransferQueued``.

:classad-attribute:`TransferPlugins`
    A string value containing a semicolon separated list of file transfer plugins
    to be supplied by the job. Each entry in this list will be of the form
    ``TAG1[,TAG2[,...]]=/path/to/plugin`` were `TAG` values are URL prefixes like `HTTP`,
    and ``/path/to/plugin`` is the path that the transfer plugin is to be transferred from.
    The files mentioned in this list will be transferred to the job sandbox before any file
    transfer plugins are invoked. A transfer plugin supplied in this will way will be used
    even if the execute node has a file transfer plugin installed that handles that URL prefix.

:classad-attribute:`TransferQueued`
    A boolean value that indicates whether the job is currently waiting
    to transfer files because of limits placed by
    ``MAX_CONCURRENT_DOWNLOADS`` :index:`MAX_CONCURRENT_DOWNLOADS`
    or ``MAX_CONCURRENT_UPLOADS``. :index:`MAX_CONCURRENT_UPLOADS`

:classad-attribute:`UserLog`
    The full path and file name on the submit machine of the log file of
    job events.

:classad-attribute:`WantContainer`
    A boolean that when true, tells HTCondor to run this job in container
    universe.  Note that container universe jobs are a "topping" above vanilla
    universe, and the JobUniverse attribute of container jobs will be 5 (vanilla)

:classad-attribute:`WantDocker`
    A boolean that when true, tells HTCondor to run this job in docker
    universe.  Note that docker universe jobs are a "topping" above vanilla
    universe, and the JobUniverse attribute of docker jobs will be 5 (vanilla)

:classad-attribute:`WantFTOnCheckpoint`
    A boolean that, when ``True``, specifies that when the ``executable``
    exits as described by ``SuccessCheckpointExitCode``,
    ``SuccessCheckpointExitBySignal``, and ``SuccessCheckpointExitSignal``,
    HTCondor should do (output) file transfer and immediately continue the
    job in the same sandbox by restarting ``executable`` with the same
    arguments as the first time.

:classad-attribute:`WantGracefulRemoval`
    A boolean expression that, when ``True``, specifies that a graceful
    shutdown of the job should be done when the job is removed or put on
    hold.

:classad-attribute:`WindowsMajorVersion`
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a major version number
    (currently 5 or 6) for a Windows operating system. This attribute
    only exists for jobs submitted from Windows machines.

:classad-attribute:`WindowsBuildNumber`
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a build number for a
    Windows operating system. This attribute only exists for jobs
    submitted from Windows machines.

:classad-attribute:`WindowsMinorVersion`
    An integer, extracted from the platform type of the machine upon
    which this job is submitted, representing a minor version number
    (currently 0, 1, or 2) for a Windows operating system. This
    attribute only exists for jobs submitted from Windows machines.

:classad-attribute:`X509UserProxy`
    The full path and file name of the file containing the X.509 user
    proxy.

:classad-attribute:`X509UserProxyEmail`
    For a job with an X.509 proxy credential, this is the email address
    extracted from the proxy.

:classad-attribute:`X509UserProxyExpiration`
    For a job that defines the submit description file command
    **x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>`,
    this is the time at which the indicated X.509 proxy credential will
    expire, measured in the number of seconds since the epoch (00:00:00
    UTC, Jan 1, 1970).

:classad-attribute:`X509UserProxyFirstFQAN`
    For a vanilla or grid universe job that defines the submit
    description file command
    **x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>`,
    this is the VOMS Fully Qualified Attribute Name (FQAN) of the
    primary role of the credential. A credential may have multiple roles
    defined, but by convention the one listed first is the primary role.

:classad-attribute:`X509UserProxyFQAN`
    For a vanilla or grid universe job that defines the submit
    description file command
    **x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>`,
    this is a serialized list of the DN and all FQAN. A comma is used as
    a separator, and any existing commas in the DN or FQAN are replaced
    with the string ``&comma;``. Likewise, any ampersands in the DN or
    FQAN are replaced with ``&amp;``.

:classad-attribute:`X509UserProxySubject`
    For a vanilla or grid universe job that defines the submit
    description file command
    **x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>`,
    this attribute contains the Distinguished Name (DN) of the
    credential used to submit the job.

:classad-attribute:`X509UserProxyVOName`
    For a vanilla or grid universe job that defines the submit
    description file command
    **x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>`,
    this is the name of the VOMS virtual organization (VO) that the
    user's credential is part of.


The following job ClassAd attributes are relevant only for **vm**
universe jobs.

:classad-attribute:`VM_MACAddr`
    The MAC address of the virtual machine's network interface, in the
    standard format of six groups of two hexadecimal digits separated by
    colons. This attribute is currently limited to apply only to Xen
    virtual machines.


The following job ClassAd attributes appear in the job ClassAd only for
the *condor_dagman* job submitted under DAGMan. They represent status
information for the DAG.

:classad-attribute:`DAG_InRecovery`
    The value 1 if the DAG is in recovery mode, and The value 0
    otherwise.
    
:classad-attribute:`DAG_NodesDone`
    The number of DAG nodes that have finished successfully. This means
    that the entire node has finished, not only an actual HTCondor job
    or jobs.
    
:classad-attribute:`DAG_NodesFailed`
    The number of DAG nodes that have failed. This value includes all
    retries, if there are any.

:classad-attribute:`DAG_NodesPostrun`
    The number of DAG nodes for which a POST script is running or has
    been deferred because of a POST script throttle setting.

:classad-attribute:`DAG_NodesPrerun`
    The number of DAG nodes for which a PRE script is running or has
    been deferred because of a PRE script throttle setting.

:classad-attribute:`DAG_NodesQueued`
    The number of DAG nodes for which the actual HTCondor job or jobs
    are queued. The queued jobs may be in any state.

:classad-attribute:`DAG_NodesReady`
    The number of DAG nodes that are ready to run, but which have not
    yet started running.

:classad-attribute:`DAG_NodesTotal`
    The total number of nodes in the DAG, including the FINAL node, if
    there is a FINAL node.

:classad-attribute:`DAG_NodesUnready`
    The number of DAG nodes that are not ready to run. This is a node in
    which one or more of the parent nodes has not yet finished.

:classad-attribute:`DAG_Status`
    The overall status of the DAG, with the same values as the macro
    ``$DAG_STATUS`` used in DAGMan FINAL nodes.

    +--------------------------------------+--------------------------------------+
    | 0                                    | OK                                   |
    +--------------------------------------+--------------------------------------+
    | 3                                    | the DAG has been aborted by an       |
    |                                      | ABORT-DAG-ON specification           |
    +--------------------------------------+--------------------------------------+


The following job ClassAd attributes do not appear in the job ClassAd as
kept by the *condor_schedd* daemon. They appear in the job ClassAd
written to the job's execute directory while the job is running.

:classad-attribute:`CpusProvisioned`
    The number of Cpus allocated to the job. With statically-allocated
    slots, it is the number of Cpus allocated to the slot. With
    dynamically-allocated slots, it is based upon the job attribute
    ``RequestCpus``, but may be larger due to the minimum given to a
    dynamic slot.

:classad-attribute:`CpusUsage`
    CpusUsage (Note the plural `Cpus`) is a floating point value that 
    represents the number of cpu cores fully used over the lifetime of
    the job.  A cpu-bound, single-threaded job will have a CpusUsage of 1.0.
    A job that is blocked on I/O for half of its life and is cpu bound
    for the other have will have a CpusUsage of 0.5.  A job that uses two
    cores fully will have a CpusUsage of 2.0.  Jobs with unexpectedly low 
    CpusUsage may be showing lowered throughput due to blocking on network or disk.

:classad-attribute:`DiskProvisioned`
    The amount of disk space in KiB allocated to the job. With
    statically-allocated slots, it is the amount of disk space allocated
    to the slot. With dynamically-allocated slots, it is based upon the
    job attribute ``RequestDisk``, but may be larger due to the minimum
    given to a dynamic slot.

:classad-attribute:`MemoryProvisioned`
    The amount of memory in MiB allocated to the job. With
    statically-allocated slots, it is the amount of memory space
    allocated to the slot. With dynamically-allocated slots, it is based
    upon the job attribute ``RequestMemory``, but may be larger due to
    the minimum given to a dynamic slot.

:classad-attribute:`<Name>Provisioned`
    The amount of the custom resource identified by ``<Name>`` allocated
    to the job. For jobs using GPUs, ``<Name>`` will be ``GPUs``. With
    statically-allocated slots, it is the amount of the resource
    allocated to the slot. With dynamically-allocated slots, it is based
    upon the job attribute ``Request<Name>``, but may be larger due to
    the minimum given to a dynamic slot.
