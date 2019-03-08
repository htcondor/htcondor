      

Machine ClassAd Attributes
==========================

:index:`ClassAd<single: ClassAd; machine attributes>`
:index:`AcceptedWhileDraining<single: AcceptedWhileDraining>`

 ``AcceptedWhileDraining``:
    Boolean which indicates if the slot accepted its current job while
    the machine was draining.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Activity>`
 ``Activity``:
    String which describes HTCondor job activity on the machine. Can
    have one of the following values:

     ``"Idle"``:
        There is no job activity
     ``"Busy"``:
        A job is busy running
     ``"Suspended"``:
        A job is currently suspended
     ``"Vacating"``:
        A job is currently checkpointing
     ``"Killing"``:
        A job is currently being killed
     ``"Benchmarking"``:
        The startd is running benchmarks
     ``"Retiring"``:
        Waiting for a job to finish or for the maximum retirement time
        to expire

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Arch>`
 ``Arch``:
    String with the architecture of the machine. Currently supported
    architectures have the following string definitions:

     ``"INTEL"``:
        Intel x86 CPU (Pentium, Xeon, etc).
     ``"X86_64"``:
        AMD/Intel 64-bit X86

    These strings show definitions for architectures no longer
    supported:

     ``"IA64"``:
        Intel Itanium
     ``"SUN4u"``:
        Sun UltraSparc CPU
     ``"SUN4x"``:
        A Sun Sparc CPU other than an UltraSparc, i.e. sun4m or sun4c
        CPU found in older Sparc workstations such as the Sparc 10,
        Sparc 20, IPC, IPX, etc.
     ``"PPC"``:
        32-bit PowerPC
     ``"PPC64"``:
        64-bit PowerPC

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; CanHibernate>`

 ``CanHibernate``:
    The *condor\_startd* has the capability to shut down or hibernate a
    machine when certain configurable criteria are met. However, before
    the *condor\_startd* can shut down a machine, the hardware itself
    must support hibernation, as must the operating system. When the
    *condor\_startd* initializes, it checks for this support. If the
    machine has the ability to hibernate, then this boolean ClassAd
    attribute will be ``True``. By default, it is ``False``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; CheckpointPlatform>`
 ``CheckpointPlatform``:
    A string which opaquely encodes various aspects about a machine’s
    operating system, hardware, and kernel attributes. It is used to
    identify systems where previously taken checkpoints for the standard
    universe may resume.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; ClockDay>`
 ``ClockDay``:
    The day of the week, where 0 = Sunday, 1 = Monday, …, and 6 =
    Saturday. :index:`ClassAd machine attribute<single: ClassAd machine attribute; ClockMin>`
 ``ClockMin``:
    The number of minutes passed since midnight.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; CondorLoadAvg>`
 ``CondorLoadAvg``:
    The load average contributed by HTCondor, either from remote jobs or
    running benchmarks.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; CondorVersion>`
 ``CondorVersion``:
    A string containing the HTCondor version number for the
    *condor\_startd* daemon, the release date, and the build
    identification number.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; ConsoleIdle>`
 ``ConsoleIdle``:
    The number of seconds since activity on the system console keyboard
    or console mouse has last been detected. The value can be modified
    with ``SLOTS_CONNECTED_TO_CONSOLE``
    :index:`SLOTS_CONNECTED_TO_CONSOLE<single: SLOTS_CONNECTED_TO_CONSOLE>` as defined at
     `Configuration
    Macros <../admin-manual/configuration-macros.html>`__.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Cpus>`
 ``Cpus``:
    The number of CPUs (cores) in this slot. It is 1 for a single CPU
    slot, 2 for a dual CPU slot, etc. For a partitionable slot, it is
    the remaining number of CPUs in the partitionable slot.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; CpuFamily>`
 ``CpuFamily``:
    On Linux machines, the Cpu family, as defined in the /proc/cpuinfo
    file. :index:`ClassAd machine attribute<single: ClassAd machine attribute; CpuModel>`
 ``CpuModel``:
    On Linux machines, the Cpu model number, as defined in the
    /proc/cpuinfo file.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; CpuCacheSize>`
 ``CpuCacheSize``:
    On Linux machines, the size of the L3 cache, in kbytes, as defined
    in the /proc/cpuinfo file.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; CurrentRank>`
 ``CurrentRank``:
    A float which represents this machine owner’s affinity for running
    the HTCondor job which it is currently hosting. If not currently
    hosting an HTCondor job, ``CurrentRank`` is 0.0. When a machine is
    claimed, the attribute’s value is computed by evaluating the
    machine’s ``Rank`` expression with respect to the current job’s
    ClassAd. :index:`ClassAd machine attribute<single: ClassAd machine attribute; DetectedCpus>`
 ``DetectedCpus``:
    Set by the value of configuration variable ``DETECTED_CORES``
    :index:`DETECTED_CORES<single: DETECTED_CORES>`.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; DetectedMemory>`
 ``DetectedMemory``:
    Set by the value of configuration variable ``DETECTED_MEMORY``
    :index:`DETECTED_MEMORY<single: DETECTED_MEMORY>`. Specified in MiB.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Disk>`
 ``Disk``:
    The amount of disk space on this machine available for the job in
    KiB (for example, 23000 = 23 MiB). Specifically, this is the amount
    of disk space available in the directory specified in the HTCondor
    configuration files by the ``EXECUTE`` :index:`EXECUTE<single: EXECUTE>` macro,
    minus any space reserved with the ``RESERVED_DISK``
    :index:`RESERVED_DISK<single: RESERVED_DISK>` macro. For static slots, this value
    will be the same as machine ClassAd attribute ``TotalSlotDisk``. For
    partitionable slots, this value will be the quantity of disk space
    remaining in the partitionable slot.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Draining>`
 ``Draining``:
    This attribute is ``True`` when the slot is draining and undefined
    if not.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; DrainingRequestId>`
 ``DrainingRequestId``:
    This attribute contains a string that is the request id of the
    draining request that put this slot in a draining state. It is
    undefined if the slot is not draining.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; DotNetVersions>`
 ``DotNetVersions``:
    The .NET framework versions currently installed on this computer.
    Default format is a comma delimited list. Current definitions:

     ``"1.1"``:
        for .Net Framework 1.1
     ``"2.0"``:
        for .Net Framework 2.0
     ``"3.0"``:
        for .Net Framework 3.0
     ``"3.5"``:
        for .Net Framework 3.5
     ``"4.0Client"``:
        for .Net Framework 4.0 Client install
     ``"4.0Full"``:
        for .Net Framework 4.0 Full install

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; DynamicSlot>`
 ``DynamicSlot``:
    For SMP machines that allow dynamic partitioning of a slot, this
    boolean value identifies that this dynamic slot may be partitioned.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; EnteredCurrentActivity>`
 ``EnteredCurrentActivity``:
    Time at which the machine entered the current Activity (see
    ``Activity`` entry above). On all platforms (including NT), this is
    measured in the number of integer seconds since the Unix epoch
    (00:00:00 UTC, Jan 1, 1970).
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; ExpectedMachineGracefulDrainingBadput>`
 ``ExpectedMachineGracefulDrainingBadput``:
    The job run time in cpu-seconds that would be lost if graceful
    draining were initiated at the time this ClassAd was published. This
    calculation assumes that jobs will run for the full retirement time
    and then be evicted without saving a checkpoint.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; ExpectedMachineGracefulDrainingCompletion>`
 ``ExpectedMachineGracefulDrainingCompletion``:
    The estimated time at which graceful draining of the machine could
    complete if it were initiated at the time this ClassAd was published
    and there are no active claims. This is measured in the number of
    integer seconds since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    This value is computed with the assumption that the machine policy
    will not suspend jobs during draining while the machine is waiting
    for the job to use up its retirement time. If suspension happens,
    the upper bound on how long draining could take is unlimited. To
    avoid suspension during draining, the ``SUSPEND`` and ``CONTINUE``
    expressions could be configured to pay attention to the ``Draining``
    attribute.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; ExpectedMachineQuickDrainingBadput>`
 ``ExpectedMachineGracefulQuickBadput``:
    The job run time in cpu-seconds that would be lost if quick or fast
    draining were initiated at the time this ClassAd was published. This
    calculation assumes that all evicted jobs will not save a
    checkpoint.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; ExpectedMachineQuickDrainingCompletion>`
 ``ExpectedMachineQuickDrainingCompletion``:
    Time at which quick or fast draining of the machine could complete
    if it were initiated at the time this ClassAd was published and
    there are no active claims. This is measured in the number of
    integer seconds since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; FileSystemDomain>`
 ``FileSystemDomain``:
    A domain name configured by the HTCondor administrator which
    describes a cluster of machines which all access the same,
    uniformly-mounted, networked file systems usually via NFS or AFS.
    This is useful for Vanilla universe jobs which require remote file
    access. :index:`ClassAd machine attribute<single: ClassAd machine attribute; HasDocker>`
 ``HasDocker``:
    A boolean value set to ``True`` if the machine is capable of
    executing docker universe jobs.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; HasEncryptExecuteDirectory>`
 ``HasEncryptExecuteDirectory``:
    A boolean value set to ``True`` if the machine is capable of
    encrypting execute directories.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; HasFileTransfer>`
 ``HasFileTransfer``:
    A boolean value that when ``True`` identifies that the machine can
    use the file transfer mechanism.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; HasFileTransferPluginMethods>`
 ``HasFileTransferPluginMethods``:
    A string of comma-separated file transfer protocols that the machine
    can support. The value can be modified with ``FILETRANSFER_PLUGINS``
    :index:`FILETRANSFER_PLUGINS<single: FILETRANSFER_PLUGINS>` as defined at  `Configuration
    Macros <../admin-manual/configuration-macros.html>`__.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Has_sse4_1>`
 ``Has_sse4_1``:
    A boolean value set to ``True`` if the machine being advertised
    supports the SSE 4.1 instructions, and ``Undefined`` otherwise.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Has_sse4_2>`
 ``Has_sse4_2``:
    A boolean value set to ``True`` if the machine being advertised
    supports the SSE 4.2 instructions, and ``Undefined`` otherwise.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; has_ssse3>`
 ``has_ssse3``:
    A boolean value set to ``True`` if the machine being advertised
    supports the SSSE 3 instructions, and ``Undefined`` otherwise.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; has_avx>`
 ``has_avx``:
    A boolean value set to ``True`` if the machine being advertised
    supports the avx instructions, and ``Undefined`` otherwise.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; HasSingularity>`
 ``HasSingularity``:
    A boolean value set to ``True`` if the machine being advertised
    supports running jobs within Singularity containers.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; HasVM>`
 ``HasVM``:
    If the configuration triggers the detection of virtual machine
    software, a boolean value reporting the success thereof; otherwise
    undefined. May also become ``False`` if HTCondor determines that it
    can’t start a VM (even if the appropriate software is detected).
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; IsWakeAble>`
 ``IsWakeAble``:
    A boolean value that when ``True`` identifies that the machine has
    the capability to be woken into a fully powered and running state by
    receiving a Wake On LAN (WOL) packet. This ability is a function of
    the operating system, the network adapter in the machine (notably,
    wireless network adapters usually do not have this function), and
    BIOS settings. When the *condor\_startd* initializes, it tries to
    detect if the operating system and network adapter both support
    waking from hibernation by receipt of a WOL packet. The default
    value is ``False``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; IsWakeEnabled>`
 ``IsWakeEnabled``:
    If the hardware and software have the capacity to be woken into a
    fully powered and running state by receiving a Wake On LAN (WOL)
    packet, this feature can still be disabled via the BIOS or software.
    If BIOS or the operating system have disabled this feature, the
    *condor\_startd* sets this boolean attribute to ``False``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobBusyTimeAvg>`
 ``JobBusyTimeAvg``:
    The Average lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor\_starter* that
    has exited. This attribute will be undefined until the first time a
    *condor\_starter* has exited.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobBusyTimeCount>`
 ``JobBusyTimeCount``:
    The total number of of jobs used to calulate the ``JobBusyTimeAvg``
    attribute. This is also the the total number times a
    *condor\_starter* has exited.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobBusyTimeMax>`
 ``JobBusyTimeMax``:
    The Maximum lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor\_starter*\ s
    that has exited. This attribute will be undefined until the first
    time a *condor\_starter* has exited.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobBusyTimeMin>`
 ``JobBusyTimeMin``:
    The Minimum lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor\_starter* that
    has exited. This attribute will be undefined until the first time a
    *condor\_starter* has exited.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobBusyTimeAvg>`
 ``RecentJobBusyTimeAvg``:
    The Average lifetime of all jobs that have exited in the last 20
    minutes, including transfer time. This is determined by measuring
    the lifetime of each *condor\_starter* that has exited in the last
    20 minutes. This attribute will be undefined if no *condor\_starter*
    has exited in the last 20 minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobBusyTimeCount>`
 ``RecentJobBusyTimeCount``:
    The total number of jobs used to calulate the
    ``RecentJobBusyTimeAvg`` attribute. This is also the the total
    number times a *condor\_starter* has exited in the last 20 minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobBusyTimeMax>`
 ``RecentJobBusyTimeMax``:
    The Maximum lifetime of all jobs that have exited in the last 20
    minutes, including transfer time. This is determined by measuring
    the lifetime of each *condor\_starter*\ s that has exited in the
    last 20 minutes. This attribute will be undefined if no
    *condor\_starter* has exited in the last 20 minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobBusyTimeMin>`
 ``RecentJobBusyTimeMin``:
    The Minimum lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor\_starter* that
    has exited. This attribute will be undefined if no *condor\_starter*
    has exited in the last 20 minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobDurationAvg>`
 ``JobDurationAvg``:
    The Average lifetime time of all jobs, not including time spent
    transferring files. This attribute will be undefined until the first
    time a job exits. Jobs that never start (because they fail to
    transfer input, for instance) will not be included in the average.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobDurationCount>`
 ``JobDurationCount``:
    The total number of of jobs used to calulate the ``JobDurationAvg``
    attribute. This is also the the total number times a job has exited.
    Jobs that never start (because input transfer fails, for instance)
    are not included in the count.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobDurationMax>`
 ``JobDurationMax``:
    The lifetime of the longest lived job that has exited. This
    attribute will be undefined until the first time a job exits.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobDurationMin>`
 ``JobDurationMin``:
    The lifetime of the shortest lived job that has exited. This
    attribute will be undefined until the first time a job exits.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobDurationAvg>`
 ``RecentJobDurationAvg``:
    The Average lifetime time of all jobs, not including time spent
    transferring files, that have exited in the last 20 minutes. This
    attribute will be undefined if no job has exited in the last 20
    minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobDurationCount>`
 ``RecentJobDurationCount``:
    The total number of jobs used to calulate the
    ``RecentJobDurationAvg`` attribute. This is the total number of jobs
    that began execution and have exited in the last 20 minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobDurationMax>`
 ``RecentJobDurationMax``:
    The lifetime of the longest lived job that has exited in the last 20
    minutes. This attribute will be undefined if no job has exited in
    the last 20 minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobDurationMin>`
 ``RecentJobDurationMin``:
    The lifetime of the shortest lived job that has exited in the last
    20 minutes. This attribute will be undefined if no job has exited in
    the last 20 minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobPreemptions>`
 ``JobPreemptions``:
    The total number of times a running job has been preempted on this
    machine.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobRankPreemptions>`
 ``JobRankPreemptions``:
    The total number of times a running job has been preempted on this
    machine due to the machine’s rank of jobs since the *condor\_startd*
    started running.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobStarts>`
 ``JobStarts``:
    The total number of jobs which have been started on this machine
    since the *condor\_startd* started running.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobUserPrioPreemptions>`
 ``JobUserPrioPreemptions``:
    The total number of times a running job has been preempted on this
    machine based on a fair share allocation of the pool since the
    *condor\_startd* started running.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; JobVM_VCPUS>`
 ``JobVM_VCPUS``:
    An attribute defined if a vm universe job is running on this slot.
    Defined by the number of virtualized CPUs in the virtual machine.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; KeyboardIdle>`
 ``KeyboardIdle``:
    The number of seconds since activity on any keyboard or mouse
    associated with this machine has last been detected. Unlike
    ``ConsoleIdle``, ``KeyboardIdle`` also takes activity on
    pseudo-terminals into account. Pseudo-terminals have virtual
    keyboard activity from telnet and rlogin sessions. Note that
    ``KeyboardIdle`` will always be equal to or less than
    ``ConsoleIdle``. The value can be modified with
    ``SLOTS_CONNECTED_TO_KEYBOARD``
    :index:`SLOTS_CONNECTED_TO_KEYBOARD<single: SLOTS_CONNECTED_TO_KEYBOARD>` as defined at
     `Configuration
    Macros <../admin-manual/configuration-macros.html>`__.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; KFlops>`
 ``KFlops``:
    Relative floating point performance as determined via a Linpack
    benchmark.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; LastDrainStartTime>`
 ``LastDrainStartTime``:
    Time when draining of this *condor\_startd* was last initiated (e.g.
    due to *condor\_defrag* or *condor\_drain*).
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; LastHeardFrom>`
 ``LastHeardFrom``:
    Time when the HTCondor central manager last received a status update
    from this machine. Expressed as the number of integer seconds since
    the Unix epoch (00:00:00 UTC, Jan 1, 1970). Note: This attribute is
    only inserted by the central manager once it receives the ClassAd.
    It is not present in the *condor\_startd* copy of the ClassAd.
    Therefore, you could not use this attribute in defining
    *condor\_startd* expressions (and you would not want to).
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; LoadAvg>`
 ``LoadAvg``:
    A floating point number representing the current load average.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Machine>`
 ``Machine``:
    A string with the machine’s fully qualified host name.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MachineMaxVacateTime>`
 ``MachineMaxVacateTime``:
    An integer expression that specifies the time in seconds the machine
    will allow the job to gracefully shut down.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MaxJobRetirementTime>`
 ``MaxJobRetirementTime``:
    When the *condor\_startd* wants to kick the job off, a job which has
    run for less than this number of seconds will not be hard-killed.
    The *condor\_startd* will wait for the job to finish or to exceed
    this amount of time, whichever comes sooner. If the job vacating
    policy grants the job X seconds of vacating time, a preempted job
    will be soft-killed X seconds before the end of its retirement time,
    so that hard-killing of the job will not happen until the end of the
    retirement time if the job does not finish shutting down before
    then. This is an expression evaluated in the context of the job
    ClassAd, so it may refer to job attributes as well as machine
    attributes. :index:`ClassAd machine attribute<single: ClassAd machine attribute; Memory>`
 ``Memory``:
    The amount of RAM in MiB in this slot. For static slots, this value
    will be the same as in ``TotalSlotMemory``. For a partitionable
    slot, this value will be the quantity remaining in the partitionable
    slot. :index:`ClassAd machine attribute<single: ClassAd machine attribute; Mips>`
 ``Mips``:
    Relative integer performance as determined via a Dhrystone
    benchmark.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MonitorSelfAge>`
 ``MonitorSelfAge``:
    The number of seconds that this daemon has been running.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MonitorSelfCPUUsage>`
 ``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MonitorSelfImageSize>`
 ``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in KiB.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MonitorSelfRegisteredSocketCount>`
 ``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MonitorSelfResidentSetSize>`
 ``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in KiB.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MonitorSelfSecuritySessions>`
 ``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MonitorSelfTime>`
 ``MonitorSelfTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MyAddress>`
 ``MyAddress``:
    String with the IP and port address of the *condor\_startd* daemon
    which is publishing this machine ClassAd. When using CCB,
    *condor\_shared\_port*, and/or an additional private network
    interface, that information will be included here as well.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; MyType>`
 ``MyType``:
    The ClassAd type; always set to the literal string ``"Machine"``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Name>`
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form “slot#@full.hostname”, for example,
    “slot1@vulture.cs.wisc.edu”, which signifies slot number 1 from
    vulture.cs.wisc.edu.
    ` <index://Offline<name>;ClassAd machine attribute>`__
 ``Offline<name>``:
    A string that lists specific instances of a user-defined machine
    resource, identified by ``name``. Each instance is currently
    unavailable for purposes of match making.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OfflineUniverses>`
 ``OfflineUniverses``:
    A ClassAd list that specifies which job universes are presently
    offline, both as strings and as the corresponding job universe
    number. Could be used the the startd to refuse to start jobs in
    offline universes:

    ::

        START = OfflineUniverses is undefined || (! member( JobUniverse, OfflineUniverses ))

    May currently only contain ``"VM"`` and ``13``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSys>`

 ``OpSys``:
    String describing the operating system running on this machine.
    Currently supported operating systems have the following string
    definitions:

     ``"LINUX"``:
        for LINUX 2.0.x, LINUX 2.2.x, LINUX 2.4.x, LINUX 2.6.x, or LINUX
        3.10.0 kernel systems, as well as Scientific Linux, Ubuntu
        versions 14.04, and Debian 7.0 (wheezy) and 8.0 (jessie)
     ``"OSX"``:
        for Darwin
     ``"FREEBSD7"``:
        for FreeBSD 7
     ``"FREEBSD8"``:
        for FreeBSD 8
     ``"WINDOWS"``:
        for all versions of Windows
     ``"SOLARIS5.10"``:
        for Solaris 2.10 or 5.10
     ``"SOLARIS5.11"``:
        for Solaris 2.11 or 5.11

    These strings show definitions for operating systems no longer
    supported:

     ``"SOLARIS28"``:
        for Solaris 2.8 or 5.8
     ``"SOLARIS29"``:
        for Solaris 2.9 or 5.9

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSysAndVer>`

 ``OpSysAndVer``:
    A string indicating an operating system and a version number.

    For Linux operating systems, it is the value of the ``OpSysName``
    attribute concatenated with the string version of the
    ``OpSysMajorVer`` attribute:

     ``"RedHat5"``:
        for RedHat Linux version 5
     ``"RedHat6"``:
        for RedHat Linux version 6
     ``"RedHat7"``:
        for RedHat Linux version 7
     ``"Fedora16"``:
        for Fedora Linux version 16
     ``"Debian6"``:
        for Debian Linux version 6
     ``"Debian7"``:
        for Debian Linux version 7
     ``"Debian8"``:
        for Debian Linux version 8
     ``"Debian9"``:
        for Debian Linux version 9
     ``"Ubuntu14"``:
        for Ubuntu 14.04
     ``"SL5"``:
        for Scientific Linux version 5
     ``"SL6"``:
        for Scientific Linux version 6
     ``"SLFermi5"``:
        for Fermi’s Scientific Linux version 5
     ``"SLFermi6"``:
        for Fermi’s Scientific Linux version 6
     ``"SLCern5"``:
        for CERN’s Scientific Linux version 5
     ``"SLCern6"``:
        for CERN’s Scientific Linux version 6

    For MacOS operating systems, it is the value of the
    ``OpSysShortName`` attribute concatenated with the string version of
    the ``OpSysVer`` attribute:

     ``"MacOSX605"``:
        for MacOS version 10.6.5 (Snow Leopard)
     ``"MacOSX703"``:
        for MacOS version 10.7.3 (Lion)

    For BSD operating systems, it is the value of the ``OpSysName``
    attribute concatenated with the string version of the
    ``OpSysMajorVer`` attribute:

     ``"FREEBSD7"``:
        for FreeBSD version 7
     ``"FREEBSD8"``:
        for FreeBSD version 8

    For Solaris Unix operating systems, it is the same value as the
    ``OpSys`` attribute:

     ``"SOLARIS5.10"``:
        for Solaris 2.10 or 5.10
     ``"SOLARIS5.11"``:
        for Solaris 2.11 or 5.11

    For Windows operating systems, it is the value of the ``OpSys``
    attribute concatenated with the string version of the
    ``OpSysMajorVer`` attribute:

     ``"WINDOWS500"``:
        for Windows 2000
     ``"WINDOWS501"``:
        for Windows XP
     ``"WINDOWS502"``:
        for Windows Server 2003
     ``"WINDOWS600"``:
        for Windows Vista
     ``"WINDOWS601"``:
        for Windows 7

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSysLegacy>`

 ``OpSysLegacy``:
    A string that holds the long-standing values for the ``OpSys``
    attribute. Currently supported operating systems have the following
    string definitions:

     ``"LINUX"``:
        for LINUX 2.0.x, LINUX 2.2.x, LINUX 2.4.x, LINUX 2.6.x, or LINUX
        3.10.0 kernel systems, as well as Scientific Linux, Ubuntu
        versions 14.04, and Debian 7 and 8
     ``"OSX"``:
        for Darwin
     ``"FREEBSD7"``:
        for FreeBSD version 7
     ``"FREEBSD8"``:
        for FreeBSD version 8
     ``"SOLARIS5.10"``:
        for Solaris 2.10 or 5.10
     ``"SOLARIS5.11"``:
        for Solaris 2.11 or 5.11
     ``"WINDOWS"``:
        for all versions of Windows

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSysLongName>`
 ``OpSysLongName``:
    A string giving a full description of the operating system. For
    Linux platforms, this is generally the string taken from
    ``/etc/hosts``, with extra characters stripped off Debian versions.

     ``"Red Hat Enterprise Linux Server release 5.7 (Tikanga)"``:
        for RedHat Linux version 5
     ``"Red Hat Enterprise Linux Server release 6.2 (Santiago)"``:
        for RedHat Linux version 6
     ``"Red Hat Enterprise Linux Server release 7.0 (Maipo)"``:
        for RedHat Linux version 7.0
     ``"Ubuntu 14.04.1 LTS"``:
        for Ubuntu 14.04 point release 1
     ``"Debian GNU/Linux 7"``:
        for Debian 7.0 (wheezy)
     ``"Debian GNU/Linux 8"``:
        for Debian 8.0 (jessie)
     ``"Fedora release 16 (Verne)"``:
        for Fedora Linux version 16
     ``"MacOSX 6.5"``:
        for MacOS version 10.6.5 (Snow Leopard)
     ``"MacOSX 7.3"``:
        for MacOS version 10.7.3 (Lion)
     ``"FreeBSD8.2-RELEASE-p3"``:
        for FreeBSD version 8
     ``"SOLARIS5.10"``:
        for Solaris 2.10 or 5.10
     ``"SOLARIS5.11"``:
        for Solaris 2.11 or 5.11
     ``"Windows XP SP3"``:
        for Windows XP
     ``"Windows 7 SP2"``:
        for Windows 7

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSysMajorVer>`
 ``OpSysMajorVer``:
    An integer value representing the major version of the operating
    system.

     ``5``:
        for RedHat Linux version 5 and derived platforms such as
        Scientific Linux
     ``6``:
        for RedHat Linux version 6 and derived platforms such as
        Scientific Linux
     ``7``:
        for RedHat Linux version 7
     ``14``:
        for Ubuntu 14.04
     ``7``:
        for Debian 7
     ``8``:
        for Debian 8
     ``16``:
        for Fedora Linux version 16
     ``6``:
        for MacOS version 10.6.5 (Snow Leopard)
     ``7``:
        for MacOS version 10.7.3 (Lion)
     ``7``:
        for FreeBSD version 7
     ``8``:
        for FreeBSD version 8
     ``5``:
        for Solaris 2.10, 5.10, 2.11, or 5.11
     ``501``:
        for Windows XP
     ``600``:
        for Windows Vista
     ``601``:
        for Windows 7

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSysName>`
 ``OpSysName``:
    A string containing a terse description of the operating system.

     ``"RedHat"``:
        for RedHat Linux version 6 and 7
     ``"Fedora"``:
        for Fedora Linux version 16
     ``"Ubuntu"``:
        for Ubuntu versions 14.04
     ``"Debian"``:
        for Debian versions 7 and 8
     ``"SnowLeopard"``:
        for MacOS version 10.6.5 (Snow Leopard)
     ``"Lion"``:
        for MacOS version 10.7.3 (Lion)
     ``"FREEBSD"``:
        for FreeBSD version 7 or 8
     ``"SOLARIS5.10"``:
        for Solaris 2.10 or 5.10
     ``"SOLARIS5.11"``:
        for Solaris 2.11 or 5.11
     ``"WindowsXP"``:
        for Windows XP
     ``"WindowsVista"``:
        for Windows Vista
     ``"Windows7"``:
        for Windows 7
     ``"SL"``:
        for Scientific Linux
     ``"SLFermi"``:
        for Fermi’s Scientific Linux
     ``"SLCern"``:
        for CERN’s Scientific Linux

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSysShortName>`
 ``OpSysShortName``:
    A string containing a short name for the operating system.

     ``"RedHat"``:
        for RedHat Linux version 5, 6 or 7
     ``"Fedora"``:
        for Fedora Linux version 16
     ``"Debian"``:
        for Debian Linux version 6 or 7 or 8
     ``"Ubuntu"``:
        for Ubuntu versions 14.04
     ``"MacOSX"``:
        for MacOS version 10.6.5 (Snow Leopard) or for MacOS version
        10.7.3 (Lion)
     ``"FreeBSD"``:
        for FreeBSD version 7 or 8
     ``"SOLARIS5.10"``:
        for Solaris 2.10 or 5.10
     ``"SOLARIS5.11"``:
        for Solaris 2.11 or 5.11
     ``"XP"``:
        for Windows XP
     ``"Vista"``:
        for Windows Vista
     ``"7"``:
        for Windows 7
     ``"SL"``:
        for Scientific Linux
     ``"SLFermi"``:
        for Fermi’s Scientific Linux
     ``"SLCern"``:
        for CERN’s Scientific Linux

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; OpSysVer>`
 ``OpSysVer``:
    An integer value representing the operating system version number.

     ``700``:
        for RedHat Linux version 7.0
     ``602``:
        for RedHat Linux version 6.2
     ``1600``:
        for Fedora Linux version 16.0
     ``1404``:
        for Ubuntu 14.04
     ``700``:
        for Debian 7.0
     ``800``:
        for Debian 8.0
     ``704``:
        for FreeBSD version 7.4
     ``802``:
        for FreeBSD version 8.2
     ``605``:
        for MacOS version 10.6.5 (Snow Leopard)
     ``703``:
        for MacOS version 10.7.3 (Lion)
     ``500``:
        for Windows 2000
     ``501``:
        for Windows XP
     ``502``:
        for Windows Server 2003
     ``600``:
        for Windows Vista or Windows Server 2008
     ``601``:
        for Windows 7 or Windows Server 2008

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; PartitionableSlot>`
 ``PartitionableSlot``:
    For SMP machines, a boolean value identifying that this slot may be
    partitioned.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobPreemptions>`
 ``RecentJobPreemptions``:
    The total number of jobs which have been preempted from this machine
    in the last twenty minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobRankPreemptions>`
 ``RecentJobRankPreemptions``:
    The total number of times a running job has been preempted on this
    machine due to the machine’s rank of jobs in the last twenty
    minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobStarts>`
 ``RecentJobStarts``:
    The total number of jobs which have been started on this machine in
    the last twenty minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RecentJobUserPrioPreemptions>`
 ``RecentJobUserPrio``:
    The total number of times a running job has been preempted on this
    machine based on a fair share allocation of the pool in the last
    twenty minutes.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; Requirements>`
 ``Requirements``:
    A boolean, which when evaluated within the context of the machine
    ClassAd and a job ClassAd, must evaluate to TRUE before HTCondor
    will allow the job to use this machine.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; RetirementTimeRemaining>`
 ``RetirementTimeRemaining``:
    An integer number of seconds after ``MyCurrentTime`` when the
    running job can be evicted. ``MaxJobRetirementTime`` is the
    expression of how much retirement time the machine offers to new
    jobs, whereas ``RetirementTimeRemaining`` is the negotiated amount
    of time remaining for the current running job. This may be less than
    the amount offered by the machine’s ``MaxJobRetirementTime``
    expression, because the job may ask for less.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; SingularityVersion>`
 ``SingularityVersion``:
    A string containing the version of Singularity available, if the
    machine being advertised supports running jobs within a Singularity
    container (see ``HasSingularity``).
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; SlotID>`
 ``SlotID``:
    For SMP machines, the integer that identifies the slot. The value
    will be X for the slot with

    ::

        name="slotX@full.hostname"

    For non-SMP machines with one slot, the value will be 1.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; SlotType>`

 ``SlotType``:
    For SMP machines with partitionable slots, the partitionable slot
    will have this attribute set to ``"Partitionable"``, and all dynamic
    slots will have this attribute set to ``"Dynamic"``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; SlotWeight>`
 ``SlotWeight``:
    This specifies the weight of the slot when calculating usage,
    computing fair shares, and enforcing group quotas. For example,
    claiming a slot with ``SlotWeight = 2`` is equivalent to claiming
    two ``SlotWeight = 1`` slots. See the description of ``SlotWeight``
    on page \ `Configuration
    Macros <../admin-manual/configuration-macros.html>`__.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; StartdIpAddr>`
 ``StartdIpAddr``:
    String with the IP and port address of the *condor\_startd* daemon
    which is publishing this machine ClassAd. When using CCB,
    *condor\_shared\_port*, and/or an additional private network
    interface, that information will be included here as well.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; State>`
 ``State``:
    String which publishes the machine’s HTCondor state. Can be:

     ``"Owner"``:
        The machine owner is using the machine, and it is unavailable to
        HTCondor.
     ``"Unclaimed"``:
        The machine is available to run HTCondor jobs, but a good match
        is either not available or not yet found.
     ``"Matched"``:
        The HTCondor central manager has found a good match for this
        resource, but an HTCondor scheduler has not yet claimed it.
     ``"Claimed"``:
        The machine is claimed by a remote *condor\_schedd* and is
        probably running a job.
     ``"Preempting"``:
        An HTCondor job is being preempted (possibly via checkpointing)
        in order to clear the machine for either a higher priority job
        or because the machine owner wants the machine back.
     ``"Drained"``:
        This slot is not accepting jobs, because the machine is being
        drained.

    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TargetType>`
 ``TargetType``:
    Describes what type of ClassAd to match with. Always set to the
    string literal ``"Job"``, because machine ClassAds always want to be
    matched with jobs, and vice-versa.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalCondorLoadAvg>`
 ``TotalCondorLoadAvg``:
    The load average contributed by HTCondor summed across all slots on
    the machine, either from remote jobs or running benchmarks.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalCpus>`
 ``TotalCpus``:
    The number of CPUs (cores) that are on the machine. This is in
    contrast with ``Cpus``, which is the number of CPUs in the slot.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalDisk>`
 ``TotalDisk``:
    The quantity of disk space in KiB available across the machine (not
    the slot). For partitionable slots, where there is one partitionable
    slot per machine, this value will be the same as machine ClassAd
    attribute ``TotalSlotDisk``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalLoadAvg>`
 ``TotalLoadAvg``:
    A floating point number representing the current load average summed
    across all slots on the machine.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalMachineDrainingBadput>`
 ``TotalMachineDrainingBadput``:
    The total job runtime in cpu-seconds that has been lost due to job
    evictions caused by draining since this *condor\_startd* began
    executing. In this calculation, it is assumed that jobs are evicted
    without checkpointing.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalMachineDrainingUnclaimedTime>`
 ``TotalMachineDrainingUnclaimedTime``:
    The total machine-wide time in cpu-seconds that has not been used
    (i.e. not matched to a job submitter) due to draining since this
    *condor\_startd* began executing.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalMemory>`
 ``TotalMemory``:
    The quantity of RAM in MiB available across the machine (not the
    slot). For partitionable slots, where there is one partitionable
    slot per machine, this value will be the same as machine ClassAd
    attribute ``TotalSlotMemory``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalSlotCpus>`
 ``TotalSlotCpus``:
    The number of CPUs (cores) in this slot. For static slots, this
    value will be the same as in ``Cpus``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalSlotDisk>`
 ``TotalSlotDisk``:
    The quantity of disk space in KiB given to this slot. For static
    slots, this value will be the same as machine ClassAd attribute
    ``Disk``. For partitionable slots, where there is one partitionable
    slot per machine, this value will be the same as machine ClassAd
    attribute ``TotalDisk``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalSlotMemory>`
 ``TotalSlotMemory``:
    The quantity of RAM in MiB given to this slot. For static slots,
    this value will be the same as machine ClassAd attribute ``Memory``.
    For partitionable slots, where there is one partitionable slot per
    machine, this value will be the same as machine ClassAd attribute
    ``TotalMemory``.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalSlots>`
 ``TotalSlots``:
    A sum of the static slots, partitionable slots, and dynamic slots on
    the machine at the current time.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeBackfillBusy>`
 ``TotalTimeBackfillBusy``:
    The number of seconds that this machine (slot) has accumulated
    within the backfill busy state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeBackfillIdle>`
 ``TotalTimeBackfillIdle``:
    The number of seconds that this machine (slot) has accumulated
    within the backfill idle state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeBackfillKilling>`
 ``TotalTimeBackfillKilling``:
    The number of seconds that this machine (slot) has accumulated
    within the backfill killing state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeClaimedBusy>`
 ``TotalTimeClaimedBusy``:
    The number of seconds that this machine (slot) has accumulated
    within the claimed busy state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeClaimedIdle>`
 ``TotalTimeClaimedIdle``:
    The number of seconds that this machine (slot) has accumulated
    within the claimed idle state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeClaimedRetiring>`
 ``TotalTimeClaimedRetiring``:
    The number of seconds that this machine (slot) has accumulated
    within the claimed retiring state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeClaimedSuspended>`
 ``TotalTimeClaimedSuspended``:
    The number of seconds that this machine (slot) has accumulated
    within the claimed suspended state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeMatchedIdle>`
 ``TotalTimeMatchedIdle``:
    The number of seconds that this machine (slot) has accumulated
    within the matched idle state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeOwnerIdle>`
 ``TotalTimeOwnerIdle``:
    The number of seconds that this machine (slot) has accumulated
    within the owner idle state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimePreemptingKilling>`
 ``TotalTimePreemptingKilling``:
    The number of seconds that this machine (slot) has accumulated
    within the preempting killing state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimePreemptingVacating>`
 ``TotalTimePreemptingVacating``:
    The number of seconds that this machine (slot) has accumulated
    within the preempting vacating state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeUnclaimedBenchmarking>`
 ``TotalTimeUnclaimedBenchmarking``:
    The number of seconds that this machine (slot) has accumulated
    within the unclaimed benchmarking state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; TotalTimeUnclaimedIdle>`
 ``TotalTimeUnclaimedIdle``:
    The number of seconds that this machine (slot) has accumulated
    within the unclaimed idle state and activity pair since the
    *condor\_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; UidDomain>`
 ``UidDomain``:
    a domain name configured by the HTCondor administrator which
    describes a cluster of machines which all have the same ``passwd``
    file entries, and therefore all have the same logins.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; VirtualMemory>`
 ``VirtualMemory``:
    The amount of currently available virtual memory (swap space)
    expressed in KiB. On Linux platforms, it is the sum of paging space
    and physical memory, which more accurately represents the virtual
    memory size of the machine.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; VM_AvailNum>`
 ``VM_AvailNum``:
    The maximum number of vm universe jobs that can be started on this
    machine. This maximum is set by the configuration variable
    ``VM_MAX_NUMBER`` :index:`VM_MAX_NUMBER<single: VM_MAX_NUMBER>`.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; VM_Guest_Mem>`
 ``VM_Guest_Mem``:
    An attribute defined if a vm universe job is running on this slot.
    Defined by the amount of memory in use by the virtual machine, given
    in Mbytes. :index:`ClassAd machine attribute<single: ClassAd machine attribute; VM_Memory>`
 ``VM_Memory``:
    Gives the amount of memory available for starting additional VM jobs
    on this machine, given in Mbytes. The maximum value is set by the
    configuration variable ``VM_MEMORY`` :index:`VM_MEMORY<single: VM_MEMORY>`.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; VM_Networking>`
 ``VM_Networking``:
    A boolean value indicating whether networking is allowed for virtual
    machines on this machine.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; VM_Type>`
 ``VM_Type``:
    The type of virtual machine software that can run on this machine.
    The value is set by the configuration variable ``VM_TYPE``
    :index:`VM_TYPE<single: VM_TYPE>`.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; VMOfflineReason>`
 ``VMOfflineReason``:
    The reason the VM universe went offline (usually because a VM
    universe job failed to launch).
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; VMOfflineTime>`
 ``VMOfflineTime``:
    The time that the VM universe went offline.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; WindowsBuildNumber>`
 ``WindowsBuildNumber``:
    An integer, extracted from the platform type, representing a build
    number for a Windows operating system. This attribute only exists on
    Windows machines.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; WindowsMajorVersion>`
 ``WindowsMajorVersion``:
    An integer, extracted from the platform type, representing a major
    version number (currently 5 or 6) for a Windows operating system.
    This attribute only exists on Windows machines.
    :index:`ClassAd machine attribute<single: ClassAd machine attribute; WindowsMinorVersion>`
 ``WindowsMinorVersion``:
    An integer, extracted from the platform type, representing a minor
    version number (currently 0, 1, or 2) for a Windows operating
    system. This attribute only exists on Windows machines.

In addition, there are a few attributes that are automatically inserted
into the machine ClassAd whenever a resource is in the Claimed state:
` <index://ClientMachine;ClassAd machine attribute (in Claimed State)>`__

 ``ClientMachine``:
    The host name of the machine that has claimed this resource
    ` <index://RemoteAutoregroup;ClassAd machine attribute (in Claimed State)>`__
 ``RemoteAutoregroup``:
    A boolean attribute which is ``True`` if this resource was claimed
    via negotiation when the configuration variable
    ``GROUP_AUTOREGROUP`` :index:`GROUP_AUTOREGROUP<single: GROUP_AUTOREGROUP>` is ``True``.
    It is ``False`` otherwise.
    ` <index://RemoteGroup;ClassAd machine attribute (in Claimed State)>`__
 ``RemoteGroup``:
    The accounting group name corresponding to the submitter that
    claimed this resource.
    ` <index://RemoteNegotiatingGroup;ClassAd machine attribute (in Claimed State)>`__
 ``RemoteNegotiatingGroup``:
    The accounting group name under which this resource negotiated when
    it was claimed. This attribute will frequently be the same as
    attribute ``RemoteGroup``, but it may differ in cases such as when
    configuration variable ``GROUP_AUTOREGROUP``
    :index:`GROUP_AUTOREGROUP<single: GROUP_AUTOREGROUP>` is ``True``, in which case it will
    have the name of the root group, identified as ``<none>``.
    ` <index://RemoteOwner;ClassAd machine attribute (in Claimed State)>`__
 ``RemoteOwner``:
    The name of the user who originally claimed this resource.
    ` <index://RemoteUser;ClassAd machine attribute (in Claimed State)>`__
 ``RemoteUser``:
    The name of the user who is currently using this resource. In
    general, this will always be the same as the ``RemoteOwner``, but in
    some cases, a resource can be claimed by one entity that hands off
    the resource to another entity which uses it. In that case,
    ``RemoteUser`` would hold the name of the entity currently using the
    resource, while ``RemoteOwner`` would hold the name of the entity
    that claimed the resource.
    ` <index://PreemptingOwner;ClassAd machine attribute (in Claimed State)>`__
 ``PreemptingOwner``:
    The name of the user who is preempting the job that is currently
    running on this resource.
    ` <index://PreemptingUser;ClassAd machine attribute (in Claimed State)>`__
 ``PreemptingUser``:
    The name of the user who is preempting the job that is currently
    running on this resource. The relationship between
    ``PreemptingUser`` and ``PreemptingOwner`` is the same as the
    relationship between ``RemoteUser`` and ``RemoteOwner``.
    ` <index://PreemptingRank;ClassAd machine attribute (in Claimed State)>`__
 ``PreemptingRank``:
    A float which represents this machine owner’s affinity for running
    the HTCondor job which is waiting for the current job to finish or
    be preempted. If not currently hosting an HTCondor job,
    ``PreemptingRank`` is undefined. When a machine is claimed and there
    is already a job running, the attribute’s value is computed by
    evaluating the machine’s ``Rank`` expression with respect to the
    preempting job’s ClassAd.
    ` <index://TotalClaimRunTime;ClassAd machine attribute (in Claimed State)>`__
 ``TotalClaimRunTime``:
    A running total of the amount of time (in seconds) that all jobs
    (under the same claim) ran (have spent in the Claimed/Busy state).
    ` <index://TotalClaimSuspendTime;ClassAd machine attribute (in Claimed State)>`__
 ``TotalClaimSuspendTime``:
    A running total of the amount of time (in seconds) that all jobs
    (under the same claim) have been suspended (in the Claimed/Suspended
    state).
    ` <index://TotalJobRunTime;ClassAd machine attribute (in Claimed State)>`__
 ``TotalJobRunTime``:
    A running total of the amount of time (in seconds) that a single job
    ran (has spent in the Claimed/Busy state).
    ` <index://TotalJobSuspendTime;ClassAd machine attribute (in Claimed State)>`__
 ``TotalJobSuspendTime``:
    A running total of the amount of time (in seconds) that a single job
    has been suspended (in the Claimed/Suspended state).

There are a few attributes that are only inserted into the machine
ClassAd if a job is currently executing. If the resource is claimed but
no job are running, none of these attributes will be defined.
` <index://JobId;ClassAd machine attribute (when running)>`__

 ``JobId``:
    The job’s identifier (for example, 152.3), as seen from *condor\_q*
    on the submitting machine.
    ` <index://JobStart;ClassAd machine attribute (when running)>`__
 ``JobStart``:
    The time stamp in integer seconds of when the job began executing,
    since the Unix epoch (00:00:00 UTC, Jan 1, 1970). For idle machines,
    the value is ``UNDEFINED``.
    ` <index://LastPeriodicCheckpoint;ClassAd machine attribute (when running)>`__
 ``LastPeriodicCheckpoint``:
    If the job has performed a periodic checkpoint, this attribute will
    be defined and will hold the time stamp of when the last periodic
    checkpoint was begun. If the job has yet to perform a periodic
    checkpoint, or cannot checkpoint at all, the
    ``LastPeriodicCheckpoint`` attribute will not be defined.

:index:`offline ClassAd<single: offline ClassAd>`

There are a few attributes that are applicable to machines that are
offline, that is, hibernating.
` <index://MachineLastMatchTime;ClassAd machine attribute (when offline)>`__

 ``MachineLastMatchTime``:
    The Unix epoch time when this offline ClassAd would have been
    matched to a job, if the machine were online. In addition, the slot1
    ClassAd of a multi-slot machine will have
    ``slot<X>_MachineLastMatchTime`` defined, where ``<X>`` is replaced
    by the slot id of each of the slots with ``MachineLastMatchTime``
    defined.
    ` <index://Offline;ClassAd machine attribute (when offline)>`__
 ``Offline``:
    A boolean value, that when ``True``, indicates this machine is in an
    offline state in the *condor\_collector*. Such ClassAds are stored
    persistently, such that they will continue to exist after the
    *condor\_collector* restarts.
    ` <index://Unhibernate;ClassAd machine attribute (when offline)>`__
 ``Unhibernate``:
    A boolean expression that specifies when a hibernating machine
    should be woken up, for example, by *condor\_rooster*.

For machines with user-defined or custom resource specifications,
including GPUs, the following attributes will be in the ClassAd for each
slot. In the name of the attribute, ``<name>`` is substituted with the
configured name given to the resource.
` <index://Assigned<name>;ClassAd machine attribute (for a user-defined resource)>`__

 ``Assigned<name>``:
    A space separated list that identifies which of these resources are
    currently assigned to slots.
    ` <index://Offline<name>;ClassAd machine attribute (for a user-defined resource)>`__
 ``Offline<name>``:
    A space separated list that indicates which of these resources is
    unavailable for match making.
    ` <index://Total<name>;ClassAd machine attribute (for a user-defined resource)>`__
 ``Total<name>``:
    An integer quantity of the total number of these resources.

For machines with custom resource specifications that include GPUs, the
following attributes may be in the ClassAd for each slot, depending on
the value of configuration variable ``MACHINE_RESOURCE_INVENTORY_GPUs``
:index:`MACHINE_RESOURCE_INVENTORY_GPUs<single: MACHINE_RESOURCE_INVENTORY_GPUs>` and what GPUs are
detected. In the name of the attribute, ``<name>`` is substituted with
the *prefix string* assigned for the GPU.
` <index://<name>BoardTempC;ClassAd machine attribute (for GPU resources)>`__

 ``<name>BoardTempC``:
    For NVIDIA devices, a dynamic attribute representing the temperature
    in Celsius of the board containing the GPU.
    ` <index://<name>Capability;ClassAd machine attribute (for GPU resources)>`__
 ``<name>Capability``:
    The CUDA-defined capability for the GPU.
    ` <index://<name>ClockMhz;ClassAd machine attribute (for GPU resources)>`__
 ``<name>ClockMhz``:
    For CUDA or Open CL devices, the integer clocking speed of the GPU
    in MHz.
    ` <index://<name>ComputeUnits;ClassAd machine attribute (for GPU resources)>`__
 ``<name>ComputeUnits``:
    For CUDA or Open CL devices, the integer number of compute units per
    GPU.
    ` <index://<name>CoresPerCU;ClassAd machine attribute (for GPU resources)>`__
 ``<name>CoresPerCU``:
    For CUDA devices, the integer number of cores per compute unit.
    ` <index://<name>DeviceName;ClassAd machine attribute (for GPU resources)>`__
 ``<name>DeviceName``:
    For CUDA or Open CL devices, a string representing the
    manufacturer’s proprietary device name.
    ` <index://<name>DieTempC;ClassAd machine attribute (for GPU resources)>`__
 ``<name>DieTempC``:
    For NVIDIA devices, a dynamic attribute representing the temperature
    in Celsius of the GPU die.
    ` <index://<name>DriverVersion;ClassAd machine attribute (for GPU resources)>`__
 ``<name>DriverVersion``:
    For CUDA devices, a string representing the manufacturer’s driver
    version.
    ` <index://<name>ECCEnabled;ClassAd machine attribute (for GPU resources)>`__
 ``<name>ECCEnabled``:
    For CUDA or Open CL devices, a boolean value representing whether
    error correction is enabled.
    ` <index://<name>EccErrorsDoubleBit;ClassAd machine attribute (for GPU resources)>`__
 ``<name>EccErrorsDoubleBit``:
    For NVIDIA devices, a count of the number of double bit errors
    detected for this GPU.
    ` <index://<name>EccErrorsSingleBit;ClassAd machine attribute (for GPU resources)>`__
 ``<name>EccErrorsSingleBit``:
    For NVIDIA devices, a count of the number of single bit errors
    detected for this GPU.
    ` <index://<name>FanSpeedPct;ClassAd machine attribute (for GPU resources)>`__
 ``<name>FanSpeedPct``:
    For NVIDIA devices, a value between 0 and 100 (inclusive), used to
    represent the level of fan operation as percentage of full fan
    speed.
    ` <index://<name>GlobalMemoryMb;ClassAd machine attribute (for GPU resources)>`__
 ``<name>GlobalMemoryMb``:
    For CUDA or Open CL devices, the quantity of memory in Mbytes in
    this GPU.
    ` <index://<name>OpenCLVersion;ClassAd machine attribute (for GPU resources)>`__
 ``<name>OpenCLVersion``:
    For Open CL devices, a string representing the manufacturer’s
    version number.
    ` <index://<name>RuntimeVersion;ClassAd machine attribute (for GPU resources)>`__
 ``<name>RuntimeVersion``:
    For CUDA devices, a string representing the manufacturer’s version
    number.

The following attributes are advertised for a machine in which
partitionable slot preemption is enabled.
` <index://ChildAccountingGroup;ClassAd machine attribute (for pslot preemption)>`__

 ``ChildAccountingGroup``:
    A ClassAd list containing the values of the ``AccountingGroup``
    attribute for each dynamic slot of the partitionable slot.
    ` <index://ChildActivity;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildActivity``:
    A ClassAd list containing the values of the ``Activity`` attribute
    for each dynamic slot of the partitionable slot.
    ` <index://ChildCpus;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildCpus``:
    A ClassAd list containing the values of the ``Cpus`` attribute for
    each dynamic slot of the partitionable slot.
    ` <index://ChildCurrentRank;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildCurrentRank``:
    A ClassAd list containing the values of the ``CurrentRank``
    attribute for each dynamic slot of the partitionable slot.
    ` <index://ChildEnteredCurrentState;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildEnteredCurrentState``:
    A ClassAd list containing the values of the ``EnteredCurrentState``
    attribute for each dynamic slot of the partitionable slot.
    ` <index://ChildMemory;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildMemory``:
    A ClassAd list containing the values of the ``Memory`` attribute for
    each dynamic slot of the partitionable slot.
    ` <index://ChildName;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildName``:
    A ClassAd list containing the values of the ``Name`` attribute for
    each dynamic slot of the partitionable slot.
    ` <index://ChildRemoteOwner;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildRemoteOwner``:
    A ClassAd list containing the values of the ``RemoteOwner``
    attribute for each dynamic slot of the partitionable slot.
    ` <index://ChildRemoteUser;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildRemoteUser``:
    A ClassAd list containing the values of the ``RemoteUser`` attribute
    for each dynamic slot of the partitionable slot.
    ` <index://ChildRetirementTimeRemaining;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildRetirementTimeRemaining``:
    A ClassAd list containing the values of the
    ``RetirementTimeRemaining`` attribute for each dynamic slot of the
    partitionable slot.
    ` <index://ChildState;ClassAd machine attribute (for pslot preemption)>`__
 ``ChildState``:
    A ClassAd list containing the values of the ``State`` attribute for
    each dynamic slot of the partitionable slot.
    ` <index://PslotRollupInformation;ClassAd machine attribute (for pslot preemption)>`__
 ``PslotRollupInformation``:
    A boolean value set to ``True`` in both the partitionable and
    dynamic slots, when configuration variable
    ``ADVERTISE_PSLOT_ROLLUP_INFORMATION`` is ``True``, such that the
    *condor\_negotiator* knows when partitionable slot preemption is
    possible and can directly preempt a dynamic slot when appropriate.

Finally, the single attribute, ``CurrentTime``, is defined by the
ClassAd environment. :index:`ClassAd attribute<single: ClassAd attribute; CurrentTime>`

 ``CurrentTime``:
    Evaluates to the the number of integer seconds since the Unix epoch
    (00:00:00 UTC, Jan 1, 1970).

      
