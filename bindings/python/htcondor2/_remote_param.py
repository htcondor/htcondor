from collections.abc import MutableMapping

from .htcondor2_impl import (
    _remote_param_keys,
    _remote_param_get,
    _remote_param_set,
    _remote_param_del,
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
        reconfigured.  See :macro:`ENABLE_RUNTIME_CONFIG`.

        :param location:  A ClassAd with a ``MyAddress`` attribute, such as
            might be returned by :meth:`htcondor2.Collector.locate`.
        """
        self.location = location
        self.refresh()


    def refresh(self):
        """
        Rebuild the dictionary based on the current configuration of the
        daemon.  Configuration values set by assigning to this dictionary
        do not become part of the current configuration until the daemon
        has been reconfigured; if you have not reconfigured the daemon,
        this method will result in a dictionary without those changes.
        """
        key_list = _remote_param_keys(self.location._handle)
        self.keys = key_list.split(',')
        self.cache = dict()


    def __getitem__(self, key : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not key in self.keys:
            raise KeyError(key)

        if not key in self.cache:
            self.cache[key] = _remote_param_get(self.location._handle, key)
        return self.cache[key]


    def __setitem__(self, key : str, value : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not isinstance(value, str):
            raise TypeError("value must be a string")

        self.keys.append(key)
        self.cache[key] = value
        return _remote_param_set(self.location._handle, key, value)


    def __delitem__(self, key : str):
        if not isinstance(key, str):
            raise TypeError("key must be a string")
        if not key in self.keys:
            raise KeyError(key)

        del self.cache[key]
        self.keys.remove(key)
        return _remote_param_set(self.location._handle, key, "")


    def __iter__(self):
        for key in self.keys:
            yield(key)


    def __len__(self):
        return len(self.keys)

