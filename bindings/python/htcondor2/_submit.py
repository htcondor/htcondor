from typing import (
    Union,
    Dict,
)

from pathlib import Path

from ._common_imports import (
    classad,
    handle_t,
)

from collections.abc import MutableMapping

from .htcondor2_impl import (
    _submit_init,
    _submit__getitem__,
    _submit__setitem__,
    _submit_keys,
    _submit_expand,
    _submit_getqargs,
    _submit_setqargs,
    _submit_from_dag,
)

#
# MutableMapping provides generic implementations for all mutable mapping
# methods except for __[get|set|del]item__(), __iter__, and __len__.
#

class Submit(MutableMapping):

    # This is exceedingly clumsy, but matches the semantics from version 1.
    def __init__(self,
        input : Union[Dict[str, str], str] = None,
        ** kwargs
    ):
        '''
        An object representing a job submit description.  It uses the same
        language as *condor_submit.*  Values in the submit description language
        have no type; they are all strings.

        If `input` is a dictionary, it will be serialized into the submit
        language.  Otherwise, `input` is a string in the submit language.

        If keyword arguments are supplied, they will update this object
        after the submit-language string, if any, is parsed.

        This object implements the Python dictionary protocol, except
        that attempts to `del()` entries will raise a `NotImplementedError`
        because the underlying logic does not distinguish between a
        missing key and a key whose value is the empty string.  (This seems
        better than allowing you to `del(s[key])` but having `key` still
        appear in s.keys().)
        '''
        self._handle = handle_t()

        if isinstance(input, dict) and len(input) != 0:
            pairs = []
            for key,value in input.items():
                if not isinstance(key, str):
                    raise TypeError("key must be a string")
                # This was an undocumented feature in version 1, and it's
                # likely to be useless.  This implementation assumes that
                # str(classad.ExprTree) is always valid in a submit-file.
                if isinstance(value, classad.ClassAd):
                    pairs.append(f"{key} = {repr(value)}")
                else:
                    # In version 1, we (implicitly) document the dict as
                    # being from strings to strings, but don't enforce it;
                    # the code basically just calls str() on the value and
                    # hopes for the best.  This is probably a mistake in
                    # general, but it's really convenient, and Ornithology
                    # depends on being able to submit bools and ints as
                    # those types.  Luckily, those are easy to check for
                    # specifically and unambiguous to handle.
                    if isinstance(value, int):
                        value = str(value)
                    elif isinstance(value, float):
                        value = str(value)
                    elif isinstance(value, bool):
                        value = str(value)
                    # This is cheating, but we use it _a lot_ in the test suite.
                    elif isinstance(value, Path):
                        value = str(value)
                    elif not isinstance(value, str):
                        raise TypeError("value must be a string")
                    pairs.append(f"{key} = {value}")
            s = "\n".join(pairs)
            _submit_init(self, self._handle, s)
        elif isinstance(input, str):
            _submit_init(self, self._handle, input)
        elif input is None:
            _submit_init(self, self._handle, None)
        else:
            raise TypeError("input must be a dictionary mapping string to strings, a string, or None")

        self.update(kwargs)


    def __getitem__(self, key):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        return _submit__getitem__(self, self._handle, key)


    def __setitem__(self, key, value):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not isinstance(value, str):
            raise TypeError("value must be a string")
        return _submit__setitem__(self, self._handle, key, value)


    def __delitem__(self, key):
        # We can actually fake this (using NULL values), but should we?
        raise NotImplementedError("Submit object keys can not be removed, but the empty string is defined to have no effect.")


    #
    # Python's documentation is confusing and somewhat self-contradictory,
    # but the build-in `dict` tries really hard to raise a `RuntimeError`
    # if the dictionary is modified while iterating.
    #
    # This suggests that it's OK for __iter__ to get the list of keys
    # and then `yield` them one-at-a-time.  This is good, since the C++
    # `SubmitHash` almost certainly can't handle anything else.
    #
    # In version 1, keys(), items(), and values() all explicitly ignored
    # the submit hash's default values, but __getitem__() did not.  This
    # was madness.  Implementing htcondor2.Submit as a MutableMapping
    # subclass eliminates the possibility of those functions drifting out
    # synch with __iter__(); the current implementation of __len__() is
    # slow, but should also always agree with whatever we do in __iter__().
    #
    # It would be possible to change __getitem__() to ignore the default
    # values as well (in C++, or less efficiently in Python), but that
    # doesn't seem helpful.
    #
    # Ignoring default values actually turns out to be necessary to correctly
    # (re)generate submit files from submit hashes.
    #
    def __iter__(self):
        keys = _submit_keys(self, self._handle)
        for key in keys.split('\0'):
            yield(key)


    def __len__(self):
        return len(self.keys())


    def __str__(self):
        rv = ''
        for key, value in self.items():
            rv = rv + f"{key} = {value}\n"
        qargs = _submit_getqargs(self, self._handle)
        if qargs is not None:
            rv = rv + "queue " + qargs
        return rv


    # Consider making this do what Schedd.submit() does.
    def __repr__(self):
        # FIXME
        pass


    def expand(self, attr : str) -> str:
        '''
        Expand all macros for the given attribute.

        :param str attr: The name of the attribute to expand.
        :return: The value of the given attribute, with all macros expanded.
        :rtype: str
        '''
        if not isinstance(attr, str):
            raise TypeError("attr must be a string")
        return _submit_expand(self, self._handle, attr)


    # In version 1, the documentation widly disagrees with the implementation;
    # this is what the implementation actually required.
    def jobs(self,
        count : int = 0,
        itemdata : iter = None,
        clusterid : int = 1,
        procid : int = 0,
        qdate : int = 0,
        owner : str = "",
    ):
        # FIXME
        pass


    # In version 1, the documentation widly disagrees with the implementation;
    # this is what the implementation actually required.
    def procs(self,
        count : int = 0,
        itemdata : iter = None,
        clusterid : int = 1,
        procid : int = 0,
        qdate : int = 0,
        owner : str = "",
    ):
        # FIXME
        pass


    def getQArgs(self) -> str:
        return _submit_getqargs(self, self._handle)


    def setQArgs(self, args : str):
        '''
        Set the queue statement.  This statement replaces the queue statement,
        if any, passed to the original constructor.
        '''
        if not isinstance(args, str):
            raise TypeError("args must be a string")
        _submit_setqargs(self, self._handle, args)


    def setSubmitMethod(self,
        method_value : int = -1,
        allow_reserved_values : bool = False
    ):
        # FIXME
        pass


    def getSubmitMethod(self) -> int:
        # FIXME
        pass


# List does not include options which vary only in capitalization.
_NewOptionNames = {
    "dagman":                   "DagmanPath",
    "schedd-daemon-ad-file":    "ScheddDaemonAdFile",
    "schedd-address-file":      "ScheddAddressFile",
    "alwaysrunpost":            "PostRun",
    "debug":                    "DebugLevel",
    "outfile_dir":              "OutfileDir",
    "config":                   "ConfigFile",
    "batch-name":               "BatchName",
    "load_save":                "SaveFile",
    "do_recurse":               "Recurse",
    "update_submit":            "UpdateSubmit",
    "import_env":               "ImportEnv",
    "include_env":              "GetFromEnv",
    "insert_env":               "AddToEnv",
    "dumprescue":               "DumpRescueDag",
    "valgrind":                 "RunValgrind",
    "suppress_notification":    "SuppressNotification",
}


def from_dag(filename : str, options : Dict[str, Union[int, bool, str]] = {}) -> Submit:
    if not isinstance(options, dict):
        raise TypeError("options must be a dict")
    if not Path(filename).exists():
        raise IOError(f"{filename} does not exist")

    # Convert from version 1 names to the proper internal names.
    internal_options = {}
    for key, value in options.items():
        if not isinstance(key, str):
            raise TypeError("options keys must by strings")
        elif len(key) == 0:
            raise KeyError("options key is empty")
        internal_key = _NewOptionNames.get(key.lower(), key)
        if not isinstance(value, (int, bool, str)):
            raise TypeError("options values must be int, bool, or str")
        internal_options[internal_key] = str(value)

    subfile = _submit_from_dag(filename, internal_options)
    subfile_text = Path(subfile).read_text()
    return Submit(subfile_text)


Submit.from_dag = from_dag
