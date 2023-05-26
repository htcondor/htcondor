from .classad2_impl import _handle as handle_t

from .classad2_impl import _classad_init
from .classad2_impl import _classad_to_string
from .classad2_impl import _classad_to_repr
from .classad2_impl import _classad_get_item
from .classad2_impl import _classad_set_item
from .classad2_impl import _classad_del_item

from collections import UserDict
from datetime import datetime, timedelta, timezone

class ClassAd(UserDict):

    def __init__(self):
        self._handle = handle_t()
        _classad_init(self, self._handle)


    def __repr__(self):
        return _classad_to_repr(self._handle)


    def __str__(self):
        return _classad_to_string(self._handle)


    def __getitem__(self, key):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_get_item(self._handle, key)


    def __setitem__(self, key, value):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_set_item(self._handle, key, value)


    def __delitem__(self, key):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_del_item(self._handle, key)


def _convert_local_datetime_to_utc_ts(dt):
    if dt.tzinfo is not None and dt.tzinfo.utcoffset(dt) is not None:
        the_epoch = datetime(1970, 1, 1, tzinfo=dt.tzinfo)
        return (dt - the_epoch) // timedelta(seconds=1)
    else:
        the_epoch = datetime(1970, 1, 1)
        naive_ts = (dt - the_epoch) // timedelta(seconds=1)
        offset = dt.astimezone().utcoffset() // timedelta(seconds=1)
        return naive_ts - offset
