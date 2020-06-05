# Copyright 2020 HTCondor Team, Computer Sciences Department,
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

from typing import Mapping, Any, Optional
import collections

import pytest

CONFIG_IDS = collections.defaultdict(set)


def _check_params(params):
    if params is None:
        return True

    for key in params.keys():
        if "-" in key:
            raise ValueError('config param keys must not include "-"')


def _add_config_ids(func, params):
    if params is None:
        return

    CONFIG_IDS[func.__module__] |= params.keys()


PARAMS = Optional[Mapping[str, Any]]


def config(*args, params: PARAMS = None):
    """
    Marks a function as a **config** fixture.
    Config is always performed before any :func:`standup` or :func:`action` fixtures
    are run.

    Parameters
    ----------
    params
    """

    def decorator(func):
        _check_params(params)
        _add_config_ids(func, params)
        return pytest.fixture(
            scope="module",
            params=params.values() if params is not None else None,
            ids=params.keys() if params is not None else None,
        )(func)

    if len(args) == 1:
        return decorator(args[0])

    return decorator


def standup(*args):
    """
    Marks a function as a **standup** fixture.
    Standup is always performed after all :func:`config` fixtures have run,
    and before any :func:`action` fixtures that depend on it.
    """

    def decorator(func):
        return pytest.fixture(scope="class")(func)

    if len(args) == 1:
        return decorator(args[0])

    return decorator


def action(*args, params: PARAMS = None):
    """
    Marks a function as an **action** fixture.
    Actions are always performed after all :func:`standup` fixtures have run,
    and before any tests that depend on them.

    Parameters
    ----------
    params
    """
    _check_params(params)

    def decorator(func):
        return pytest.fixture(
            scope="class",
            params=params.values() if params is not None else None,
            ids=params.keys() if params is not None else None,
        )(func)

    if len(args) == 1:
        return decorator(args[0])

    return decorator
