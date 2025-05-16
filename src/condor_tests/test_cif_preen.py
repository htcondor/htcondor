#!/usr/bin/pytest

import pytest

import os
import time
import subprocess
from pathlib import Path

#
# Construct a synthetic directory, `syndicate`, underneath $(LOCK), which has
# (multiple) examples of each of the five kinds of files; some should be
# deleted and some shouldn't be.  Then run condor_preen.  Then (the actual
# tests) check to make sure the files that should have been spared still
# remain, and that the files that should have been removed were.
#

REMOVED = [
    [ "oldkeyfile",
      "oldremovelock.rm_1", "oldremovelock.rm_2",
      "oldkeyfile.message",
      "oldkeyfile.1234", "oldkeyfile.5678",
      ".oldkeyfile.432",
    ],
]

PRESERVED = [
    [ "newkeyfile",
      "newremovelock.rm_1", "newremovelock.rm_2",
      "newkeyfile.message",
      "newkeyfile.1234", "newkeyfile.5678",
      ".newkeyfile.432",
    ],
]


@pytest.fixture(params=REMOVED)
def removed(request):
    return request.param


@pytest.fixture(params=PRESERVED)
def preserved(request):
    return request.param


@pytest.fixture
def populated(test_dir, removed, preserved):
    syndicate = test_dir / "syndicate"
    syndicate.mkdir(parents=True, exist_ok=True)
    os.chdir(syndicate.as_posix())

    for file in removed:
        Path(file).touch()
        then = int(time.time()) - 301
        os.utime(file, (then, then))
    for file in preserved:
        Path(file).touch()

    return Path(syndicate.as_posix())


@pytest.fixture
def preened(test_dir, populated):
    env = {
        ** os.environ,
        '_CONDOR_LOG': test_dir.as_posix(),
        '_CONDOR_LOCK': test_dir.as_posix(),
        '_CONDOR_TOOL_DEBUG': 'D_TEST D_FULLDEBUG D_SUB_SECOND D_CATEGORY',
    }

    rv = subprocess.run(
        ['condor_preen', '-d'],
        env=env,
        timeout=60,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    print(rv.stdout)

    # We could write more complicated logic in condor_preen to try and
    # remove files which depend on other files being missed or expired
    # in a single pass, but there's really no need; this isn't a
    # correctness requirement.
    rv = subprocess.run(
        ['condor_preen', '-d'],
        env=env,
        timeout=60,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    print(rv.stdout)

    rv = subprocess.run(
        ['condor_preen', '-d'],
        env=env,
        timeout=60,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    print(rv.stdout)

    return rv.returncode == 0


class TestCIFPreen:

    def test_files(self, preened, populated, removed, preserved):
        assert(preened)

        os.chdir(populated.as_posix())
        for file in preserved:
            assert Path(file).exists()
        for file in removed:
            assert not Path(file).exists()
