DaemonMaster ClassAd Attributes
===============================

:classad-attribute-def:`CondorVersion`
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:classad-attribute-def:`DaemonStartTime`
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`DaemonLastReconfigTime`
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`LinuxCapabilities`
    A hexadecimal formatted string that holds all set effective Linux
    capabilities bit mask. This hex string can be decoded using ``capsh``.
    Only exists if running on a Linux OS.
 
:classad-attribute-def:`Machine`
    A string with the machine's fully qualified host name.

:classad-attribute-def:`MasterIpAddr`
    String with the IP and port address of the :tool:`condor_master` daemon
    which is publishing this DaemonMaster ClassAd.

:classad-attribute-def:`MonitorSelfAge`
    The number of seconds that this daemon has been running.

:classad-attribute-def:`MonitorSelfCPUUsage`
    The fraction of recent CPU time utilized by this daemon.

:classad-attribute-def:`MonitorSelfImageSize`
    The amount of virtual memory consumed by this daemon in Kbytes.

:classad-attribute-def:`MonitorSelfRegisteredSocketCount`
    The current number of sockets registered by this daemon.

:classad-attribute-def:`MonitorSelfResidentSetSize`
    The amount of resident memory used by this daemon in Kbytes.

:classad-attribute-def:`MonitorSelfSecuritySessions`
    The number of open (cached) security sessions for this daemon.

:classad-attribute-def:`MonitorSelfTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which this daemon last checked
    and set the attributes with names that begin with the string
    ``MonitorSelf``.

:classad-attribute-def:`MyAddress`
    String with the IP and port address of the :tool:`condor_master` daemon
    which is publishing this ClassAd.

:classad-attribute-def:`MyCurrentTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the :tool:`condor_master`
    daemon last sent a ClassAd update to the *condor_collector*.

:classad-attribute-def:`Name`
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with a unique name. These
    names will be of the form "slot#@full.hostname", for example,
    "slot1@vulture.cs.wisc.edu", which signifies slot number 1 from
    vulture.cs.wisc.edu.

:classad-attribute-def:`PublicNetworkIpAddr`
    Description is not yet written.

:classad-attribute-def:`RealUid`
    The UID under which the :tool:`condor_master` is started.

:classad-attribute-def:`UpdateSequenceNumber`
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor_collector*. The *condor_collector* uses
    this value to sequence the updates it receives.
