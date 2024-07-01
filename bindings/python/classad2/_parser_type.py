import sys
import enum


class ParserType(enum.IntEnum):
    """
    An enumeration of ClassAd parser types.

    .. attribute:: Auto

        Assuming that the input is a sequence of serialized ClassAds
        in either the "new" or the "old" format, determine which.

    .. attribute:: Old

        Assume that the input is a sequence of ClassAds serialized in
        the "old" (or "long") format.

    .. attribute:: New

        Assume that the input is a sequence of ClassAds serialized in
        the "new" format.
    """

    Auto        = -1
    Old         = 0
    XML         = 1
    JSON        = 2
    New         = 3
    FileAuto    = 4
