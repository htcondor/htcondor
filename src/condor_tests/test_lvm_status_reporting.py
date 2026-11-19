#!/usr/bin/env pytest

# Regression test for `condor_status -startd -lvm`. See
# src/condor_status.V6/status.cpp SDO_StartD_Lvm mode.

import logging
import re

import pytest

from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)

READABLE_BYTES_RE = re.compile(r"\d+(\.\d+)?\s*(B|KB|MB|GB|TB)\b")


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


@standup
def condor(test_dir, thin_provisioning):
    with Condor(
        test_dir / "condor",
        config=lvm_config(thin_provisioning, test_dir / "condor" / "lvm_backing.img"),
    ) as condor:
        yield condor


@action
def condor_status_lvm_output(condor):
    result = condor.run_command(["condor_status", "-startd", "-lvm"], timeout=120)
    return result


class TestLVMStatusReporting:
    def test_condor_status_lvm_succeeds(self, condor_status_lvm_output):
        assert condor_status_lvm_output.returncode == 0

    def test_condor_status_lvm_reports_disk_column(self, condor_status_lvm_output):
        stdout = condor_status_lvm_output.stdout
        assert "DISK" in stdout

        lines = [line for line in stdout.splitlines() if line.strip()]
        assert len(lines) >= 2, "expected a header line plus at least one slot row"

        assert any(READABLE_BYTES_RE.search(line) for line in lines[1:]), (
            "expected at least one slot row with a human-readable disk size"
        )
