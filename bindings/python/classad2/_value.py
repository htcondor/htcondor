import sys
import enum


class Value(enum.IntEnum):
    """
    An enumeration of special ClassAd values.

    .. attribute:: Undefined

    .. attribute:: Error
    """

    Error        = 1<<0
    Undefined    = 1<<1
