import warnings
import textwrap

from . import htcondor
from ._wrap import wraps

DEPRECATION_WARNING_FORMAT_STR = ".. warning::\n\n    {}\n\n"


def add_deprecation_doc_warning(obj, message):
    try:
        obj.__doc__ = DEPRECATION_WARNING_FORMAT_STR.format(message) + textwrap.dedent(
            obj.__doc__ or ""
        )
    except AttributeError:
        # Ignore not being able to change docstrings in Python 2.
        pass


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
        # this implies we're deprecating a class, in which case the warning
        # will be applied to the class docstring via deprecate_class below
        if func.__name__ != "__init__":
            add_deprecation_doc_warning(func, message)

        @wraps(func)
        def wrapper(*args, **kwargs):
            warnings.warn(message, FutureWarning)
            return func(*args, **kwargs)

        return wrapper

    return deprecated


def deprecate_method(message, cls, method=None):
    if method is None:
        method = cls.__init__

    setattr(cls, method.__name__, deprecate(message)(method))
    return cls


def deprecate_class(message, cls):
    add_deprecation_doc_warning(cls, message)
    return deprecate_method(message, cls, None)


# deprecations by version of HTCondor; call these in __init__.py


def deprecate_8_9_8():
    ## from python-bindings/log_reader.cpp

    # FIXME: deprecate htcondor.EntryType enum

    # This didn't exist on Windows, so trying to deprecate it throws AttributeError.
    if hasattr(htcondor, "LogReader"):
        deprecate_class(
            "LogReader is deprecated since v8.9.8 and will be removed in a future release.",
            htcondor.LogReader,
        )

    ## from python-bindings/event.cpp

    # FIXME: deprecate htcondor.LockType enum

    # This class can't be constructed from Python, so this is documentary.
    deprecate_class(
        "EventIterator is deprecated since v8.9.8 and will be removed in a future release.",
        htcondor.EventIterator,
    )

    # This class can't be constructed from Python, so this is documentary.
    deprecate_class(
        "FileLock is deprecated since v8.9.8 and will be removed in a future release.",
        htcondor.EventIterator,
    )

    htcondor.lock = deprecate(
        "lock() is deprecated since v8.9.8 and will be removed in a future release."
    )(htcondor.lock)

    htcondor.read_events = deprecate(
        "read_events() is deprecated since v8.9.8 and will be removed in a future release; use JobEventLog instead."
    )(htcondor.read_events)

    ## See GT#7524.
    htcondor.Schedd.negotiate = deprecate(
        "Schedd.negotiate() is deprecated since v8.9.8 and will be removed in a future release.",
    )(htcondor.Schedd.negotiate)

    ## See GT#7630
    def normalize_query_args(method):
        @wraps(method)
        def wrapper(self, *args, **kwargs):
            kwargs = _normalize_query_args(method, kwargs)
            return method(self, *args, **kwargs)

        return wrapper

    def _normalize_query_args(method, kwargs):
        kwargs = kwargs.copy()

        if "requirements" in kwargs:
            warnings.warn(
                'The "requirements" argument of {}() will be renamed to "constraint" in a future release. For now, it will accept both names.'.format(
                    method.__name__
                ),
                FutureWarning,
            )
            kwargs["constraint"] = kwargs.pop("requirements")
        if "attr_list" in kwargs:
            warnings.warn(
                'The "attr_list" argument of {}() will be renamed to "projection" in a future release. For now, it will accept both names.'.format(
                    method.__name__
                ),
                FutureWarning,
            )
            kwargs["projection"] = kwargs.pop("attr_list")

        return kwargs

    htcondor.Schedd.query = normalize_query_args(htcondor.Schedd.query)
    htcondor.Schedd.xquery = normalize_query_args(htcondor.Schedd.xquery)
    htcondor.Schedd.history = normalize_query_args(htcondor.Schedd.history)
