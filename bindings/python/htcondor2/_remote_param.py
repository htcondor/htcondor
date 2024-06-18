from collections.abc import MutableMapping

from .htcondor2_impl import (
    _remote_param_keys,
    _remote_param_get,
    _remote_param_set,
)

from ._common_imports import (
    classad
)

class RemoteParam(MutableMapping):

    def __init__(self, location : classad.ClassAd):
        """
        The keys and values of this :class:`collections.abc.MutableMapping` are the keys
        and values of the specified daemon's configuration.  Assigning to
        a key sets the "runtime" configuration for the corresponding
        HTCondor macro, which won't take effect until the daemon has been
        reconfigured.  See :macro:`ENABLE_RUNTIME_CONFIG`.  Nonetheless,
        prior assignments will be reflected in subsequent lookups, but
        see :meth:`refresh`, below.

        :param location:  A ClassAd with a ``MyAddress`` attribute, such as
            might be returned by :meth:`htcondor2.Collector.locate`.
        """
        self._location = location
        self.refresh()


    def refresh(self):
        """
        Rebuild the dictionary based on the current configuration of the
        daemon.  Configuration values set by assigning to this dictionary
        do not become part of the current configuration until the daemon
        has been reconfigured; if you have not reconfigured the daemon,
        this method will result in a dictionary without those changes.
        """
        key_list = _remote_param_keys(self._location._handle)
        self._keys = key_list.split(',')
        self._cache = dict()


    def __getitem__(self, key : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not key in self._keys:
            raise KeyError(key)

        if not key in self._cache:
            self._cache[key] = _remote_param_get(self._location._handle, key)
        return self._cache[key]


    def __setitem__(self, key : str, value : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not isinstance(value, str):
            raise TypeError("value must be a string")

        if key not in self._keys:
            self._keys.append(key)
        self._cache[key] = value
        return _remote_param_set(self._location._handle, key, value)


    def __delitem__(self, key : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not key in self._keys:
            raise KeyError(key)

        del self._cache[key]
        self._keys.remove(key)
        return _remote_param_set(self._location._handle, key, "")


    def __iter__(self):
        for key in self._keys:
            yield(key)


    def __len__(self):
        return len(self._keys)

