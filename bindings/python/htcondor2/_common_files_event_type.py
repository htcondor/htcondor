import sys
import enum


class CommonFilesEventType(enum.IntEnum):
    """
    An enumeration of file transfer event types.

    .. attribute:: TransferQueued
    .. attribute:: TransferStarted
    .. attribute:: TransferFinished
    .. attribute:: WaitStarted
    .. attribute:: WaitFinished
    """

    TransferQueued   = 1
    TransferStarted  = 2
    TransferFinished = 3
    WaitStarted      = 4
    WaitFinished     = 5
