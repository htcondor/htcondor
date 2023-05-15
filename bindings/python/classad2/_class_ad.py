from .classad2_impl import _handle as handle_t

from .classad2_impl import _classad_init
from .classad2_impl import _classad_to_string
from .classad2_impl import _classad_to_repr
from .classad2_impl import _classad_get_item
from .classad2_impl import _classad_set_item

from collections import UserDict

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
