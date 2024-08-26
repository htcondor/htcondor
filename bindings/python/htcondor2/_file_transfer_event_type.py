import sys
import enum


class FileTransferEventType(enum.IntEnum):
    """
    An enumeration of file transfer event types.

    .. attribute:: IN_QUEUED
    .. attribute:: IN_STARTED
    .. attribute:: IN_FINISHED
    .. attribute:: OUT_QUEUED
    .. attribute:: OUT_STARTED
    .. attribute:: OUT_FINISHED
    """

    IN_QUEUED       = 1
    IN_STARTED      = 2
    IN_FINISHED     = 3
    OUT_QUEUED      = 4
    OUT_STARTED     = 5
    OUT_FINISHED    = 6
