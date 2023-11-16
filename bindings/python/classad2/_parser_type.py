import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class ParserType(enum.IntEnum):
    """
    An enumeration of ClassAd parser types.

    .. attribute:: Auto

        Assuming that the input is a sequence of serialized ClassAds,
        determine if the first ad is in the "old" or "new" format and
        parse all ads accordingly.

    .. attribute:: Old

        Assume that the input is a sequence of ClassAds serialized in
        the "old" (or "long") format.

    .. attribute:: New

        Assume that the input is a sequence of ClassAds serialzied in
        the "new" format.
    """

    Auto        = -1
    Old         = 0
    XML         = 1
    JSON        = 2
    New         = 3
    FileAuto    = 4
