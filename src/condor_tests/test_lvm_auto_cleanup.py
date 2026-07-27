#!/usr/bin/env pytest

# Verifies the startd's auto loopback-backed LVM setup (backing file, loop
# device, PV, VG) is fully torn down on clean shutdown. See
# src/condor_startd.V6/VolumeManager.cpp ~VolumeManager().
#
# Uses its own Condor `with` block instead of @standup, so LVM/loopback
# state can be inspected synchronously right after shutdown.

import logging
import pytest

from ornithology import *

from liblvm import (
    LVMTestable,
    LVM_SKIP_REASON,
    lvm_config,
    vg_exists,
    pv_exists,
    loop_dev_for_file,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)

AUTO_VG_NAME = "condor_test_auto_cleanup_vg"


@config(params={"thin": True, "thick": False})
def thin_provisioning(request):
    return request.param


class TestLVMAutoCleanup:
    def test_auto_lvm_devices_removed_after_shutdown(self, test_dir, thin_provisioning):
        backing_file = test_dir / "condor" / "lvm_backing.img"
        conf = lvm_config(
            thin_provisioning,
            backing_file,
            extra={"LVM_AUTO_VG_NAME": AUTO_VG_NAME},
        )

        loop_dev = None
        with Condor(test_dir / "condor", config=conf) as condor:
            job = condor.submit(
                {
                    "shell": "(fallocate -l 4M f.bin || dd if=/dev/zero of=f.bin bs=1M count=4)",
                    "universe": "vanilla",
                    "output": "output",
                    "error": "error",
                    "log": "auto_cleanup_log",
                    "request_cpus": "1",
                    "request_memory": "64m",
                    "request_disk": "32m",
                },
                count=1,
            )
            assert job.wait(
                condition=ClusterState.all_complete,
                timeout=120,
                verbose=True,
                fail_condition=ClusterState.any_held,
            )

            loop_dev = loop_dev_for_file(backing_file)
            assert loop_dev, "loopback device for backing file was never created"
            assert vg_exists(AUTO_VG_NAME)

        # condor_master has shut down; everything it auto-created is gone.
        assert not vg_exists(AUTO_VG_NAME)
        assert not pv_exists(loop_dev)
        assert loop_dev_for_file(backing_file) is None
        assert not backing_file.exists()
