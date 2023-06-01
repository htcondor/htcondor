from .classad2_impl import _handle as handle_t

from .classad2_impl import (
    _classad_init,
    _classad_init_from_string,
    _classad_init_from_dict,
    _classad_to_string,
    _classad_to_repr,
    _classad_get_item,
    _classad_set_item,
    _classad_del_item,
    _exprtree_eq,
    _classad_size,
    _classad_keys,
)

from collections.abc import MutableMapping
from datetime import datetime, timedelta, timezone


#
# MutableMapping provides generic implementations for all mutable mapping
# methods except for __[get|set|del]item__(), __iter__, and __len__.
#
# The version 1 documentation says "[A] ClassAd object is iterable
# (returning the attributes) and implements the dictionary protocol.
# The items(), keys(), values(), get(), setdefault(), and update()
# methods have the same semantics as a dictionary."
#

class ClassAd(MutableMapping):

    def __init__(self, input=None):
        self._handle = handle_t()

        if input is None:
            _classad_init(self, self._handle)
            return
        if isinstance(input, dict):
            _classad_init_from_dict(self, self._handle, input)
            return
        if isinstance(input, str):
            _classad_init_from_string(self, self._handle, input)
            return
        raise TypeError("input must be None, str, or dict")


    def __repr__(self):
        return _classad_to_repr(self._handle)


    def __str__(self):
        return _classad_to_string(self._handle)


    # In version 1, we didn't allow Python to check it other knew how
    # to do equality comparisons against us, which was probably wrong.
    #
    # If don't define __eq__(), the one we get from the ABC Mapping
    # allows comparisons against other `Mapping`s, which we may or
    # may not actually want.
    def __eq__(self, other):
        if type(self) is type(other):
            return _exprtree_eq(self._handle, other._handle)
        else:
            return NotImplemented


    #
    # The MutableMapping abstract methods.
    #

    def __len__(self):
        return _classad_size(self._handle)


    def __getitem__(self, key):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_get_item(self._handle, key)


    def __setitem__(self, key, value):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        if isinstance(value, dict):
            return _classad_set_item(self._handle, key, ClassAd(value))
        return _classad_set_item(self._handle, key, value)


    def __delitem__(self, key):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_del_item(self._handle, key)


    def __iter__(self):
        keys = _classad_keys(self._handle)
        for key in keys:
            yield key


def _convert_local_datetime_to_utc_ts(dt):
    if dt.tzinfo is not None and dt.tzinfo.utcoffset(dt) is not None:
        the_epoch = datetime(1970, 1, 1, tzinfo=dt.tzinfo)
        return (dt - the_epoch) // timedelta(seconds=1)
    else:
        the_epoch = datetime(1970, 1, 1)
        naive_ts = (dt - the_epoch) // timedelta(seconds=1)
        offset = dt.astimezone().utcoffset() // timedelta(seconds=1)
        return naive_ts - offset
