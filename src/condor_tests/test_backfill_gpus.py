#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
    # simulate detection of type 4 (A100), 3 units
    GPU_SIM = 4,3
    GPU_DISCOVERY_EXTRA = $(GPU_DISCOVERY_EXTRA) -sim:$(GPU_SIM)
    use Feature : PartitionableSlot(1)
    use Feature : PartitionableSlot(2)
    SLOT_TYPE_2_BACKFILL = true
    STARTD_EVENTLOG = $(LOG)/EpEvents
    STARTD_DEBUG = $(STARTD_DEBUG) D_COMMAND:1
"""
#endtestreq

#import pytest
import logging
import htcondor2 as htcondor
import classad2 as classad

#from pathlib import Path
from ornithology import *


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Query the startd directly rather than with condor_who: on Windows condor_who
# runs tasklist, which can stall past the run_command timeout on a loaded machine.
# Returns the slot ads keyed by slot name (i.e. slot1)
def get_slot_ads(default_condor):
    ads = default_condor.direct_status(
        htcondor.DaemonType.Startd,
        htcondor.AdType.Startd,
        projection=['Name', 'GPUs', 'TotalSlotGPUs', 'AssignedGPUs'],
    )
    return {ad['Name'].split("@",maxsplit=1)[0] : ad for ad in ads}


@action
def the_before_ads(default_condor, test_dir):
    return get_slot_ads(default_condor)

@action
def the_after_ads(default_condor, test_dir):
    return get_slot_ads(default_condor)

@action
def reconfig_and_wait(default_condor, test_dir):
    success = False
    default_condor.run_command(['condor_reconfig'])
    with default_condor.use_config():
        ep_eventlog = htcondor.param.get('STARTD_EVENTLOG')
        with htcondor.JobEventLog(ep_eventlog) as evl:
            for ev in evl.events(60):
                if ev.type == htcondor.JobEventType.EP_RECONFIG:
                    success = True
                    break

    return success


class TestBackfillGpus:

    def test_slots_have_gpus(self, the_before_ads):
        slot1 = the_before_ads.get('slot1')
        slot2 = the_before_ads.get('slot2')
        expect_assign1 = 'GPU-a0223334,GPU-b1223334,GPU-c2223334'
        expect_assign2 = 'GPU-c2223334,GPU-b1223334,GPU-a0223334'
        assert (slot1.get('GPUs') == 3
            and slot1.get('TotalSlotGPUs') == 3
            and slot1.get('AssignedGPUs') == expect_assign1
            and slot2.get('GPUs') == 3
            and slot2.get('TotalSlotGPUs') == 3
            and slot2.get('AssignedGPUs') == expect_assign2)

    def test_reconfig_and_wait(self, reconfig_and_wait):
        assert reconfig_and_wait

    def test_slots_still_have_gpus(self, the_after_ads):
        slot1 = the_after_ads.get('slot1')
        slot2 = the_after_ads.get('slot2')
        expect_assign1 = 'GPU-a0223334,GPU-b1223334,GPU-c2223334'
        expect_assign2 = 'GPU-c2223334,GPU-b1223334,GPU-a0223334'
        assert (slot1.get('GPUs') == 3
            and slot1.get('TotalSlotGPUs') == 3
            and slot1.get('AssignedGPUs') == expect_assign1
            and slot2.get('GPUs') == 3
            and slot2.get('TotalSlotGPUs') == 3
            and slot2.get('AssignedGPUs') == expect_assign2)

