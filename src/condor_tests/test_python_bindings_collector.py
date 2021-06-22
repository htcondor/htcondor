#!/usr/bin/env pytest

import classad
import htcondor
import logging
import os
import time

from ornithology import *


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor", 
        config={
            "DAEMON_LIST": "COLLECTOR MASTER",
            "USE_SHARED_PORT": False
        }
    ) as condor:
        yield condor


@standup
def collector(condor):
    collector = condor.get_local_collector()
    # Certain ad types require various fields to exist to be recognized correctly
    collector.advertise([classad.ClassAd({"MyType": "Accounting", "Name": "Accounting-1"})], "UPDATE_ACCOUNTING_AD")
    collector.advertise([classad.ClassAd({"MyType": "Accounting", "Name": "Accounting-2"})], "UPDATE_ACCOUNTING_AD")
    collector.advertise([classad.ClassAd({"MyType": "Collector", "Name": "Collector-1", "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>", "UpdatesLost": 0})], "UPDATE_COLLECTOR_AD")
    collector.advertise([classad.ClassAd({"MyType": "DaemonMaster", "Name": "DaemonMaster-1"})], "UPDATE_MASTER_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-1"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-2"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-3"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-4"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Negotiator", "Name": "Negotiator-1"})], "UPDATE_NEGOTIATOR_AD")
    collector.advertise([classad.ClassAd({"MyType": "Scheduler", "Name": "Scheduler-1", "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>"})], "UPDATE_SCHEDD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Submitter", "Name": "Submitter-1", "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>"})], "UPDATE_SUBMITTOR_AD")
    yield collector


@action
def accounting_ads(collector):
    ads = collector.query(htcondor.AdTypes.Accounting, True, [])
    return ads


@action
def collector_ad_counts(collector):
    ads = collector.query()
    counts = {}
    for ad in ads :
        if ad['MyType'] in counts:
            counts[ad['MyType']] += 1
        else:
            counts[ad['MyType']] = 1
    return counts


@action
def collector_ads(collector):
    ads = collector.query(htcondor.AdTypes.Collector, True, [])
    return ads


@action
def machine_ads(collector):
    ads = collector.query(htcondor.AdTypes.Startd, True, ["MyType", "Name", "Arch", "OpSys"])
    return ads


@action
def master_ads(collector):
    ads = collector.query(htcondor.AdTypes.Master, True, [])
    return ads


@action
def negotiator_ads(collector):
    ads = collector.query(htcondor.AdTypes.Negotiator, True, [])
    return ads


@action
def submitter_ads(collector):
    ads = collector.query(htcondor.AdTypes.Submitter, True, ["MyType", "Name", "MyAddress", "IdleJobs", "RunningJobs"])
    return ads


@standup
def collector_locate_ad(collector):
    collector_ad = collector.locate(htcondor.DaemonTypes.Collector)
    return collector_ad


@standup
def collector_locate_all_startd_ads(collector):
    collector_locate_all_startd_ads = collector.locateAll(htcondor.DaemonTypes.Startd)
    return collector_locate_all_startd_ads


class TestCollectorQuery:

    def test_collector_ad_counts(self, collector_ad_counts):
        assert collector_ad_counts["Accounting"] == 2
        assert collector_ad_counts["Collector"] == 1
        assert collector_ad_counts["DaemonMaster"] == 1
        assert collector_ad_counts["Machine"] == 4
        assert collector_ad_counts["Negotiator"] == 1
        assert collector_ad_counts["Scheduler"] == 1
        assert collector_ad_counts["Submitter"] == 1

    def test_accounting_ads(self, accounting_ads):
        assert len(accounting_ads) == 2

    def test_collector_ads(self, collector_ads):
        assert len(collector_ads) == 1

    def test_machine_ads(self, machine_ads):
        assert len(machine_ads) == 4

    def test_master_ads(self, master_ads):
        assert len(master_ads) == 1

    def test_negotiator_ads(self, negotiator_ads):
        assert len(negotiator_ads) == 1

    def test_submitter_ads(self, submitter_ads):
        assert len(submitter_ads) == 1

    def test_collector_locate(self, collector_locate_ad):
        assert collector_locate_ad["MyType"] == "Collector"

    def test_collector_locate_all(self, collector_locate_all_startd_ads):
        assert len(collector_locate_all_startd_ads) == 4