#!/usr/bin/env pytest

import htcondor2 as htcondor
import logging

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor", 
        config={
            "FOO": "BAR"
        },
    ) as condor:
        yield condor


@action
def full_version(condor):
    result = condor.run_command(["condor_version"], timeout=5)
    assert result.returncode == 0
    condor_version = result.stdout.split("\n")[0]
    assert len(condor_version) > 0
    return condor_version


@action
def version(condor):
    with condor.use_config():
        condor_version = htcondor.param.get("CONDOR_VERSION")
        assert condor_version is not None
    return condor_version


@action
def full_platform(condor):
    result = condor.run_command(["condor_version"], timeout=5)
    assert result.returncode == 0
    condor_platform = result.stdout.split("\n")[-1]
    assert len(condor_platform) > 0
    return condor_platform


class TestPythonBindingsVersion:

    def test_full_version(self, condor, full_version):
        print(f"\n\nChecking htcondor.version() against {full_version}\n\n")
        assert htcondor.version() == full_version

    def test_full_platform(self, condor, full_platform):
        print(f"\n\nChecking htcondor.platform() against {full_platform}\n\n")
        assert htcondor.platform() == full_platform
