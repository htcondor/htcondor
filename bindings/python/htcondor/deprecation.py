import warnings as _warnings
import functools as _functools


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


def deprecate_method(message, cls, method=None):
    if method is None:
        method = cls.__init__

    # assigned protects us from accessing attributes of the function object
    # that may not exist inside functools.wraps, which can fail on 2.7
    # Compare https://github.com/python/cpython/blob/2.7/Lib/functools.py#L33 to
    #         https://github.com/python/cpython/blob/3.7/Lib/functools.py#L53
    assigned = (a for a in _functools.WRAPPER_ASSIGNMENTS if hasattr(method, a))

    @_functools.wraps(method, assigned)
    def wrapper(*args, **kwargs):
        _warnings.warn(message, FutureWarning)
        return method(*args, **kwargs)

    setattr(cls, method.__name__, wrapper)
    return cls


def deprecate_class(message, cls):
    return deprecate_method(message, cls, None)


# FIXME: deprecate this enum
# from .htcondor import EntryType as _EntryType

from .htcondor import LogReader as _LogReader

LogReader = deprecate_class("deprecated", _LogReader)


# FIXME: deprecate this enum
# from .htcondor import LockType as _LockType

# This class can't be constructed from Python, so maybe we don't need this?
from .htcondor import EventIterator as _EventIterator

EventIterator = deprecate_class("deprecated", _EventIterator)

# This class can't be constructed from Python, so maybe we don't need this?
from .htcondor import FileLock as _FileLock

FileLock = deprecate_class("deprecated", _FileLock)

from .htcondor import lock

lock = deprecate("deprecated")(lock)

from .htcondor import read_events

read_events = deprecate("Deprecated; use JobEventLog, instead.")(read_events)


from .htcondor import Negotiator as _Negotiator

Negotiator = deprecate_class("deprecated", _Negotiator)
