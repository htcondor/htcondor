#!/usr/bin/env pytest

import classad2
import htcondor2

from ornithology import (
    action,
    Condor,
    ClusterState,
);


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "STARTD_ENVIRONMENT":           "http_proxy= https_proxy=",
        },
    ) as the_condor:
        yield the_condor


@action
def expected_prl():
    # We can add more detail as warranted.
    return [
        {
            "TransferUrl": 'debug://error/resolution[FailedName="a.b.c"]',
        },
        {
            "TransferUrl": 'debug://error/resolution[FailedName="x.y.z"]',
        },
        {
            "TransferUrl": 'file://no-such.file/1',
        },
        {
            "TransferUrl": 'file://no-such.file/2',
        },
        {
            "TransferUrl": 'http://no-such.tld/3',
        },
        {
            "TransferUrl": 'http://no-such.tld/4',
        },
    ]


@action
def expected_pil():
    expected_debug = {
        "TransferClass":    1,
        "PluginBasename":   "debug_plugin.py",
        "Schemes":          "debug",
        "ExitBySignal":     False,
    }

    expected_curl = {
        "TransferClass":    1,
        "PluginBasename":   "curl_plugin",
        "Schemes":          "file,http",
        "ExitBySignal":     False,
    }

    return (expected_debug, expected_curl)


@action
def the_held_job(the_condor, path_to_sleep, test_dir):
    the_description = {
        "universe":                 "vanilla",
        "executable":               path_to_sleep,
        "arguments":                1,
        "transfer_executable":      False,
        "should_transfer_files":    True,
        "log":                      test_dir / "sleep.log",
        "request_cpus":             1,
        "request_memory":           1,
        "transfer_input_files":     ",".join([
            'file://no-such.file/1',
            'file://no-such.file/2',
            'debug://error/resolution[FailedName="a.b.c"]',
            'debug://error/resolution[FailedName="x.y.z"]',
            'http://no-such.tld/3',
            'http://no-such.tld/4',
        ]),
    }
    job_handle = the_condor.submit(the_description, count=1)

    assert job_handle.wait(
        condition=ClusterState.all_held,
        fail_condition=ClusterState.any_complete,
        timeout=60,
    )

    return job_handle


@action
def the_input_epoch(the_held_job, the_condor):
    # There should be a v2 binding for this.
    p = the_condor.run_command([
        "condor_history",
        "-epochs",
        "-type", "input",
        "-limit", 1,
        "-l",
        the_held_job.clusterid,
    ])
    ad = classad2.parseOne(p.stdout)

    return ad


@action
def actual_prl(the_input_epoch):
    return the_input_epoch['InputPluginResultList']


@action
def actual_pil(the_input_epoch):
    return the_input_epoch['InputPluginInvocations']


class TestMultipleSchemes:

    def test_plugin_result_list(self, actual_prl, expected_prl):
        assertions = 0
        attributes = 0

        for expected in expected_prl:
            attributes += len(expected)
            transferURL = expected["TransferUrl"]

            # We could break after the asserting that the values match, but
            # continuing also asserts that each ad in the actual PRL has
            # a TransferURL, which catches the case where we accidentally
            # stick a PIL in it.
            for actual in actual_prl:
                if actual["TransferUrl"] != transferURL:
                    continue
                for key in expected.keys():
                    assert actual[key] == expected[key]
                    assertions += 1

        assert assertions == attributes


    def test_plugin_invocation_list(self, actual_pil, expected_pil):
        assertions = 0
        attributes = 0

        # We could break after the asserting that the values match, but
        # continuing also asserts that each ad in the actual PIL has
        # a PluginBasename, which catches the case where we accidentally
        # stick a PRL in it.
        for expected in expected_pil:
            attributes += len(expected)
            plugin_basename = expected["PluginBasename"]
            for actual in actual_pil:
                if actual["PluginBasename"] != plugin_basename:
                    continue
                for key in expected.keys():
                    assert actual[key] == expected[key]
                    assertions += 1

        assert assertions == attributes
