#!/usr/bin/env pytest

import re

from ornithology import (
    action,
    ClusterState,
    Condor,
    DaemonLog,
)

import classad2

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            # For simplicity, so that each test job gets its own starter log
            "STARTER_LOG_NAME_APPEND":      "JobID",

            "LOG_FILETRANSFER_PLUGIN_STDOUT_ON_SUCCESS":    "D_ALWAYS",
            "LOG_FILETRANSFER_PLUGIN_STDOUT_ON_FAILURE":    "D_ALWAYS",
        }
    ) as the_condor:
        yield the_condor


@action
def the_log(test_dir):
    return test_dir / "job.log"


@action
def the_completed_job(the_condor, the_log):
    description = {
        'executable':            '/bin/sleep',
        'transfer_executable':   False,
        'should_transfer_files': True,
        'universe':              'vanilla',
        'arguments':             1,

        'log':                  the_log.as_posix(),

        'transfer_input_files': 'debug://sleep/1, http://google.com',
        'MY.PluginData':        '[ foo = "bar"; ]',
        'MY.debug_PluginData':  '[ bar = "debug-data"; ]',
        'MY.http_PluginData':   '[ baz = "http-data"; ]',
    }

    the_job_handle = the_condor.submit(
        description=description,
        count=1,
    )

    assert the_job_handle.wait(
        timeout=120,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job_handle


@action
def the_starter_log_path(test_dir, the_completed_job):
    return (test_dir / "condor" / "log" / f"StarterLog.{the_completed_job.clusterid}.0")


class TestPluginClassAdTransfer:

    def test_v3_plugins(the_condor, the_completed_job, the_starter_log_path):
        expected_ads = [
            classad2.ClassAd({"foo": "bar"}),
            classad2.ClassAd({"bar": "debug-data"}),
        ]

        # 'FILETRANSFER: plugin ... exit=0 stdout:\n<new_ad>\n<new_ad>\n'
        prefix = 'FILETRANSFER: plugin '
        suffix = ' exit=0 stdout:'

        ad_block = ''
        in_ad_block = False
        starter_log = the_starter_log_path.read_text()
        for line in starter_log.splitlines():
            if not in_ad_block:
                if prefix in line and suffix in line:
                    in_ad_block = True
            else:
                if re.match(r'^\d\d/\d\d/\d\d \d\d:\d\d:\d\d ', line):
                    break
                ad_block += line
        actual_ads = [ad for ad in classad2.parseAds(ad_block)]
        assert(len(actual_ads) == 2)
        print()
        print(actual_ads)

        assert expected_ads[0] == actual_ads[0]
        assert expected_ads[1] == actual_ads[1]
