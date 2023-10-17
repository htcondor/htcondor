from collections.abc import MutableMapping

from .htcondor2_impl import (
    _param__getitem__,
    _param__setitem__,
    _param__delitem__,
    _param_keys,
)

class _Param(MutableMapping):

    def __init__(self):
        pass


    def __getitem__(self, key : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        return _param__getitem__(key)


    def __setitem__(self, key : str, value : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not isinstance(value, str):
            raise TypeError("value must be a string")
        return _param__setitem__(key, value)


    def __delitem__(self, key : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        return _param__delitem__(key)


    def __iter__(self):
        keys = _param_keys()
        for key in keys.split('\0'):
            yield(key)


    def __len__(self):
        return len(self.keys())

