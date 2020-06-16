import warnings as _warnings;

def deprecate(message):
    """
    A decorator that marks a function (definition) as deprecated:

    @deprecate("deprecated")
    def function():
        pass

    Can also be used to mark a function as deprecated by wrapping it:

    function = deprecate("deprecated")(function)
    """
    def deprecated(func):
        def wrapper(*args, **kwargs):
            _warnings.warn(message, FutureWarning)
            return func(*args, **kwargs)

        return wrapper

    return deprecated

def deprecate_class(message, c):
    def class_name(*args, **kwargs):
        _warnings.warn(message, FutureWarning)
        return c(*args, **kwargs)

    return class_name


# FIXME: deprecate this enum
# from .htcondor import EntryType as _EntryType

from .htcondor import LogReader as _LogReader
LogReader = deprecate_class("deprecated", _LogReader)


# FIXME: deprecate this enum
# from .htcondor import LockType as _LockType

from .htcondor import EventIterator as _EventIterator
EventIterator = deprecate_class("deprecated", _EventIterator)

from .htcondor import FileLock as _FileLock
FileLock = deprecate_class("deprecated", _FileLock)

from .htcondor import lock
lock = deprecate("deprecated")(lock)

from .htcondor import read_events
read_events = deprecate("deprecated")(read_events)


from .htcondor import Negotiator as _Negotiator
Negotiator = deprecate_class("deprecated", _Negotiator)
