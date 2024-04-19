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
import itertools

import pytest

from pathlib import Path
import time
import os

import htcondor2 as htcondor
from htcondor2.personal import PersonalPool


TEST_COUNTER = itertools.count()


@pytest.fixture(scope="function")
def fn_test_dir(request, tmp_path):
    # set in test_htcondor_personal.run
    if "ON_BATLAB" in os.environ:
        path = Path.cwd() / "test_htcondor_personal-test-dirs"
    else:
        path = tmp_path

    path = path / "{}-{}-{}".format(request.node.name, next(TEST_COUNTER), time.time())
    path.mkdir(parents=True)

    return path


@pytest.fixture(scope="function")
def local_dir(fn_test_dir):
    d = fn_test_dir / "local_dir"
    d.mkdir(parents=True)

    return d


@pytest.fixture(scope="function")
def another_local_dir(fn_test_dir):
    d = fn_test_dir / "another_local_dir"
    d.mkdir(parents=True)

    return d


# Pool fixtures sleep before yielding because we can remove some flakiness
# by trying to make sure that the collector has actually gotten ads from all
# of the daemons. For example, some tests would ephemerally fail in
# Collector.locate because the test runs so fast that the schedd hasn't had
# time to send an ad to the collector yet.
# If the flakiness returns, either increase the delay,
# or find a cleverer way to wait for the pool to be really-actually-ready.

POOL_DELAY = 3


@pytest.fixture(scope="function")
def pool(local_dir):
    with PersonalPool(local_dir=local_dir) as pool:
        time.sleep(POOL_DELAY)
        yield pool


@pytest.fixture(scope="function")
def another_pool(another_local_dir):
    with PersonalPool(local_dir=another_local_dir) as another_pool:
        time.sleep(POOL_DELAY)
        yield another_pool
