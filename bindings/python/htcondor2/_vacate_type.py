import sys
import enum


class VacateType(enum.IntEnum):
    """
    An enumeration of vacate policies that can be sent to a *condor_startd*.

    The following explanations are reminders; the
    `actual policies <http://htcondor.readthedocs.io/en/latest/admin-manual/ep-policy-configuration.html#machine-states>`_
    are way more complicated.

    .. attribute:: Graceful

        Job are immediately soft-killed (generally meaning sent a ``SIGTERM``).
        The startd then waits a certain amount of time to allow jobs to shut
        down cleanly before the vacate policy becomes the following.

    .. attribute:: Fast

        Jobs are immediately hard-killed (generally meaning sent a ``SIGKILL``).
    """

    Graceful = 0
    Fast     = 1
