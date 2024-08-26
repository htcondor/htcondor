import sys
import enum


class DaemonType(enum.IntEnum):
    """
    An enumeration of known daemon types.

    .. attribute:: Any

        Any type of daemon; useful when specifying queries where all matching
        daemons should be returned.

    .. attribute:: Master

        A *condor_master* daemon.

    .. attribute:: Schedd

        A *condor_schedd* daemon.

    .. attribute:: Startd

        A *condor_startd* daemon.

    .. attribute:: Collector

        A *condor_collector* daemon.

    .. attribute:: Negotiator

        A *condor_negotiator* daemon.

    .. attribute:: Credd

        A *condor_credd* daemon.

    .. attribute:: HAD

        A *condor_had* daemon.

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
