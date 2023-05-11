import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class AdType(enum.IntEnum):
    """
    An enumeration of known ad types.

    .. attribute:: Any
        Matches any type of ad.

    .. attribute:: Generic

    .. attribute:: Startd

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

    .. attribute:: Debug

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
    # XferService = 20
    # LeaseManager = 21
    # Defrag = 22
    Accounting = 23
