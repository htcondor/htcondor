from typing import Union

from ._log_level import LogLevel
from .htcondor2_impl import _py_dprintf

def _log(level : Union[LogLevel, int], message : str):
    """
    Log a message using the HTCondor logging subsystem.

    :param level:  Specify multiple :class:`LogLevel` values using bitwise-or.
    :param message:  The message to log.
    """
    if not (isinstance(level, LogLevel) or isinstance(level, int)):
        raise TypeError("level must a LogLevel or a bitwise-or of such")
    _py_dprintf(int(level), message + "\n")
