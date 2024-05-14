import sys
import enum


class LogLevel(enum.IntEnum):
    """
    An enumeration of "log levels" to use with :func:`log`.  Some
    values of this enumeration select types of log messages; others
    control the format of the header.  See
    `the documentation <https://htcondor.readthedocs.io/en/latest/admin-manual/configuration-macros.html#%3CSUBSYS%3E_DEBUG>`_.

    .. attribute:: Always
    .. attribute:: Audit
    .. attribute:: Config
    .. attribute:: DaemonCore
    .. attribute:: Error
    .. attribute:: FullDebug
    .. attribute:: Hostname
    .. attribute:: Job
    .. attribute:: Machine
    .. attribute:: Network
    .. attribute:: NoHeader
    .. attribute:: PID
    .. attribute:: Priv
    .. attribute:: Protocol
    .. attribute:: Security
    .. attribute:: Status
    .. attribute:: SubSecond
    .. attribute:: Terse
    .. attribute:: Timestamp
    .. attribute:: Verbose
    """

    Always = 0
    Error = 1
    Status = 2
    # ZKM = 3
    Job = 4
    Machine = 5
    Config = 6
    Protocol = 7
    Priv = 8
    DaemonCore = 9
    Verbose = 10
    Security = 11
    # Command = 12
    # Match = 13
    # If D_COMMAND and D_MATCH aren't on the list, why is D_NETWORK?
    Network = 14
    # Keyboard = 15
    # ProcFamily = 16
    # Idle = 17
    # Threads = 18
    # Accountant = 19
    # Syscalls = 20
    # Reserved1 = 21
    Hostname = 22
    # Why is D_PERF_TRACE missing?
    # PerfTrace = 23
    Audit = 24
    # Test = 25
    # Stats = 26
    # Materialize = 27
    # Bug = 28

    Terse = (0<<8)
    # Verbose = (1<<8)
    # Diagnostic = (2<<8)
    # Never = (3<<8)

    FullDebug = (1<<10)
    # ErrorAlso = (1<<11)
    # Except = (1<<12)
    # Backtrace = (1<<24)
    # Ident = (1<<25)
    SubSecond = (1<<26)
    Timestamp = (1<<27)
    PID = (1<<28)
    # FDs = (1<<29)
    # CAT = (1<<30)
    NoHeader = (1<<31)
