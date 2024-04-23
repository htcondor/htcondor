from typing import Optional

#
# Python > 3.8 on Windows doesn't use PATH to find DLLs any more,
# which breaks everything.
#
def _add_dll_dir():
    import platform as _platform

    if _platform.system() in ["Windows"]:
        import sys as _sys
        import os as _os
        from os import path as _path

        if _sys.version_info >= (3,8):
            bin_path = _path.realpath(_path.join(__file__, r'..\..\..\..\bin'))
            return _os.add_dll_directory(bin_path)
    else:
        from contextlib import contextmanager as _contextmanager
        @_contextmanager
        def _noop():
            yield None

        return _noop()


with _add_dll_dir():

    # Modules.
    from ._common_imports import classad

    # Module variables.
    from ._param import _Param
    param = _Param()

    # Module functions.
    from .htcondor2_impl import _version as version
    from .htcondor2_impl import _platform as platform
    from .htcondor2_impl import _set_subsystem as set_subsystem
    from .htcondor2_impl import _reload_config as reload_config

    from ._loose_functions import send_command
    from ._loose_functions import send_alive

    from .htcondor2_impl import _enable_debug as enable_debug
    from .htcondor2_impl import _enable_log as enable_log
    from ._logging import _log as log

    # Enumerations.
    from ._subsystem_type import SubsystemType
    from ._daemon_type import DaemonType
    from ._ad_type import AdType
    from ._drain_type import DrainType
    from ._completion_type import CompletionType
    from ._cred_type import CredType
    from ._query_opt import QueryOpt
    from ._job_action import JobAction
    from ._transaction_flag import TransactionFlag
    from ._job_event_type import JobEventType
    from ._file_transfer_event_type import FileTransferEventType
    from ._log_level import LogLevel
    from ._daemon_command import DaemonCommand
    from ._submit_method import SubmitMethod

    # Classes.
    from ._collector import Collector
    from ._negotiator import Negotiator
    from ._startd import Startd
    from ._credd import Credd
    from ._cred_check import CredCheck
    from ._schedd import Schedd
    from ._submit import Submit
    from ._submit_result import SubmitResult
    from ._submit_result import _SpooledProcAdList
    from ._job_event import JobEvent
    from ._job_event_log import JobEventLog
    from ._job_status import JobStatus
    from ._remote_param import RemoteParam

    # Additional aliases for compatibility with the `htcondor` module.
    from ._daemon_type import DaemonType as DaemonTypes
    from ._ad_type import AdType as AdTypes
    from ._cred_type import CredType as CredTypes
    from ._drain_type import DrainType as DrainTypes
    from ._transaction_flag import TransactionFlag as TransactionFlags
    from ._query_opt import QueryOpt as QueryOpts
    from ._daemon_command import DaemonCommand as DaemonCommands
