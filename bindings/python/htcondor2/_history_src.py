import enum

# NOTE: For internal use currently
#       Keep up to date with HistoryRecordSource
#       enum in src/condor_tools/history.cpp
class HistorySrc(enum.IntEnum):
    """
    An enumeration of history record sources.

    .. attribute:: ScheddJob

        Search final job history in :macro:`HISTORY`

    .. attribute:: StartdJob

        Search job history from Startd's :macro:`HISTORY`

    .. attribute:: JobEpoch

        Search historical records from :macro:`JOB_EPOCH_HISTORY`.
        Currently contains job epoch, transfer, and spawn records.

    .. attribute:: Daemon

        Search for historical daemon records from ``<SUBSYS>_DAEMON_HISTORY``.

        .. note::

            Requires history queries AdType to be set to the desired
            daemon subsystem.

    :meta private:
    """

    ScheddJob = 0
    StartdJob = 1
    JobEpoch = 2
    Daemon = 3