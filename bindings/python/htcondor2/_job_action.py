import sys
import enum


class JobAction(enum.IntEnum):
    """
    An enumeration describing the actions that may be performed on a job
    in the queue.

    .. attribute:: Hold

        Put a job on hold, vacating a running job if necessary.  A job will
        stay in the hold state until explicitly acted upon by the admin or
        owner.

    .. attribute:: Release

        Release a job from the hold state, returning it to ``Idle``.

    .. attribute:: Suspend

        Suspend the processes of a running job (on Unix platforms, this
        triggers a ``SIGSTOP``).  The job's processes stay in memory but no
        longer get scheduled on the CPU.

    .. attribute:: Continue

        Continue a suspended jobs (on Unix, ``SIGCONT``).  The processes in a
        previously suspended job will be scheduled to get CPU time again.

    .. attribute:: Remove

        Remove a job from the Schedd's queue, cleaning it up first on the
        remote host (if running).  This requires the remote host to
        acknowledge it has successfully vacated the job, meaning ``Remove``
        may not be instantaneous.

    .. attribute:: RemoveX

        Immediately remove a job from the schedd queue, even if it means
        the job is left running on the remote resource.

    .. attribute:: Vacate

        Cause a running job to be killed on the remote resource and return
        to idle state.  With ``Vacate``, jobs may be given significant time
        to cleanly shut down.

    .. attribute:: VacateFast

        Vacate a running job as quickly as possible, without providing time
        for the job to cleanly terminate.
    """

    # JA_ERROR = 0
    Hold       = 1
    Release    = 2
    Remove     = 3
    RemoveX    = 4
    Vacate     = 5
    VacateFast = 6
    # JA_CLEAR_DIRTY_JOB_ATTRS
    Suspend    = 8
    Continue   = 9
