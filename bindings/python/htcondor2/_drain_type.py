import sys
import enum


class DrainType(enum.IntEnum):
    """
    An enumeration of draining policies that can be set to a *condor_startd*.

    The following explanations are reminders; the
    `actual policies <http://htcondor.readthedocs.io/en/latest/admin-manual/ep-policy-configuration.html#machine-states>`_
    are way more complicated.

    .. attribute:: Graceful

        The default; jobs are allowed to continue running until they reach
        their promised `max job retirement time <http://htcondor.readthedocs.io/en/latest/admin-manual/configuration-macros.html#MAXJOBRETIREMENTTIME>`_.  To
        reduce idle time, all jobs may continue to run until the most distant
        such deadline, at which point the draining policy effectively becomes
        the following.

    .. attribute:: Quick

        Job are immediately soft-killed (generally meaning sent a ``SIGTERM``).
        The startd then waits a certain amount of time to allow jobs to shut
        down cleanly before the draining policy becomes the following.

    .. attribute:: Fast

        Jobs are immediately hard-killed (generally meaning sent a ``SIGKILL``).
    """

    Graceful = 0
    Quick    = 10
    Fast     = 20
