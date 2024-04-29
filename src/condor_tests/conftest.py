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

import logging

from pathlib import Path
import shutil
import re
import copy
import time
import tempfile
import os
import sys
import platform
import subprocess

import pytest

from _pytest.nodes import Item
from _pytest.runner import CallInfo

import htcondor

from ornithology import Condor, CONFIG_IDS


os.environ["_CONDOR_DAGMAN_AVOID_SLASH_TMP"] = "FALSE"


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytest_plugins = ["ornithology.plugin"]

RE_ID = re.compile(r"\[([^()]*)\]$")

ALREADY_SEEN = set()


def pytest_addoption(parser):
    parser.addoption(
        "--base-test-dir",
        action="store",
        default=None,
        help="Set the base directory that per-test directories will be created in. Defaults to a time-and-pid-stamped directory in the system temporary directory.",
    )


STAMP = "{}-{}".format(int(time.time()), os.getpid())


def get_base_test_dir(config):
    base_dir = config.getoption("base_test_dir")

    if base_dir is None:
        base_dir = Path(tempfile.gettempdir()) / "condor-tests-{}".format(STAMP)

    base_dir = Path(base_dir).absolute()

    base_dir.mkdir(exist_ok=True, parents=True)

    return base_dir


@pytest.hookimpl(tryfirst=True)
def pytest_report_header(config, startdir):
    return [
        "",
        "Base per-test directory: {}".format(get_base_test_dir(config)),
        "Platform:\n{}".format(platform.platform()),
        "Python version:\n{}".format(sys.version),
        "Python bindings version:\n{}".format(htcondor.version()),
        "HTCondor version:\n{}".format(
            subprocess.run(["condor_version"], stdout=subprocess.PIPE)
            .stdout.decode("utf-8")
            .strip()
        ),
        "",
    ]


class TestDir:
    def __init__(self):
        self._path = None

    def _recompute(self, funcitem):
        dir = (
            get_base_test_dir(funcitem.config)
            / funcitem.module.__name__
            / funcitem.cls.__name__
        )

        id = RE_ID.search(funcitem.nodeid)

        if id is not None:
            config_ids = CONFIG_IDS[funcitem.module.__name__]
            ids = sorted(id for id in id.group(1).split("-") if id in config_ids)
            if len(ids) > 0:
                dir /= "-".join(ids)

        if dir not in ALREADY_SEEN and dir.exists():
            shutil.rmtree(dir)

        ALREADY_SEEN.add(dir)
        dir.mkdir(parents=True, exist_ok=True)

        self._path = dir
        logger.info(
            "Test directory for test {} is {}".format(funcitem.nodeid, self._path)
        )

    @property
    def __dict__(self):
        return self._path.__dict__

    def __repr__(self):
        return repr(self._path)

    def __bool__(self):
        try:
            return bool(self._path)
        except RuntimeError:
            return False

    def __dir__(self):
        return dir(self._path)

    def __getattr__(self, name):
        return getattr(self._path, name)

    __str__ = lambda x: str(x._path)
    __lt__ = lambda x, o: x._path < o
    __le__ = lambda x, o: x._path <= o
    __eq__ = lambda x, o: x._path == o
    __ne__ = lambda x, o: x._path != o
    __gt__ = lambda x, o: x._path > o
    __ge__ = lambda x, o: x._path >= o
    __hash__ = lambda x: hash(x._path)
    __len__ = lambda x: len(x._path)
    __getitem__ = lambda x, i: x._path[i]
    __iter__ = lambda x: iter(x._path)
    __contains__ = lambda x, i: i in x._path
    __add__ = lambda x, o: x._path + o
    __sub__ = lambda x, o: x._path - o
    __mul__ = lambda x, o: x._path * o
    __floordiv__ = lambda x, o: x._path // o
    __mod__ = lambda x, o: x._path % o
    __divmod__ = lambda x, o: x._path.__divmod__(o)
    __pow__ = lambda x, o: x._path ** o
    __lshift__ = lambda x, o: x._path << o
    __rshift__ = lambda x, o: x._path >> o
    __and__ = lambda x, o: x._path & o
    __xor__ = lambda x, o: x._path ^ o
    __or__ = lambda x, o: x._path | o
    __truediv__ = lambda x, o: x._path.__truediv__(o)
    __neg__ = lambda x: -(x._path)
    __pos__ = lambda x: +(x._path)
    __abs__ = lambda x: abs(x._path)
    __invert__ = lambda x: ~(x._path)
    __complex__ = lambda x: complex(x._path)
    __int__ = lambda x: int(x._path)
    __float__ = lambda x: float(x._path)
    __oct__ = lambda x: oct(x._path)
    __hex__ = lambda x: hex(x._path)
    __index__ = lambda x: x._path.__index__()
    __coerce__ = lambda x, o: x._path.__coerce__(x, o)
    __enter__ = lambda x: x._path.__enter__()
    __exit__ = lambda x, *a, **kw: x._path.__exit__(*a, **kw)
    __radd__ = lambda x, o: o + x._path
    __rsub__ = lambda x, o: o - x._path
    __rmul__ = lambda x, o: o * x._path
    __rdiv__ = lambda x, o: o / x._path
    __rtruediv__ = __rdiv__
    __rfloordiv__ = lambda x, o: o // x._path
    __rmod__ = lambda x, o: o % x._path
    __rdivmod__ = lambda x, o: x._path.__rdivmod__(o)
    __copy__ = lambda x: copy.copy(x._path)


TEST_DIR = TestDir()


@pytest.fixture(scope="class")
def test_dir() -> Path:
    """
    This fixture provides a :class:`pathlib.Path` pointing to the unique, isolated
    test directory for the current test.

    Returns
    -------
    path
        The path to the test directory.
    """
    return TEST_DIR


def pytest_runtest_protocol(item, nextitem):
    if "test_dir" in item.fixturenames:
        TEST_DIR._recompute(item)
        os.chdir(str(TEST_DIR))


@pytest.fixture(scope="class")
def default_condor(test_dir):
    with Condor(local_dir=test_dir / "condor") as condor:
        yield condor


custom_exit_status = 0


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: Item, call: CallInfo):
    # Do the wrapped thing.
    outcome = yield

    global custom_exit_status

    result = outcome.get_result()
    if result.failed and custom_exit_status != 1:
        if result.when == "setup":
            custom_exit_status = 136
        elif result.when == "teardown":
            custom_exit_status = 137
        else:
            custom_exit_status = 1

@pytest.hookimpl(trylast=True)
def pytest_sessionfinish(session, exitstatus):
    global custom_exit_status

    if custom_exit_status > 1:
        session.exitstatus = custom_exit_status


# This is broken beyond belief, but PyTest doesn't allow you to anything sane.
def pytest_collection_modifyitems(items):
    the_list = []
    the_list[:] = items

    preen = []
    shadow = []
    others = []
    for item in the_list:
        if item.location[0] == "test_checkpoint_destination_two.py":
            if "[shadow-" in item.name:
                shadow.append(item)
            elif "-shadow]" in item.name:
                shadow.append(item)
            elif "[preen-" in item.name:
                preen.append(item)
            elif "-preen]" in item.name:
                preen.append(item)
            else:
                others.append(item)
        else:
            others.append(item)

    the_list = [ * others, * preen, * shadow ]
    items[:] = the_list
