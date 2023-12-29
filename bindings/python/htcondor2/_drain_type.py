import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class DrainType(enum.IntEnum):
    """
    An enumeration of draining policies that can be set to a *condor_startd*.

    .. attribute:: Fast
    .. attribute:: Graceful
    .. attribute:: Quick
    """

    Graceful = 0
    Quick    = 10
    Fast     = 20
