from .classad2_impl import _handle as handle_t

from .classad2_impl import _classad_init
from .classad2_impl import _classad_to_string

class ClassAd():

    def __init__(self):
        self._handle = handle_t()
        _classad_init(self, self._handle)


    def __str__(self):
        return _classad_to_string(self._handle)


    def __getitem__(self, key):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_get_item(self._handle, key)
