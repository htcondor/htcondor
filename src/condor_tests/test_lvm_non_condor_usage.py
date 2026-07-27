#!/usr/bin/env pytest

# Verifies foreign (non-condor-tagged) LVs are counted as
# LvmNonCondorDiskUsage and excluded from slots' available disk. See
# CONDOR_LV_TAG in src/condor_startd.V6/VolumeManager.cpp.
#
# Thick only: thin availability comes from the pool's size/data_percent,
# unaffected by a foreign LV living outside the pool.
#
# Uses a manually pre-created VG rather than auto-loopback, since auto
# mode wipes/recreates its VG on startup (CleanupAllDevices), which would
# destroy the foreign LV before it could be measured.

import logging
import pytest

import htcondor2 as htcondor
from ornithology import *

from liblvm import (
    LVMTestable,
    LVM_SKIP_REASON,
    ManualLVMStore,
    create_foreign_lv,
    SMALL_BACKING_FILE_MB,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)

FOREIGN_LV_NAME = "test_lvm_foreign_lv"
FOREIGN_LV_SIZE_MB = 16


@standup
def condor(test_dir):
    vg_name = "condor_test_noncondor_vg"

    with ManualLVMStore(
        test_dir / "lvm_backing.img",
        vg_name,
        size_mb=SMALL_BACKING_FILE_MB,
    ) as store:
        # Create the foreign LV before condor starts -- manual mode never
        # tears down or recreates a pre-existing volume group.
        create_foreign_lv(store.vg_name, FOREIGN_LV_NAME, FOREIGN_LV_SIZE_MB)

        lvm_conf = {
            "STARTD_ENFORCE_DISK_LIMITS": "True",
            "LVM_VOLUME_GROUP_NAME": store.vg_name,
        }

        with Condor(test_dir / "condor", config=lvm_conf) as condor:
            yield condor


@action
def startd_ad(condor):
    # Published on the startd's daemon ad, not per-slot Machine ads.
    ads = condor.status(
        ad_type=htcondor.AdTypes.StartDaemon,
        projection=["Name", "LvmDetectedDisk", "LvmNonCondorDiskUsage"],
    )
    matching = [ad for ad in ads if "LvmNonCondorDiskUsage" in ad]
    assert len(matching) >= 1
    return matching[0]


@action
def partitionable_slot_ad(condor):
    ads = condor.status(
        ad_type=htcondor.AdTypes.Startd,
        projection=["Name", "PartitionableSlot", "TotalSlotDisk"],
    )
    matching = [ad for ad in ads if ad.get("PartitionableSlot")]
    assert len(matching) >= 1
    return matching[0]


class TestLVMNonCondorUsage:
    def test_non_condor_usage_reflects_foreign_lv(self, startd_ad):
        foreign_bytes = FOREIGN_LV_SIZE_MB * 1024 * 1024
        # LVM rounds LV sizes up to the nearest VG extent.
        assert startd_ad["LvmNonCondorDiskUsage"] >= foreign_bytes
        assert startd_ad["LvmNonCondorDiskUsage"] < foreign_bytes * 1.5

    def test_available_disk_excludes_foreign_lv(self, startd_ad, partitionable_slot_ad):
        detected_kb = startd_ad["LvmDetectedDisk"] / 1024
        slot_kb = partitionable_slot_ad["TotalSlotDisk"]
        # Gap should be on the order of the foreign LV's size, not just
        # filesystem overhead.
        assert (detected_kb - slot_kb) >= (FOREIGN_LV_SIZE_MB * 1024 * 0.5)
