import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class CompletionType(enum.IntEnum):
    """
    .. attribute:: Nothing
    .. attribute:: Resume

    Added in 8.9.11:

    .. attribute:: Exit
    .. attribute:: Restart

    Added in 9.11.0:

    .. attribute:: Reconfig
    """

    Nothing  = 0
    Resume   = 1
    Exit     = 2
    Restart  = 3
    Reconfig = 4
