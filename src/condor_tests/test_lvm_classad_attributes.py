#!/usr/bin/env pytest

# Verifies the LvmDetectedDisk/DiskQuantum classad attributes the startd
# publishes when STARTD_ENFORCE_DISK_LIMITS is on. Published on the
# startd's daemon ad (AdType.StartDaemon), not per-slot Machine ads -- see
# src/condor_startd.V6/VolumeManager.h PublishDiskInfo().

import logging
import pytest

import htcondor2 as htcondor
from ornithology import *

from liblvm import LVMTestable, LVM_SKIP_REASON, lvm_config

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

pytestmark = pytest.mark.skipif(not LVMTestable(), reason=LVM_SKIP_REASON)


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
def startd_ads_with_lvm_info(condor):
    ads = condor.status(
        ad_type=htcondor.AdTypes.StartDaemon,
        projection=["Name", "LvmDetectedDisk", "DiskQuantum"],
    )
    return [ad for ad in ads if "LvmDetectedDisk" in ad]


class TestLVMClassadAttributes:
    def test_lvm_detected_disk_published_and_positive(self, startd_ads_with_lvm_info):
        assert len(startd_ads_with_lvm_info) >= 1
        for ad in startd_ads_with_lvm_info:
            assert ad["LvmDetectedDisk"] > 0

    def test_disk_quantum_published_and_positive(self, startd_ads_with_lvm_info):
        assert len(startd_ads_with_lvm_info) >= 1
        for ad in startd_ads_with_lvm_info:
            assert "DiskQuantum" in ad
            assert ad["DiskQuantum"] > 0
