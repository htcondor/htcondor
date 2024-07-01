import sys
import enum


class CompletionType(enum.IntEnum):
    """
    An enumeration of what a startd can do when it finishes draining.

    .. attribute:: Nothing

        The startd will remain in draining state.

    .. attribute:: Resume

        The startd will start accepting jobs again.

    .. attribute:: Exit

        The startd will exit, and ask the master not to restart it
        automatically.

    .. attribute:: Restart

        The startd will restart.  Many startd configuration changes
        require a restart to take effect.

    .. attribute:: Reconfig

        The startd will reconfig.  Some startd configuration changes,
        including the ``START`` expression, do not require a restart
        to take effect.
    """

    Nothing  = 0
    Resume   = 1
    Exit     = 2
    Restart  = 3
    Reconfig = 4
