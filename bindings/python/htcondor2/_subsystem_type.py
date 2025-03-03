import sys
import enum

# The values in this file must match those in the enum SubsystemType
# in src/condor_utils/subsystem_info.h

class SubsystemType(enum.IntEnum):
    """
    An enumeration of known subsystem types.

    .. attribute:: Auto

    .. attribute:: Collector

    .. attribute:: Daemon

    .. attribute:: Dagman

    .. attribute:: GAHP

    .. attribute:: Job

    .. attribute:: Master

    .. attribute:: Negotiator

    .. attribute:: Schedd

    .. attribute:: Shadow

    .. attribute:: SharedPort

    .. attribute:: Startd

    .. attribute:: Starter

    .. attribute:: Submit

    .. attribute:: Tool
    """

    Master = 1
    Collector = 2
    Negotiator = 3
    Schedd = 4
    Shadow = 5
    Startd = 6
    Starter = 7
    GAHP = 8
    Dagman = 9
    SharedPort = 10
    Daemon = 11
    Tool = 12
    Submit = 13
    Job = 14
    Auto = 15
