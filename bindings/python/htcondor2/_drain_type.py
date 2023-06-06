import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class DrainType(enum.IntEnum):
    """
    Draining policies that can be set to a *condor_startd*.

    The values of the enumeration are:

    .. attribute:: Fast
    .. attribute:: Graceful
    .. attribute:: Quick
    """

    Graceful = 0
    Quick    = 10
    Fast     = 20
