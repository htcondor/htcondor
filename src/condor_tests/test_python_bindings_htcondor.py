#!/usr/bin/env pytest

import htcondor2 as htcondor
import logging
import time

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


@standup
def collector(condor):
    collector = condor.get_local_collector()
    yield collector


@standup
def collector_ad(collector):
    collector_ad = collector.query(htcondor.AdTypes.Collector)
    return collector_ad


@standup
def startd_ads(collector):
    private_ads = collector.query(htcondor.AdTypes.Startd)
    return private_ads


@standup
def private_ads(collector):
    private_ads = collector.query(htcondor.AdTypes.StartdPrivate)
    return private_ads


@standup
def claim_ads(collector, private_ads):
    claim_ads = []
    for ad in collector.locateAll(htcondor.DaemonTypes.Startd):
        for pvt_ad in private_ads:
            if pvt_ad.get('Name') == ad['Name']:
                ad['ClaimId'] = pvt_ad['Capability']
                claim_ads.append(ad)
    return claim_ads


@action
def schedd_submit_test_job(condor, test_dir):
    job = condor.submit(
        {
            "executable": "/bin/echo",
            "arguments": "Hello, schedd!",
            "output": (test_dir / "schedd_submit_test_job.out").as_posix(),
            "error": "schedd_submit_test_job.err",
            "log": "schedd_submit_test_job.log"
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job


@action
def negotiator_test_job(condor):
    job = condor.submit(
        {
            "executable": "/bin/echo",
            "arguments": "Hello, collector!",
            "output": "negotiator_job.out",
            "error": "negotiator_job.err",
            "log": "negotiator_job.log"
        }
    )
    return job



class TestPythonBindingsHtcondor:

    def test_remote_param(self, condor, collector_ad):
        print(f"\n\nLooking for FOO in parameters...{condor.config['FOO']}\n\n")
        print(f"\n\ncollector_ad: {collector_ad}\n\n")
        rparam = htcondor.RemoteParam(collector_ad)
        assert "FOO" in rparam
        assert rparam["FOO"] == "BAR"
        assert len(rparam.keys()) > 100

    def test_remote_set_param(self, collector_ad):
        rparam = htcondor.RemoteParam(collector_ad)
        assert "OOF" not in rparam
        rparam["OOF"] = "BAR"
        htcondor.send_command(collector_ad, htcondor.DaemonCommands.Reconfig)
        rparam2 = htcondor.RemoteParam(collector_ad)
        assert "OOF" in rparam2

    def test_schedd_submit(self, condor, test_dir, schedd_submit_test_job):
        assert open(test_dir / "schedd_submit_test_job.out").read() == "Hello, schedd!\n"

    def test_negotiatior(self, negotiator_test_job, startd_ads, private_ads, claim_ads):
        assert len(startd_ads) == len(private_ads)
        assert len(startd_ads) == len(claim_ads)
        assert negotiator_test_job.wait(condition=ClusterState.all_terminal)
        assert negotiator_test_job.state[0] == JobStatus.COMPLETED

    def test_ping(self, collector_ad):
        assert "MyAddress" in collector_ad
        secman = htcondor.SecMan()
        authz_ad = secman.ping(collector_ad, "WRITE")
        assert "AuthCommand" in authz_ad
        assert authz_ad['AuthCommand'] == 60021
        assert "AuthorizationSucceeded" in authz_ad
        assert authz_ad['AuthorizationSucceeded']

        authz_ad = secman.ping(collector_ad["MyAddress"], "WRITE")
        assert "AuthCommand" in authz_ad
        assert authz_ad['AuthCommand'] == 60021
        assert "AuthorizationSucceeded" in authz_ad
        assert authz_ad['AuthorizationSucceeded']

        authz_ad = secman.ping(collector_ad["MyAddress"])
        assert "AuthCommand" in authz_ad
        assert authz_ad['AuthCommand'] == 60011
        assert "AuthorizationSucceeded" in authz_ad
        assert authz_ad['AuthorizationSucceeded']
