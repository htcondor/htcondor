# Copyright 2019 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import threading
import functools
import inspect

from . import htcondor

# this is the single, global, re-entrant lock for the htcondor module
LOCK = threading.RLock()

# TODO: we would like to replace this with a per-daemon lock
# to do so, replace LOCK with a dictionary {daemon_id -> lock},
# determine which daemon we are talking to (somehow...),
# and get the lock object for that daemon, creating it if it doesn't exist

# any function/method included in DO_NOT_LOCK will not be locked
# * if you include a class, none of its methods will be locked
# * enums don't need to be listed; they have no methods, not even __init__
# * context manager __enter__ methods SHOULD be listed here if they return self (avoid recursion)
DO_NOT_LOCK = {
    htcondor.version,
    htcondor.platform,
    htcondor.JobEventLog,
    htcondor.JobEvent,
    htcondor.SubmitResult,
    htcondor.Transaction.__enter__,  # avoids recursion (this method returns self!)
}


def add_locks_to_module(mod, skip=None):
    skip = skip or set()

    for name, obj in vars(mod).items():
        if obj in skip:
            continue

        # "recurse" down into classes, but actually use a different function
        if inspect.isclass(obj):
            setattr(mod, name, add_locks_to_class_methods(obj, skip = skip))
        # functions defined by boost look like builtins, not functions
        elif inspect.isbuiltin(obj):
            setattr(mod, name, add_lock(obj))


def add_locks_to_class_methods(cls, skip=None):
    skip = skip or set()

    for name, obj in vars(cls).items():
        if obj in skip:
            continue

        # functions defined by boost look like builtins, not functions
        if inspect.isbuiltin(obj):
            setattr(cls, name, add_lock(obj))
    return cls


def add_lock(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        acquired = False
        is_cm = False
        try:
            acquired = LOCK.acquire()

            rv = func(*args, **kwargs)

            # if the function returned a context manager,
            # create a LockedContext to manage the lock
            is_cm = is_context_manager(rv)
            if is_cm:
                return LockedContext(rv, LOCK)
            else:
                return rv
        finally:
            # if we actually managed to acquire the lock, release it
            # but not if the LockedContext is responsible for it!
            if acquired and not is_cm:
                LOCK.release()

    return wrapper


class LockedContext:
    def __init__(self, cm, lock):
        self.cm = cm
        self.lock = lock

    def __enter__(self):
        return self.cm.__enter__()

    def __exit__(self, *args, **kwargs):
        try:
            return self.cm.__exit__(*args, **kwargs)
        finally:
            # we don't need to check if the lock was acquired here
            # we are guaranteed to have it, because it was passed to us already-acquired
            self.lock.release()


def is_context_manager(obj):
    """An object is a context manager if it has __enter__ and __exit__ methods."""
    return all(hasattr(obj, m) for m in ("__enter__", "__exit__"))
