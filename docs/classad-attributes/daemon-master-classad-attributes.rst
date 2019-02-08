      

DaemonMaster ClassAd Attributes
===============================

 ``CkptServer``:
    A string with with the fully qualified host name of the machine
    running a checkpoint server.
 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
 ``Machine``:
    A string with the machine’s fully qualified host name.
 ``MasterIpAddr``:
    String with the IP and port address of the *condor\_master* daemon
    which is publishing this DaemonMaster ClassAd.
 ``MonitorSelfAge``:
    The number of seconds that this daemon has been running.
 ``MonitorSelfCPUUsage``:
    The fraction of recent CPU time utilized by this daemon.
 ``MonitorSelfImageSize``:
    The amount of virtual memory consumed by this daemon in Kbytes.
 ``MonitorSelfRegisteredSocketCount``:
    The current number of sockets registered by this daemon.
 ``MonitorSelfResidentSetSize``:
    The amount of resident memory used by this daemon in Kbytes.
 ``MonitorSelfSecuritySessions``:
    The number of open (cached) security sessions for this daemon.
 ``MonitorSelfTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.
 ``MyAddress``:
    String with the IP and port address of the *condor\_master* daemon
    which is publishing this ClassAd.
 ``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor\_master*
    daemon last sent a ClassAd update to the *condor\_collector*.
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form “slot#@full.hostname”, for example,
    “slot1@vulture.cs.wisc.edu”, which signifies slot number 1 from
    vulture.cs.wisc.edu.
 ``PublicNetworkIpAddr``:
    Description is not yet written.
 ``RealUid``:
    The UID under which the *condor\_master* is started.
 ``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor\_collector*. The *condor\_collector* uses
    this value to sequence the updates it receives.

      
