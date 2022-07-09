Machine ClassAd Attributes
==========================


:index:`machine attributes<single: machine attributes; ClassAd>`

:classad-attribute:`AcceptedWhileDraining`
    Boolean which indicates if the slot accepted its current job while
    the machine was draining.

:classad-attribute:`Activity`
    String which describes HTCondor job activity on the machine. Can
    have one of the following values:

    ``"Idle"``
        There is no job activity

    ``"Busy"``
        A job is busy running

    ``"Suspended"``
        A job is currently suspended

    ``"Vacating"``
        A job is currently checkpointing

    ``"Killing"``
        A job is currently being killed

    ``"Benchmarking"``
        The startd is running benchmarks

    ``"Retiring"``
        Waiting for a job to finish or for the maximum retirement time to expire

:classad-attribute:`Arch`
    String with the architecture of the machine. Currently supported
    architectures have the following string definitions:

    ``"INTEL"``
        Intel x86 CPU (Pentium, Xeon, etc).

    ``"X86_64"``
        AMD/Intel 64-bit X86

:classad-attribute:`CanHibernate`
    The *condor_startd* has the capability to shut down or hibernate a
    machine when certain configurable criteria are met. However, before
    the *condor_startd* can shut down a machine, the hardware itself
    must support hibernation, as must the operating system. When the
    *condor_startd* initializes, it checks for this support. If the
    machine has the ability to hibernate, then this boolean ClassAd
    attribute will be ``True``. By default, it is ``False``.

:classad-attribute:`ClockDay`
    The day of the week, where 0 = Sunday, 1 = Monday, ..., and 6 =
    Saturday.
    
:classad-attribute:`ClockMin`
    The number of minutes passed since midnight.

:classad-attribute:`CondorLoadAvg`
    The load average contributed by HTCondor, either from remote jobs or
    running benchmarks.

:classad-attribute:`CondorVersion`
    A string containing the HTCondor version number for the
    *condor_startd* daemon, the release date, and the build
    identification number.

:classad-attribute:`ConsoleIdle`
    The number of seconds since activity on the system console keyboard
    or console mouse has last been detected. The value can be modified
    with ``SLOTS_CONNECTED_TO_CONSOLE`` :index:`SLOTS_CONNECTED_TO_CONSOLE` as defined in the
    :ref:`admin-manual/configuration-macros:condor_startd configuration
    file macros` section.

:classad-attribute:`Cpus`
    The number of CPUs (cores) in this slot. It is 1 for a single CPU
    slot, 2 for a dual CPU slot, etc. For a partitionable slot, it is
    the remaining number of CPUs in the partitionable slot.

:classad-attribute:`CpuFamily`
    On Linux machines, the Cpu family, as defined in the /proc/cpuinfo
    file.

:classad-attribute:`CpuModel`
    On Linux machines, the Cpu model number, as defined in the
    /proc/cpuinfo file.

:classad-attribute:`CpuCacheSize`
    On Linux machines, the size of the L3 cache, in kbytes, as defined
    in the /proc/cpuinfo file.

:classad-attribute:`CurrentRank`
    A float which represents this machine owner's affinity for running
    the HTCondor job which it is currently hosting. If not currently
    hosting an HTCondor job, ``CurrentRank`` is 0.0. When a machine is
    claimed, the attribute's value is computed by evaluating the
    machine's ``Rank`` expression with respect to the current job's
    ClassAd.
    
:classad-attribute:`DetectedCpus`
    Set by the value of configuration variable ``DETECTED_CORES``

:classad-attribute:`DetectedMemory`
    Set by the value of configuration variable ``DETECTED_MEMORY``.
    :index:`DETECTED_MEMORY` Specified in MiB.

:classad-attribute:`Disk`
    The amount of disk space on this machine available for the job in
    KiB (for example, 23000 = 23 MiB). Specifically, this is the amount
    of disk space available in the directory specified in the HTCondor
    configuration files by the ``EXECUTE`` :index:`EXECUTE` macro,
    minus any space reserved with the ``RESERVED_DISK`` :index:`RESERVED_DISK`
    macro. For static slots, this value
    will be the same as machine ClassAd attribute ``TotalSlotDisk``. For
    partitionable slots, this value will be the quantity of disk space
    remaining in the partitionable slot.

:classad-attribute:`Draining`
    This attribute is ``True`` when the slot is draining and undefined
    if not.

:classad-attribute:`DrainingRequestId`
    This attribute contains a string that is the request id of the
    draining request that put this slot in a draining state. It is
    undefined if the slot is not draining.

:classad-attribute:`DotNetVersions`
    The .NET framework versions currently installed on this computer.
    Default format is a comma delimited list. Current definitions:

     ``"1.1"``
        for .Net Framework 1.1
     ``"2.0"``
        for .Net Framework 2.0
     ``"3.0"``
        for .Net Framework 3.0
     ``"3.5"``
        for .Net Framework 3.5
     ``"4.0Client"``
        for .Net Framework 4.0 Client install
     ``"4.0Full"``
        for .Net Framework 4.0 Full install


:classad-attribute:`DynamicSlot`
    For SMP machines that allow dynamic partitioning of a slot, this
    boolean value identifies that this dynamic slot may be partitioned.

:classad-attribute:`EnteredCurrentActivity`
    Time at which the machine entered the current Activity (see
    ``Activity`` entry above). On all platforms (including NT), this is
    measured in the number of integer seconds since the Unix epoch
    (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`ExpectedMachineGracefulDrainingBadput`
    The job run time in cpu-seconds that would be lost if graceful
    draining were initiated at the time this ClassAd was published. This
    calculation assumes that jobs will run for the full retirement time
    and then be evicted without saving a checkpoint.

:classad-attribute:`ExpectedMachineGracefulDrainingCompletion`
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

:classad-attribute:`ExpectedMachineQuickDrainingBadput`
    The job run time in cpu-seconds that would be lost if quick or fast
    draining were initiated at the time this ClassAd was published. This
    calculation assumes that all evicted jobs will not save a
    checkpoint.

:classad-attribute:`ExpectedMachineQuickDrainingCompletion`
    Time at which quick or fast draining of the machine could complete
    if it were initiated at the time this ClassAd was published and
    there are no active claims. This is measured in the number of
    integer seconds since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute:`FileSystemDomain`
    A domain name configured by the HTCondor administrator which
    describes a cluster of machines which all access the same,
    uniformly-mounted, networked file systems usually via NFS or AFS.
    This is useful for Vanilla universe jobs which require remote file
    access.

:classad-attribute:`HasDocker`
    A boolean value set to ``True`` if the machine is capable of
    executing docker universe jobs.

:classad-attribute:`HasEncryptExecuteDirectory`
    A boolean value set to ``True`` if the machine is capable of
    encrypting execute directories.

:classad-attribute:`HasFileTransfer`
    A boolean value that when ``True`` identifies that the machine can
    use the file transfer mechanism.

:classad-attribute:`HasFileTransferPluginMethods`
    A string of comma-separated file transfer protocols that the machine
    can support. The value can be modified with ``FILETRANSFER_PLUGINS`` :index:`FILETRANSFER_PLUGINS` 
    as defined in :ref:`admin-manual/configuration-macros:condor_starter configuration file
    entries`.

:classad-attribute:`HasUserNamespaces`
    A boolean value that when ``True`` identifies that the jobs on this machine
    can create user namespaces without root privileges.

:classad-attribute:`Has_sse4_1`
    A boolean value set to ``True`` if the machine being advertised
    supports the SSE 4.1 instructions, and ``Undefined`` otherwise.

:classad-attribute:`Has_sse4_2`
    A boolean value set to ``True`` if the machine being advertised
    supports the SSE 4.2 instructions, and ``Undefined`` otherwise.

:classad-attribute:`has_ssse3`
    A boolean value set to ``True`` if the machine being advertised
    supports the SSSE 3 instructions, and ``Undefined`` otherwise.

:classad-attribute:`has_avx`
    A boolean value set to ``True`` if the machine being advertised
    supports the avx instructions, and ``Undefined`` otherwise.

:classad-attribute:`has_avx2`
    A boolean value set to ``True`` if the machine being advertised
    supports the avx2 instructions, and ``Undefined`` otherwise.

:classad-attribute:`has_avx512f`
    A boolean value set to ``True`` if the machine being advertised
    support the avx512f (foundational) instructions.

:classad-attribute:`has_avx512dq`
    A boolean value set to ``True`` if the machine being advertised
    support the avx512dq instructions.

:classad-attribute:`has_avx512dnni`
    A boolean value set to ``True`` if the machine being advertised
    support the avx512dnni instructions.

:classad-attribute:`HasSelfCheckpointTransfers`
    A boolean value set to ``True`` if the machine being advertised
    supports transferring (checkpoint) files (to the submit node)
    when the job successfully self-checkpoints.

:classad-attribute:`HasSingularity`
    A boolean value set to ``True`` if the machine being advertised
    supports running jobs within Singularity containers.

:classad-attribute:`HasVM`
    If the configuration triggers the detection of virtual machine
    software, a boolean value reporting the success thereof; otherwise
    undefined. May also become ``False`` if HTCondor determines that it
    can't start a VM (even if the appropriate software is detected).

:classad-attribute:`IsWakeAble`
    A boolean value that when ``True`` identifies that the machine has
    the capability to be woken into a fully powered and running state by
    receiving a Wake On LAN (WOL) packet. This ability is a function of
    the operating system, the network adapter in the machine (notably,
    wireless network adapters usually do not have this function), and
    BIOS settings. When the *condor_startd* initializes, it tries to
    detect if the operating system and network adapter both support
    waking from hibernation by receipt of a WOL packet. The default
    value is ``False``.

:classad-attribute:`IsWakeEnabled`
    If the hardware and software have the capacity to be woken into a
    fully powered and running state by receiving a Wake On LAN (WOL)
    packet, this feature can still be disabled via the BIOS or software.
    If BIOS or the operating system have disabled this feature, the
    *condor_startd* sets this boolean attribute to ``False``.

:classad-attribute:`JobBusyTimeAvg`
    The Average lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor_starter* that
    has exited. This attribute will be undefined until the first time a
    *condor_starter* has exited.

:classad-attribute:`JobBusyTimeCount`
    attribute. This is also the the total number times a
    *condor_starter* has exited.

:classad-attribute:`JobBusyTimeMax`
    The Maximum lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor_starter* s
    that has exited. This attribute will be undefined until the first
    time a *condor_starter* has exited.

:classad-attribute:`JobBusyTimeMin`
    The Minimum lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor_starter* that
    has exited. This attribute will be undefined until the first time a
    *condor_starter* has exited.

:classad-attribute:`RecentJobBusyTimeAvg`
    The Average lifetime of all jobs that have exited in the last 20
    minutes, including transfer time. This is determined by measuring
    the lifetime of each *condor_starter* that has exited in the last
    20 minutes. This attribute will be undefined if no *condor_starter*
    has exited in the last 20 minutes.

:classad-attribute:`RecentJobBusyTimeCount`
    The total number of jobs used to calulate the
    ``RecentJobBusyTimeAvg`` attribute. This is also the the total
    number times a *condor_starter* has exited in the last 20 minutes.

:classad-attribute:`RecentJobBusyTimeMax`
    The Maximum lifetime of all jobs that have exited in the last 20
    minutes, including transfer time. This is determined by measuring
    the lifetime of each *condor_starter* s that has exited in the
    last 20 minutes. This attribute will be undefined if no
    *condor_starter* has exited in the last 20 minutes.

:classad-attribute:`RecentJobBusyTimeMin`
    The Minimum lifetime of all jobs, including transfer time. This is
    determined by measuring the lifetime of each *condor_starter* that
    has exited. This attribute will be undefined if no *condor_starter*
    has exited in the last 20 minutes.

:classad-attribute:`JobDurationAvg`
    The Average lifetime time of all jobs, not including time spent
    transferring files. This attribute will be undefined until the first
    time a job exits. Jobs that never start (because they fail to
    transfer input, for instance) will not be included in the average.

:classad-attribute:`JobDurationCount`
    attribute. This is also the the total number times a job has exited.
    Jobs that never start (because input transfer fails, for instance)
    are not included in the count.

:classad-attribute:`JobDurationMax`
    The lifetime of the longest lived job that has exited. This
    attribute will be undefined until the first time a job exits.

:classad-attribute:`JobDurationMin`
    The lifetime of the shortest lived job that has exited. This
    attribute will be undefined until the first time a job exits.

:classad-attribute:`RecentJobDurationAvg`
    The Average lifetime time of all jobs, not including time spent
    transferring files, that have exited in the last 20 minutes. This
    attribute will be undefined if no job has exited in the last 20
    minutes.

:classad-attribute:`RecentJobDurationCount`
    The total number of jobs used to calulate the
    ``RecentJobDurationAvg`` attribute. This is the total number of jobs
    that began execution and have exited in the last 20 minutes.

:classad-attribute:`RecentJobDurationMax`
    The lifetime of the longest lived job that has exited in the last 20
    minutes. This attribute will be undefined if no job has exited in
    the last 20 minutes.

:classad-attribute:`RecentJobDurationMin`
    The lifetime of the shortest lived job that has exited in the last
    20 minutes. This attribute will be undefined if no job has exited in
    the last 20 minutes.

:classad-attribute:`JobPreemptions`
    The total number of times a running job has been preempted on this
    machine.

:classad-attribute:`JobRankPreemptions`
    The total number of times a running job has been preempted on this
    machine due to the machine's rank of jobs since the *condor_startd*
    started running.

:classad-attribute:`JobStarts`
    The total number of jobs which have been started on this machine
    since the *condor_startd* started running.

:classad-attribute:`JobUserPrioPreemptions`
    The total number of times a running job has been preempted on this
    machine based on a fair share allocation of the pool since the
    *condor_startd* started running.

:classad-attribute:`JobVM_VCPUS`
    An attribute defined if a vm universe job is running on this slot.
    Defined by the number of virtualized CPUs in the virtual machine.

:classad-attribute:`KeyboardIdle`
    The number of seconds since activity on any keyboard or mouse
    associated with this machine has last been detected. Unlike
    ``ConsoleIdle``, ``KeyboardIdle`` also takes activity on
    pseudo-terminals into account. Pseudo-terminals have virtual
    keyboard activity from telnet and rlogin sessions. Note that
    ``KeyboardIdle`` will always be equal to or less than
    ``ConsoleIdle``. The value can be modified with
    ``SLOTS_CONNECTED_TO_KEYBOARD`` :index:`SLOTS_CONNECTED_TO_KEYBOARD` as defined in the
    :ref:`admin-manual/configuration-macros:condor_startd configuration file
    macros` section.

:classad-attribute:`KFlops`
    Relative floating point performance as determined via a Linpack
    benchmark.

:classad-attribute:`LastDrainStartTime`
    Time when draining of this *condor_startd* was last initiated (e.g.
    due to *condor_defrag* or *condor_drain*).

:classad-attribute:`LastDrainStopTime`
    Time when draining of this *condor_startd* was last stopped (e.g.
    by being cancelled).

:classad-attribute:`LastHeardFrom`
    Time when the HTCondor central manager last received a status update
    from this machine. Expressed as the number of integer seconds since
    the Unix epoch (00:00:00 UTC, Jan 1, 1970). Note: This attribute is
    only inserted by the central manager once it receives the ClassAd.
    It is not present in the *condor_startd* copy of the ClassAd.
    Therefore, you could not use this attribute in defining
    *condor_startd* expressions (and you would not want to).

:classad-attribute:`LoadAvg`
    A floating point number representing the current load average.

:classad-attribute:`Machine`
    A string with the machine's fully qualified host name.

:classad-attribute:`MachineMaxVacateTime`
    An integer expression that specifies the time in seconds the machine
    will allow the job to gracefully shut down.

:classad-attribute:`MaxJobRetirementTime`
    When the *condor_startd* wants to kick the job off, a job which has
    run for less than this number of seconds will not be hard-killed.
    The *condor_startd* will wait for the job to finish or to exceed
    this amount of time, whichever comes sooner. If the job vacating
    policy grants the job X seconds of vacating time, a preempted job
    will be soft-killed X seconds before the end of its retirement time,
    so that hard-killing of the job will not happen until the end of the
    retirement time if the job does not finish shutting down before
    then. This is an expression evaluated in the context of the job
    ClassAd, so it may refer to job attributes as well as machine
    attributes.

:classad-attribute:`Memory`
    The amount of RAM in MiB in this slot. For static slots, this value
    will be the same as in ``TotalSlotMemory``. For a partitionable
    slot, this value will be the quantity remaining in the partitionable
    slot. 
    
:classad-attribute:`Mips`
    Relative integer performance as determined via a Dhrystone
    benchmark.

:classad-attribute:`MonitorSelfAge`
    The number of seconds that this daemon has been running.

:classad-attribute:`MonitorSelfCPUUsage`
    The fraction of recent CPU time utilized by this daemon.

:classad-attribute:`MonitorSelfImageSize`
    The amount of virtual memory consumed by this daemon in KiB.

:classad-attribute:`MonitorSelfRegisteredSocketCount`
    The current number of sockets registered by this daemon.

:classad-attribute:`MonitorSelfResidentSetSize`
    The amount of resident memory used by this daemon in KiB.

:classad-attribute:`MonitorSelfSecuritySessions`
    The number of open (cached) security sessions for this daemon.

:classad-attribute:`MonitorSelfTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.

:classad-attribute:`MyAddress`
    String with the IP and port address of the *condor_startd* daemon
    which is publishing this machine ClassAd. When using CCB,
    *condor_shared_port*, and/or an additional private network
    interface, that information will be included here as well.

:classad-attribute:`MyCurrentTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor_startd*
    daemon last sent a ClassAd update to the *condor_collector*.

:classad-attribute:`MyType`
    The ClassAd type; always set to the literal string ``"Machine"``.

:classad-attribute:`Name`
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form "slot#@full.hostname", for example,
    "slot1@vulture.cs.wisc.edu", which signifies slot number 1 from
    vulture.cs.wisc.edu.

:classad-attribute:`Offline`
    A string that lists specific instances of a user-defined machine
    resource, identified by ``name``. Each instance is currently
    unavailable for purposes of match making.

:classad-attribute:`OfflineUniverses`
    A ClassAd list that specifies which job universes are presently
    offline, both as strings and as the corresponding job universe
    number. Could be used the the startd to refuse to start jobs in
    offline universes:

    .. code-block:: condor-config

        START = OfflineUniverses is undefined || (! member( JobUniverse, OfflineUniverses ))

    May currently only contain ``"VM"`` and ``13``.

:classad-attribute:`OpSys`
    String describing the operating system running on this machine.
    Currently supported operating systems have the following string
    definitions:

     ``"LINUX"``
        for LINUX 2.0.x, LINUX 2.2.x, LINUX 2.4.x, LINUX 2.6.x, or LINUX
        3.10.0 kernel systems, as well as Scientific Linux, Ubuntu
        versions 14.04, and Debian 7.0 (wheezy) and 8.0 (jessie)
     ``"OSX"``
        for Darwin
     ``"FREEBSD7"``
        for FreeBSD 7
     ``"FREEBSD8"``
        for FreeBSD 8
     ``"WINDOWS"``
        for all versions of Windows

:classad-attribute:`OpSysAndVer`
    A string indicating an operating system and a version number.

    For Linux operating systems, it is the value of the ``OpSysName``
    attribute concatenated with the string version of the
    ``OpSysMajorVer`` attribute:

     ``"RedHat5"``
        for RedHat Linux version 5
     ``"RedHat6"``
        for RedHat Linux version 6
     ``"RedHat7"``
        for RedHat Linux version 7
     ``"Fedora16"``
        for Fedora Linux version 16
     ``"Debian6"``
        for Debian Linux version 6
     ``"Debian7"``
        for Debian Linux version 7
     ``"Debian8"``
        for Debian Linux version 8
     ``"Debian9"``
        for Debian Linux version 9
     ``"Ubuntu14"``
        for Ubuntu 14.04
     ``"SL5"``
        for Scientific Linux version 5
     ``"SL6"``
        for Scientific Linux version 6
     ``"SLFermi5"``
        for Fermi's Scientific Linux version 5
     ``"SLFermi6"``
        for Fermi's Scientific Linux version 6
     ``"SLCern5"``
        for CERN's Scientific Linux version 5
     ``"SLCern6"``
        for CERN's Scientific Linux version 6

    For MacOS operating systems, it is the value of the
    ``OpSysShortName`` attribute concatenated with the string version of
    the ``OpSysVer`` attribute:

     ``"MacOSX605"``
        for MacOS version 10.6.5 (Snow Leopard)
     ``"MacOSX703"``
        for MacOS version 10.7.3 (Lion)

    For BSD operating systems, it is the value of the ``OpSysName``
    attribute concatenated with the string version of the
    ``OpSysMajorVer`` attribute:

     ``"FREEBSD7"``
        for FreeBSD version 7
     ``"FREEBSD8"``
        for FreeBSD version 8

    For Windows operating systems, it is the value of the ``OpSys``
    attribute concatenated with the string version of the
    ``OpSysMajorVer`` attribute:

     ``"WINDOWS500"``
        for Windows 2000
     ``"WINDOWS501"``
        for Windows XP
     ``"WINDOWS502"``
        for Windows Server 2003
     ``"WINDOWS600"``
        for Windows Vista
     ``"WINDOWS601"``
        for Windows 7

:classad-attribute:`OpSysLegacy`
    A string that holds the long-standing values for the ``OpSys``
    attribute. Currently supported operating systems have the following
    string definitions:

     ``"LINUX"``
        for LINUX 2.0.x, LINUX 2.2.x, LINUX 2.4.x, LINUX 2.6.x, or LINUX
        3.10.0 kernel systems, as well as Scientific Linux, Ubuntu
        versions 14.04, and Debian 7 and 8
     ``"OSX"``
        for Darwin
     ``"FREEBSD7"``
        for FreeBSD version 7
     ``"FREEBSD8"``
        for FreeBSD version 8
     ``"WINDOWS"``
        for all versions of Windows

:classad-attribute:`OpSysLongName`
    A string giving a full description of the operating system. For
    Linux platforms, this is generally the string taken from
    ``/etc/hosts``, with extra characters stripped off Debian versions.

     ``"Red Hat Enterprise Linux Server release 6.2 (Santiago)"``
        for RedHat Linux version 6
     ``"Red Hat Enterprise Linux Server release 7.0 (Maipo)"``
        for RedHat Linux version 7.0
     ``"Ubuntu 14.04.1 LTS"``
        for Ubuntu 14.04 point release 1
     ``"Debian GNU/Linux 8"``
        for Debian 8.0 (jessie)
     ``"Fedora release 16 (Verne)"``
        for Fedora Linux version 16
     ``"MacOSX 7.3"``
        for MacOS version 10.7.3 (Lion)
     ``"FreeBSD8.2-RELEASE-p3"``
        for FreeBSD version 8
     ``"Windows XP SP3"``
        for Windows XP
     ``"Windows 7 SP2"``
        for Windows 7

:classad-attribute:`OpSysMajorVer`
    An integer value representing the major version of the operating
    system.

     ``5``
        for RedHat Linux version 5 and derived platforms such as
        Scientific Linux
     ``6``
        for RedHat Linux version 6 and derived platforms such as
        Scientific Linux
     ``7``
        for RedHat Linux version 7
     ``14``
        for Ubuntu 14.04
     ``7``
        for Debian 7
     ``8``
        for Debian 8
     ``16``
        for Fedora Linux version 16
     ``6``
        for MacOS version 10.6.5 (Snow Leopard)
     ``7``
        for MacOS version 10.7.3 (Lion)
     ``7``
        for FreeBSD version 7
     ``8``
        for FreeBSD version 8
     ``501``
        for Windows XP
     ``600``
        for Windows Vista
     ``601``
        for Windows 7

:classad-attribute:`OpSysName`
    A string containing a terse description of the operating system.

     ``"RedHat"``
        for RedHat Linux version 6 and 7
     ``"Fedora"``
        for Fedora Linux version 16
     ``"Ubuntu"``
        for Ubuntu versions 14.04
     ``"Debian"``
        for Debian versions 7 and 8
     ``"SnowLeopard"``
        for MacOS version 10.6.5 (Snow Leopard)
     ``"Lion"``
        for MacOS version 10.7.3 (Lion)
     ``"FREEBSD"``
        for FreeBSD version 7 or 8
     ``"WindowsXP"``
        for Windows XP
     ``"WindowsVista"``
        for Windows Vista
     ``"Windows7"``
        for Windows 7
     ``"SL"``
        for Scientific Linux
     ``"SLFermi"``
        for Fermi's Scientific Linux
     ``"SLCern"``
        for CERN's Scientific Linux

:classad-attribute:`OpSysShortName`
    A string containing a short name for the operating system.

     ``"RedHat"``
        for RedHat Linux version 5, 6 or 7
     ``"Fedora"``
        for Fedora Linux version 16
     ``"Debian"``
        for Debian Linux version 6 or 7 or 8
     ``"Ubuntu"``
        for Ubuntu versions 14.04
     ``"MacOSX"``
        for MacOS version 10.6.5 (Snow Leopard) or for MacOS version
        10.7.3 (Lion)
     ``"FreeBSD"``
        for FreeBSD version 7 or 8
     ``"XP"``
        for Windows XP
     ``"Vista"``
        for Windows Vista
     ``"7"``
        for Windows 7
     ``"SL"``
        for Scientific Linux
     ``"SLFermi"``
        for Fermi's Scientific Linux
     ``"SLCern"``
        for CERN's Scientific Linux

:classad-attribute:`OpSysVer`
    An integer value representing the operating system version number.

     ``700``
        for RedHat Linux version 7.0
     ``602``
        for RedHat Linux version 6.2
     ``1600``
        for Fedora Linux version 16.0
     ``1404``
        for Ubuntu 14.04
     ``700``
        for Debian 7.0
     ``800``
        for Debian 8.0
     ``704``
        for FreeBSD version 7.4
     ``802``
        for FreeBSD version 8.2
     ``605``
        for MacOS version 10.6.5 (Snow Leopard)
     ``703``
        for MacOS version 10.7.3 (Lion)
     ``500``
        for Windows 2000
     ``501``
        for Windows XP
     ``502``
        for Windows Server 2003
     ``600``
        for Windows Vista or Windows Server 2008
     ``601``
        for Windows 7 or Windows Server 2008

:classad-attribute:`PartitionableSlot`
    For SMP machines, a boolean value identifying that this slot may be
    partitioned.

:classad-attribute:`RecentJobPreemptions`
    The total number of jobs which have been preempted from this machine
    in the last twenty minutes.

:classad-attribute:`RecentJobRankPreemptions`
    The total number of times a running job has been preempted on this
    machine due to the machine's rank of jobs in the last twenty
    minutes.

:classad-attribute:`RecentJobStarts`
    The total number of jobs which have been started on this machine in
    the last twenty minutes.

:classad-attribute:`RecentJobUserPrioPreemptions`
    The total number of times a running job has been preempted on this
    machine based on a fair share allocation of the pool in the last
    twenty minutes.

:classad-attribute:`Requirements`
    A boolean, which when evaluated within the context of the machine
    ClassAd and a job ClassAd, must evaluate to TRUE before HTCondor
    will allow the job to use this machine.

:classad-attribute:`RetirementTimeRemaining` when the
    running job can be evicted. ``MaxJobRetirementTime`` is the
    expression of how much retirement time the machine offers to new
    jobs, whereas ``RetirementTimeRemaining`` is the negotiated amount
    of time remaining for the current running job. This may be less than
    the amount offered by the machine's ``MaxJobRetirementTime``
    expression, because the job may ask for less.

:classad-attribute:`SingularityVersion`
    A string containing the version of Singularity available, if the
    machine being advertised supports running jobs within a Singularity
    container (see ``HasSingularity``).

:classad-attribute:`SlotID`
    For SMP machines, the integer that identifies the slot. The value
    will be X for the slot with

    .. code-block:: condor-config

        name="slotX@full.hostname"

    For non-SMP machines with one slot, the value will be 1.

:classad-attribute:`SlotType`
    For SMP machines with partitionable slots, the partitionable slot
    will have this attribute set to ``"Partitionable"``, and all dynamic
    slots will have this attribute set to ``"Dynamic"``.

:classad-attribute:`SlotWeight`
    This specifies the weight of the slot when calculating usage,
    computing fair shares, and enforcing group quotas. For example,
    claiming a slot with ``SlotWeight = 2`` is equivalent to claiming
    two ``SlotWeight = 1`` slots. See the description of ``SlotWeight``
    in :ref:`admin-manual/configuration-macros:condor_startd configuration
    file macros`.

:classad-attribute:`StartdIpAddr`
    String with the IP and port address of the *condor_startd* daemon
    which is publishing this machine ClassAd. When using CCB,
    *condor_shared_port*, and/or an additional private network
    interface, that information will be included here as well.

:classad-attribute:`State`
    String which publishes the machine's HTCondor state. Can be:

     ``"Owner"``
        The machine owner is using the machine, and it is unavailable to
        HTCondor.
     ``"Unclaimed"``
        The machine is available to run HTCondor jobs, but a good match
        is either not available or not yet found.
     ``"Matched"``
        The HTCondor central manager has found a good match for this
        resource, but an HTCondor scheduler has not yet claimed it.
     ``"Claimed"``
        The machine is claimed by a remote *condor_schedd* and is
        probably running a job.
     ``"Preempting"``
        An HTCondor job is being preempted (possibly via checkpointing)
        in order to clear the machine for either a higher priority job
        or because the machine owner wants the machine back.
     ``"Drained"``
        This slot is not accepting jobs, because the machine is being
        drained.

:classad-attribute:`TargetType`
    Describes what type of ClassAd to match with. Always set to the
    string literal ``"Job"``, because machine ClassAds always want to be
    matched with jobs, and vice-versa.

:classad-attribute:`TotalCondorLoadAvg`
    The load average contributed by HTCondor summed across all slots on
    the machine, either from remote jobs or running benchmarks.

:classad-attribute:`TotalCpus`
    The number of CPUs (cores) that are on the machine. This is in
    contrast with ``Cpus``, which is the number of CPUs in the slot.

:classad-attribute:`TotalDisk`
    The quantity of disk space in KiB available across the machine (not
    the slot). For partitionable slots, where there is one partitionable
    slot per machine, this value will be the same as machine ClassAd
    attribute ``TotalSlotDisk``.

:classad-attribute:`TotalLoadAvg`
    A floating point number representing the current load average summed
    across all slots on the machine.

:classad-attribute:`TotalMachineDrainingBadput`
    The total job runtime in cpu-seconds that has been lost due to job
    evictions caused by draining since this *condor_startd* began
    executing. In this calculation, it is assumed that jobs are evicted
    without checkpointing.

:classad-attribute:`TotalMachineDrainingUnclaimedTime`
    The total machine-wide time in cpu-seconds that has not been used
    (i.e. not matched to a job submitter) due to draining since this
    *condor_startd* began executing.

:classad-attribute:`TotalMemory`
    The quantity of RAM in MiB available across the machine (not the
    slot). For partitionable slots, where there is one partitionable
    slot per machine, this value will be the same as machine ClassAd
    attribute ``TotalSlotMemory``.

:classad-attribute:`TotalSlotCpus`
    The number of CPUs (cores) in this slot. For static slots, this
    value will be the same as in ``Cpus``.

:classad-attribute:`TotalSlotDisk`
    The quantity of disk space in KiB given to this slot. For static
    slots, this value will be the same as machine ClassAd attribute
    ``Disk``. For partitionable slots, where there is one partitionable
    slot per machine, this value will be the same as machine ClassAd
    attribute ``TotalDisk``.

:classad-attribute:`TotalSlotMemory`
    The quantity of RAM in MiB given to this slot. For static slots,
    this value will be the same as machine ClassAd attribute ``Memory``.
    For partitionable slots, where there is one partitionable slot per
    machine, this value will be the same as machine ClassAd attribute
    ``TotalMemory``.

:classad-attribute:`TotalSlots`
    A sum of the static slots, partitionable slots, and dynamic slots on
    the machine at the current time.

:classad-attribute:`TotalTimeBackfillBusy`
    The number of seconds that this machine (slot) has accumulated
    within the backfill busy state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeBackfillIdle`
    The number of seconds that this machine (slot) has accumulated
    within the backfill idle state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeBackfillKilling`
    The number of seconds that this machine (slot) has accumulated
    within the backfill killing state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeClaimedBusy`
    The number of seconds that this machine (slot) has accumulated
    within the claimed busy state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeClaimedIdle`
    The number of seconds that this machine (slot) has accumulated
    within the claimed idle state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeClaimedRetiring`
    The number of seconds that this machine (slot) has accumulated
    within the claimed retiring state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeClaimedSuspended`
    The number of seconds that this machine (slot) has accumulated
    within the claimed suspended state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeMatchedIdle`
    The number of seconds that this machine (slot) has accumulated
    within the matched idle state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeOwnerIdle`
    The number of seconds that this machine (slot) has accumulated
    within the owner idle state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimePreemptingKilling`
    The number of seconds that this machine (slot) has accumulated
    within the preempting killing state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimePreemptingVacating`
    The number of seconds that this machine (slot) has accumulated
    within the preempting vacating state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeUnclaimedBenchmarking`
    The number of seconds that this machine (slot) has accumulated
    within the unclaimed benchmarking state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`TotalTimeUnclaimedIdle`
    The number of seconds that this machine (slot) has accumulated
    within the unclaimed idle state and activity pair since the
    *condor_startd* began executing. This attribute will only be
    defined if it has a value greater than 0.

:classad-attribute:`UidDomain`
    file entries, and therefore all have the same logins.

:classad-attribute:`VirtualMemory`
    The amount of currently available virtual memory (swap space)
    expressed in KiB. On Linux platforms, it is the sum of paging space
    and physical memory, which more accurately represents the virtual
    memory size of the machine.

:index:`VM_MAX_NUMBER`

:classad-attribute:`VM_AvailNum`
    The maximum number of vm universe jobs that can be started on this
    machine. This maximum is set by the configuration variable
    ``VM_MAX_NUMBER``.

:classad-attribute:`VM_Guest_Mem`
    An attribute defined if a vm universe job is running on this slot.
    Defined by the amount of memory in use by the virtual machine, given
    in Mbytes.

:index:`VM_MEMORY`

:classad-attribute:`VM_Memory`
    Gives the amount of memory available for starting additional VM jobs
    on this machine, given in Mbytes. The maximum value is set by the
    configuration variable ``VM_MEMORY``.
    
:classad-attribute:`VM_Networking`
    A boolean value indicating whether networking is allowed for virtual
    machines on this machine.

:classad-attribute:`VM_Type`
    The type of virtual machine software that can run on this machine.
    The value is set by the configuration variable ``VM_TYPE``

:classad-attribute:`VMOfflineReason`
    The reason the VM universe went offline (usually because a VM
    universe job failed to launch).

:classad-attribute:`VMOfflineTime`
    The time that the VM universe went offline.

:classad-attribute:`WindowsBuildNumber`
    An integer, extracted from the platform type, representing a build
    number for a Windows operating system. This attribute only exists on
    Windows machines.

:classad-attribute:`WindowsMajorVersion`
    An integer, extracted from the platform type, representing a major
    version number (currently 5 or 6) for a Windows operating system.
    This attribute only exists on Windows machines.

:classad-attribute:`WindowsMinorVersion`
    An integer, extracted from the platform type, representing a minor
    version number (currently 0, 1, or 2) for a Windows operating
    system. This attribute only exists on Windows machines.


In addition, there are a few attributes that are automatically inserted
into the machine ClassAd whenever a resource is in the Claimed state:

:classad-attribute:`ClientMachine`
    The host name of the machine that has claimed this resource

:index:`GROUP_AUTOREGROUP`

:classad-attribute:`RemoteAutoregroup`
    A boolean attribute which is ``True`` if this resource was claimed
    via negotiation when the configuration variable
    ``GROUP_AUTOREGROUP`` is ``True``. It is ``False`` otherwise.

:classad-attribute:`RemoteGroup`
    The accounting group name corresponding to the submitter that
    claimed this resource.

:classad-attribute:`RemoteNegotiatingGroup`
    The accounting group name under which this resource negotiated when
    it was claimed. This attribute will frequently be the same as
    attribute ``RemoteGroup``, but it may differ in cases such as when
    configuration variable ``GROUP_AUTOREGROUP`` :index:`GROUP_AUTOREGROUP` 
    is ``True``, in which case it will
    have the name of the root group, identified as ``<none>``.

:classad-attribute:`RemoteOwner`
    The name of the user who originally claimed this resource.

:classad-attribute:`RemoteUser`
    The name of the user who is currently using this resource. In
    general, this will always be the same as the ``RemoteOwner``, but in
    some cases, a resource can be claimed by one entity that hands off
    the resource to another entity which uses it. In that case,
    ``RemoteUser`` would hold the name of the entity currently using the
    resource, while ``RemoteOwner`` would hold the name of the entity
    that claimed the resource.

:classad-attribute:`PreemptingOwner`
    The name of the user who is preempting the job that is currently
    running on this resource.

:classad-attribute:`PreemptingUser`
    The name of the user who is preempting the job that is currently
    running on this resource. The relationship between
    ``PreemptingUser`` and ``PreemptingOwner`` is the same as the
    relationship between ``RemoteUser`` and ``RemoteOwner``.

:classad-attribute:`PreemptingRank`
    A float which represents this machine owner's affinity for running
    the HTCondor job which is waiting for the current job to finish or
    be preempted. If not currently hosting an HTCondor job,
    ``PreemptingRank`` is undefined. When a machine is claimed and there
    is already a job running, the attribute's value is computed by
    evaluating the machine's ``Rank`` expression with respect to the
    preempting job's ClassAd.

:classad-attribute:`TotalClaimRunTime`
    A running total of the amount of time (in seconds) that all jobs
    (under the same claim) ran (have spent in the Claimed/Busy state).

:classad-attribute:`TotalClaimSuspendTime`
    A running total of the amount of time (in seconds) that all jobs
    (under the same claim) have been suspended (in the Claimed/Suspended
    state).

:classad-attribute:`TotalJobRunTime`
    A running total of the amount of time (in seconds) that a single job
    ran (has spent in the Claimed/Busy state).

:classad-attribute:`TotalJobSuspendTime`
    A running total of the amount of time (in seconds) that a single job
    has been suspended (in the Claimed/Suspended state).


There are a few attributes that are only inserted into the machine
ClassAd if a job is currently executing. If the resource is claimed but
no job are running, none of these attributes will be defined.

:classad-attribute:`JobId`
    The job's identifier (for example, 152.3), as seen from *condor_q*
    on the submitting machine.

:classad-attribute:`JobStart`
    The time stamp in integer seconds of when the job began executing,
    since the Unix epoch (00:00:00 UTC, Jan 1, 1970). For idle machines,
    the value is ``UNDEFINED``.

:classad-attribute:`LastPeriodicCheckpoint`
    If the job has performed a periodic checkpoint, this attribute will
    be defined and will hold the time stamp of when the last periodic
    checkpoint was begun. If the job has yet to perform a periodic
    checkpoint, or cannot checkpoint at all, the
    ``LastPeriodicCheckpoint`` attribute will not be defined.


:index:`offline ClassAd`

There are a few attributes that are applicable to machines that are
offline, that is, hibernating.

:classad-attribute:`MachineLastMatchTime`
    The Unix epoch time when this offline ClassAd would have been
    matched to a job, if the machine were online. In addition, the slot1
    ClassAd of a multi-slot machine will have
    ``slot<X>_MachineLastMatchTime`` defined, where ``<X>`` is replaced
    by the slot id of each of the slots with ``MachineLastMatchTime``
    defined.

:classad-attribute:`Offline`
    A boolean value, that when ``True``, indicates this machine is in an
    offline state in the *condor_collector*. Such ClassAds are stored
    persistently, such that they will continue to exist after the
    *condor_collector* restarts.

:classad-attribute:`Unhibernate`
    A boolean expression that specifies when a hibernating machine
    should be woken up, for example, by *condor_rooster*.


For machines with user-defined or custom resource specifications,
including GPUs, the following attributes will be in the ClassAd for each
slot. In the name of the attribute, ``<name>`` is substituted with the
configured name given to the resource.

:classad-attribute:`Assigned<name>`
    A space separated list that identifies which of these resources are
    currently assigned to slots.

:classad-attribute:`Offline<name>`
    A space separated list that indicates which of these resources is
    unavailable for match making.

:classad-attribute:`Total<name>`
    An integer quantity of the total number of these resources.


For machines with custom resource specifications that include GPUs, the
following attributes may be in the ClassAd for each slot, depending on
the value of configuration variable ``MACHINE_RESOURCE_INVENTORY_GPUs``
:index:`MACHINE_RESOURCE_INVENTORY_GPUs` and what GPUs are
detected. In the name of the attribute, ``<name>`` is substituted with
the *prefix string* assigned for the GPU.

:classad-attribute:`<name>BoardTempC`
    For NVIDIA devices, a dynamic attribute representing the temperature
    in Celsius of the board containing the GPU.

:classad-attribute:`<name>Capability`
    The CUDA-defined capability for the GPU.

:classad-attribute:`<name>ClockMhz`
    For CUDA or Open CL devices, the integer clocking speed of the GPU
    in MHz.

:classad-attribute:`<name>ComputeUnits`
    For CUDA or Open CL devices, the integer number of compute units per
    GPU.

:classad-attribute:`<name>CoresPerCU`
    For CUDA devices, the integer number of cores per compute unit.

:classad-attribute:`<name>DeviceName`
    For CUDA or Open CL devices, a string representing the
    manufacturer's proprietary device name.

:classad-attribute:`<name>DieTempC`
    For NVIDIA devices, a dynamic attribute representing the temperature
    in Celsius of the GPU die.

:classad-attribute:`<name>DriverVersion`
    For CUDA devices, a string representing the manufacturer's driver
    version.

:classad-attribute:`<name>ECCEnabled`
    For CUDA or Open CL devices, a boolean value representing whether
    error correction is enabled.

:classad-attribute:`<name>EccErrorsDoubleBit`
    For NVIDIA devices, a count of the number of double bit errors
    detected for this GPU.

:classad-attribute:`<name>EccErrorsSingleBit`
    For NVIDIA devices, a count of the number of single bit errors
    detected for this GPU.

:classad-attribute:`<name>FanSpeedPct`
    For NVIDIA devices, a value between 0 and 100 (inclusive), used to
    represent the level of fan operation as percentage of full fan
    speed.

:classad-attribute:`<name>GlobalMemoryMb`
    For CUDA or Open CL devices, the quantity of memory in Mbytes in
    this GPU.

:classad-attribute:`<name>OpenCLVersion`
    For Open CL devices, a string representing the manufacturer's
    version number.

:classad-attribute:`<name>RuntimeVersion`
    For CUDA devices, a string representing the manufacturer's version
    number.


The following attributes are advertised for a machine in which
partitionable slot preemption is enabled.

:classad-attribute:`ChildAccountingGroup`
    A ClassAd list containing the values of the ``AccountingGroup``
    attribute for each dynamic slot of the partitionable slot.

:classad-attribute:`ChildActivity`
    A ClassAd list containing the values of the ``Activity`` attribute
    for each dynamic slot of the partitionable slot.

:classad-attribute:`ChildCpus`
    A ClassAd list containing the values of the ``Cpus`` attribute for
    each dynamic slot of the partitionable slot.

:classad-attribute:`ChildCurrentRank`
    A ClassAd list containing the values of the ``CurrentRank``
    attribute for each dynamic slot of the partitionable slot.

:classad-attribute:`ChildEnteredCurrentState`
    A ClassAd list containing the values of the ``EnteredCurrentState``
    attribute for each dynamic slot of the partitionable slot.

:classad-attribute:`ChildMemory`
    A ClassAd list containing the values of the ``Memory`` attribute for
    each dynamic slot of the partitionable slot.

:classad-attribute:`ChildName`
    A ClassAd list containing the values of the ``Name`` attribute for
    each dynamic slot of the partitionable slot.

:classad-attribute:`ChildRemoteOwner`
    A ClassAd list containing the values of the ``RemoteOwner``
    attribute for each dynamic slot of the partitionable slot.

:classad-attribute:`ChildRemoteUser`
    A ClassAd list containing the values of the ``RemoteUser`` attribute
    for each dynamic slot of the partitionable slot.

:classad-attribute:`ChildRetirementTimeRemaining`
    A ClassAd list containing the values of the
    ``RetirementTimeRemaining`` attribute for each dynamic slot of the
    partitionable slot.

:classad-attribute:`ChildState`
    A ClassAd list containing the values of the ``State`` attribute for
    each dynamic slot of the partitionable slot.

:classad-attribute:`PslotRollupInformation`
    A boolean value set to ``True`` in both the partitionable and
    dynamic slots, when configuration variable
    ``ADVERTISE_PSLOT_ROLLUP_INFORMATION`` is ``True``, such that the
    *condor_negotiator* knows when partitionable slot preemption is
    possible and can directly preempt a dynamic slot when appropriate.

The single attribute, ``CurrentTime``, is defined by the
ClassAd environment.

:classad-attribute:`CurrentTime`
    Evaluates to the the number of integer seconds since the Unix epoch
    (00:00:00 UTC, Jan 1, 1970).


.. _CommonCloudAttributes:

.. rubric:: Common Cloud Attributes

The following attributes are advertised when
``use feature:CommonCloudAttributesGoogle`` or
``use feature:CommonCloudAttributesAWS`` is enabled.  All values are strings.

:classad-attribute:`CloudImage`
    Identifies the VM image.  ("image" or "AMI ID")

:classad-attribute:`CloudVMType`
    Identifies the type of resource allocated.  ("machine type" or "instance type")

:classad-attribute:`CloudRegion`
    Identifies the geographic area in which the instance is running.

:classad-attribute:`CloudZone`
    Identifies a specific ("availability") zone within the region.

:classad-attribute:`CloudProvider`
    Presently, either ``"Google"`` or ``"AWS"``.

:classad-attribute:`CloudPlatform`
    Presently, either ``"GCE"`` or ``"EC2"``.

:classad-attribute:`CloudInstanceID`
    The instance's identifier with its provider (on its platform).

:classad-attribute:`CloudInterruptible`
    ``"True"`` if the instance, and ``"False"`` otherwise.
