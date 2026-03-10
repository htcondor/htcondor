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

def get_who_ads(default_condor):
    who_snap = default_condor.run_command(["condor_who", "-startd", "-snap", "-long"])

    # extract the condor_who ads and put them into a dict of dicts by type and name
    # where slot related ads will be keyed by the slot prefix (i.e. slot1_1)
    the_ads = {'StartD':{}, 'Machine':{}, 'Slot.Config':{}, 'Slot.State':{}, 'Machine.Extra':{}}
    for ad in classad.parseAds(who_snap.stdout):
        mytype = ad.get('MyType')
        name = ad.get('Name')
        if mytype != 'Machine.Extra' : name = name.split("@",maxsplit=1)[0]
        the_ads[mytype][name]=ad

    return the_ads


@action
def the_before_ads(default_condor, test_dir):
    return get_who_ads(default_condor)

@action
def the_after_ads(default_condor, test_dir):
    return get_who_ads(default_condor)

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
        who_ads = the_before_ads
        slot1 = who_ads.get('Machine').get('slot1')
        slot2 = who_ads.get('Machine').get('slot2')
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
        who_ads = the_after_ads
        slot1 = who_ads.get('Machine').get('slot1')
        slot2 = who_ads.get('Machine').get('slot2')
        expect_assign1 = 'GPU-a0223334,GPU-b1223334,GPU-c2223334'
        expect_assign2 = 'GPU-c2223334,GPU-b1223334,GPU-a0223334'
        assert (slot1.get('GPUs') == 3
            and slot1.get('TotalSlotGPUs') == 3
            and slot1.get('AssignedGPUs') == expect_assign1
            and slot2.get('GPUs') == 3
            and slot2.get('TotalSlotGPUs') == 3
            and slot2.get('AssignedGPUs') == expect_assign2)

