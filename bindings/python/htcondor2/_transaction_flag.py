import sys
import enum


class TransactionFlag(enum.IntEnum):
    """
    An enumeration of the flags affecting the characteristics of a transaction.

    .. attribute:: Default

        The default transaction is not any of the following.

    .. attribute:: NonDurable

        Non-durable transactions are changes that may be lost when the
        *condor_schedd* crashes.  ``NonDurable`` is used for performance,
        as it eliminates extra ``fsync()`` calls.

    .. attribute:: SetDirty

        This marks the changed ClassAds as dirty, causing an update
        notification to be sent to the *condor_shadow* and the
        *condor_gridmanager*, if they are managing the job.

    .. attribute:: ShouldLog

        Causes any changes to the job queue to be logged in the relevant job
        event log.
    """

    # None = 0
    Default = (0<<0)
    NonDurable = (1<<0)
    # SetAttribute_NoAck = (1<<1)
    SetDirty = (1<<2)
    ShouldLog = (1<<3)
    # SetAttribute_OnlyMyJobs = (1<<4);
    # SetAttribute_QueryOnly = (1<<5);
    # SetAttribute_unused = (1<<6)
    # SetAttribute_PostSubmitClusterChange = (1<<7)
