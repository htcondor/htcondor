import sys
import enum


class JobStatus(enum.IntEnum):
    """
    An enumeration of HTCondor job status values.

    .. attribute:: IDLE

    .. attribute:: RUNNING

    .. attribute:: REMOVED

    .. attribute:: COMPLETED

    .. attribute:: HELD

    .. attribute:: TRANSFERRING_OUTPUT

    .. attribute:: SUSPENDED
    """

    IDLE = 1
    RUNNING = 2
    REMOVED = 3
    COMPLETED = 4
    HELD = 5
    TRANSFERRING_OUTPUT = 6
    SUSPENDED = 7
