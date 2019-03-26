      

DaemonMaster ClassAd Attributes
===============================

:index:`DaemonMaster attributes;ClassAd<single: DaemonMaster attributes;ClassAd>`
:index:`CkptServer;ClassAd DaemonMaster attribute<single: CkptServer;ClassAd DaemonMaster attribute>`

 ``CkptServer``:
    A string with with the fully qualified host name of the machine
    running a checkpoint server.
    :index:`CondorVersion;ClassAd DaemonMaster attribute<single: CondorVersion;ClassAd DaemonMaster attribute>`
 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
    :index:`DaemonStartTime;ClassAd DaemonMaster attribute<single: DaemonStartTime;ClassAd DaemonMaster attribute>`
 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`DaemonLastReconfigTime;ClassAd DaemonMaster attribute<single: DaemonLastReconfigTime;ClassAd DaemonMaster attribute>`
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`Machine;ClassAd DaemonMaster attribute<single: Machine;ClassAd DaemonMaster attribute>`
 ``Machine``:
    A string with the machine’s fully qualified host name.
    :index:`MasterIpAddr;ClassAd DaemonMaster attribute<single: MasterIpAddr;ClassAd DaemonMaster attribute>`
 ``MasterIpAddr``:
    String with the IP and port address of the *condor\_master* daemon
    which is publishing this DaemonMaster ClassAd.
    :index:`MonitorSelfAge;ClassAd DaemonMaster attribute<single: MonitorSelfAge;ClassAd DaemonMaster attribute>`
 ``MonitorSelfAge``:
    The number of seconds that this daemon has been running.
    :index:`MonitorSelfCPUUsage;ClassAd DaemonMaster attribute<single: MonitorSelfCPUUsage;ClassAd DaemonMaster attribute>`
 ``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.
    :index:`MonitorSelfImageSize;ClassAd DaemonMaster attribute<single: MonitorSelfImageSize;ClassAd DaemonMaster attribute>`
 ``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in Kbytes.
    :index:`MonitorSelfRegisteredSocketCount;ClassAd DaemonMaster attribute<single: MonitorSelfRegisteredSocketCount;ClassAd DaemonMaster attribute>`
 ``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.
    :index:`MonitorSelfResidentSetSize;ClassAd DaemonMaster attribute<single: MonitorSelfResidentSetSize;ClassAd DaemonMaster attribute>`
 ``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in Kbytes.
    :index:`MonitorSelfSecuritySessions;ClassAd DaemonMaster attribute<single: MonitorSelfSecuritySessions;ClassAd DaemonMaster attribute>`
 ``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.
    :index:`MonitorSelfTime;ClassAd DaemonMaster attribute<single: MonitorSelfTime;ClassAd DaemonMaster attribute>`
 ``MonitorSelfTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.
    :index:`MyAddress;ClassAd DaemonMaster attribute<single: MyAddress;ClassAd DaemonMaster attribute>`
 ``MyAddress``:
    String with the IP and port address of the *condor\_master* daemon
    which is publishing this ClassAd.
    :index:`MyCurrentTime;ClassAd DaemonMaster attribute<single: MyCurrentTime;ClassAd DaemonMaster attribute>`
 ``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor\_master*
    daemon last sent a ClassAd update to the *condor\_collector*.
    :index:`Name;ClassAd DaemonMaster attribute<single: Name;ClassAd DaemonMaster attribute>`
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form “slot#@full.hostname”, for example,
    “slot1@vulture.cs.wisc.edu”, which signifies slot number 1 from
    vulture.cs.wisc.edu.
    :index:`PublicNetworkIpAddr;ClassAd DaemonMaster attribute<single: PublicNetworkIpAddr;ClassAd DaemonMaster attribute>`
 ``PublicNetworkIpAddr``:
    Description is not yet written.
    :index:`RealUid;ClassAd DaemonMaster attribute<single: RealUid;ClassAd DaemonMaster attribute>`
 ``RealUid``:
    The UID under which the *condor\_master* is started.
    :index:`UpdateSequenceNumber;ClassAd DaemonMaster attribute<single: UpdateSequenceNumber;ClassAd DaemonMaster attribute>`
 ``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor\_collector*. The *condor\_collector* uses
    this value to sequence the updates it receives.

      
