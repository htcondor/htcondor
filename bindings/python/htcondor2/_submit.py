from typing import Union

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
)

#
# MutableMapping provides generic implementations for all mutable mapping
# methods except for __[get|set|del]item__(), __iter__, and __len__.
#

class Submit(MutableMapping):

    # This is exceedingly clumsy, but matches the semantics from version 1.
    def __init__(self,
        input : Union[dict[str, str], str] = None,
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
                # This was an undocumented feature in version 1, and it's
                # likely to be useless.  This implementation assumes that
                # str(classad.ExprTree) is always valid as a submit-file.
                if isinstance(value, classad.ClassAd):
                    pairs.append(f"{key} = {repr(value)}")
                else:
                    pairs.append(f"{key} = {str(value)}")
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
    def __iter__(self):
        keys = _submit_keys(self, self._handle)
        for key in keys.split('\0'):
            yield(key)


    def __len__(self):
        return len(self.keys())


    def expand(self, attr : str) -> str:
        pass


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
        pass


    def getQArgs(self) -> str:
        pass


    def setQArgs(self, args : str):
        pass


    def from_dag(filename : str, options : dict = {}):
        pass


    def setSubmitMethod(self,
        method_value : int = -1,
        allow_reserved_values : bool = False
    ):
        pass


    def getSubmitMethod(self) -> int:
        pass
