import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class DaemonType(enum.IntEnum):
    """
    An enumeration of known daemon types.

    .. attribute:: none

        Any type of daemon; useful when specifying queries where all matching
        daemons should be returned.

    .. attribute:: Any

        Any type of daemon; useful when specifying queries where all matching
        daemons should be returned.

    .. attribute:: Master

    .. attribute:: Schedd

    .. attribute:: Startd

    .. attribute:: Collector

    .. attribute:: Negotiator

    .. attribute:: Credd

    .. attribute:: HAD

    .. attribute:: Generic

        Any other type of daemon.
    """

    none = 0
    Any = 1
    Master = 2
    Schedd = 3
    Startd = 4
    Collector = 5
    Negotiator = 6
    # Kbdd = 7
    # DAGMan = 8
    # ViewCollector = 9
    # Cluster = 10
    # Shadow = 11
    # Starter = 12
    Credd = 13
    # Stork = 14
    # Transferd = 15
    # LeaseManager = 16
    HAD = 17
    Generic = 18
