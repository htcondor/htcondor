import sys
import enum


class AdType(enum.IntEnum):
    """
    An enumeration of known ad types.

    .. attribute:: Any

        Matches any type of ad.

    .. attribute:: Generic

    .. attribute:: Slot

        Slot ads, produced by the *condor_startd* daemon.  Used for
        matchmaking.

    .. attribute:: StartDaemon

        Ads about the *condor_startd* daemon itself.  Used for
        location requests and monitoring.

    .. attribute:: Startd

        Either type of ad produced by the *condor_startd* daemon.  Used
        for backwards compatibility.  Use :const:`AdType.Slot` or
        :const:`AdType.StartDaemon` to be explicit.

    .. attribute:: StartdPrivate

    .. attribute:: Schedd

    .. attribute:: Master

    .. attribute:: Collector

    .. attribute:: Negotiator

    .. attribute:: Submitter

    .. attribute:: Grid

    .. attribute:: HAD

    .. attribute:: License

    .. attribute:: Credd

    .. attribute:: Placementd

    .. attribute:: Defrag

    .. attribute:: Accounting

    """

    Startd = 0
    Schedd = 1
    Master = 2
    # Gateway = 3
    # CkptSrvr = 4
    StartdPrivate = 5
    # Submittor = 6
    Submitter = 6
    Collector = 7
    License = 8
    # Storage = 9
    Any = 10
    # bogus = 11
    # Cluster = 12
    Negotiator = 13
    HAD = 14
    Generic = 15
    Credd = 16
    # Database = 17
    # TT = 18
    Grid = 19
    Placementd = 20
    # LeaseManager = 21
    Defrag = 22
    Accounting = 23
    Slot = 24
    StartDaemon = 25

    @staticmethod
    def from_MyType(my_type : str) -> "AdType":
        AdTypeToMyType = {
            AdType.Startd: "Machine",
            AdType.Schedd: "Scheduler",
            AdType.Master: "DaemonMaster",
            AdType.StartdPrivate: "MachinePrivate",
            AdType.Submitter: "Submitter",
            AdType.Collector: "Collector",
            AdType.License: "License",
            AdType.Any: "Any",
            AdType.Negotiator: "Negotiator",
            AdType.HAD: "HAD",
            AdType.Generic: "Generic",
            AdType.Credd: "CredD",
            AdType.Grid: "Grid",
            AdType.Placementd: "PlacementD",
            AdType.Accounting: "Accounting",
            AdType.Slot: "Machine",
            AdType.StartDaemon: "StartD",
        }
        MyTypeToAdType = dict(map(reversed, AdTypeToMyType.items()))

        return MyTypeToAdType.get(my_type)
