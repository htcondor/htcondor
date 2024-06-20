#!/usr/bin/env pytest

import classad2 as classad
import htcondor2 as htcondor
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
            "COLLECTOR_DEBUG": "D_FULLDEBUG",
            "DAEMON_LIST": "COLLECTOR MASTER",
            "USE_SHARED_PORT": False,
        }
    ) as condor:
        yield condor


@standup
def collector(condor):
    collector = condor.get_local_collector()
    collector.advertise([classad.ClassAd({"MyType": "Accounting", "Name": "Accounting-1", "IsPytest": True})], "UPDATE_ACCOUNTING_AD")
    collector.advertise([classad.ClassAd({"MyType": "Accounting", "Name": "Accounting-2", "IsPytest": True})], "UPDATE_ACCOUNTING_AD")
    collector.advertise([classad.ClassAd({"MyType": "Collector", "Name": "Collector-1", "IsPytest": True})], "UPDATE_COLLECTOR_AD")
    collector.advertise([classad.ClassAd({"MyType": "DaemonMaster", "Name": "DaemonMaster-1", "IsPytest": True})], "UPDATE_MASTER_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-1", "IsPytest": True, "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>", "StartdIpAddr": "127.0.0.1"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-2", "IsPytest": True, "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>", "StartdIpAddr": "127.0.0.1"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-3", "IsPytest": True, "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>", "StartdIpAddr": "127.0.0.1"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Machine", "Name": "Machine-4", "IsPytest": True, "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>", "StartdIpAddr": "127.0.0.1"})], "UPDATE_STARTD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Negotiator", "Name": "Negotiator-1", "IsPytest": True})], "UPDATE_NEGOTIATOR_AD")
    collector.advertise([classad.ClassAd({"MyType": "Scheduler", "Name": "Scheduler-1", "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>", "IsPytest": True})], "UPDATE_SCHEDD_AD")
    collector.advertise([classad.ClassAd({"MyType": "Submitter", "Name": "Submitter-1", "MyAddress": "<127.0.0.1:38900?addrs=127.0.0.1-38900&alias=localhost&noUDP&sock=startd_6695_1b0e>", "IsPytest": True})], "UPDATE_SUBMITTOR_AD")
    # Wait for all ads to appear in the collector before returning
    for i in range(30):
        ads = collector.query(htcondor.AdTypes.Any, "IsPytest == True")
        if len(ads) == 11:
            return collector
        time.sleep(1)
    # If they did not all appear within 30 seconds, something is wrong
    assert False


@action
def accounting_ads(collector):
    ads = collector.query(htcondor.AdTypes.Accounting, "IsPytest == True", [])
    return ads


@action
def collector_ad_counts(collector):
    ads = collector.query(htcondor.AdTypes.Any, "IsPytest == True")
    counts = {}
    for ad in ads :
        if ad['MyType'] in counts:
            counts[ad['MyType']] += 1
        else:
            counts[ad['MyType']] = 1
    return counts


@action
def collector_ads(collector):
    ads = collector.query(htcondor.AdTypes.Collector, "IsPytest == True", [])
    return ads


@action
def machine_ads(collector):
    ads = collector.query(htcondor.AdTypes.Startd, "IsPytest == True", [])
    return ads


@action
def master_ads(collector):
    ads = collector.query(htcondor.AdTypes.Master, "IsPytest == True", [])
    return ads


@action
def negotiator_ads(collector):
    ads = collector.query(htcondor.AdTypes.Negotiator, "IsPytest == True", [])
    return ads


@action
def scheduler_ads(collector):
    ads = collector.query(htcondor.AdTypes.Schedd, "IsPytest == True", [])
    return ads


@action
def submitter_ads(collector):
    ads = collector.query(htcondor.AdTypes.Submitter, "IsPytest == True", [])
    return ads


@action
def locate_collector_ad(collector):
    locate_collector_ad = collector.locate(htcondor.DaemonTypes.Collector)
    return locate_collector_ad


@action
def locate_all_startd_ads(collector):
    locate_all_startd_ads = collector.locateAll(htcondor.DaemonTypes.Startd)
    return locate_all_startd_ads


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

    def test_scheduler_ads(self, scheduler_ads):
        assert len(scheduler_ads) == 1

    def test_submitter_ads(self, submitter_ads):
        assert len(submitter_ads) == 1

    def test_locate(self, locate_collector_ad):
        assert locate_collector_ad["MyType"] == "Collector"

    def test_locate_all(self, locate_all_startd_ads):
        assert len(locate_all_startd_ads) == 4
