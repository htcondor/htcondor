import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class Value(enum.IntEnum):
    """
    An enumeration of special ClassAd values.

    .. attribute:: Undefined

    .. attribute:: Error
    """

    Error        = 1<<0
    Undefined    = 1<<1
