#!/usr/bin/env pytest

import re
from time import sleep

from ornithology import *
from htcondor_cli.convert_ad import _ad_to_daemon_status

UNIT_TEST_CASES = {
    # Verify No MyType -> None
    "NO_MYTYPE" : {
        "foo" : "bar"
    },
    # Verify Wrong MyType -> None
    "FILTERED_MYTYPE" : {
        "MyType" : "Shadow"
    },
    # Verify MyType for master and 0 rdcdc -> 100 HP
    "MASTER_RDCDC_0%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 100,
    },
    # Verify MyType Collector and 1 rdcdc -> 0 HP
    "COLLECTOR_RDCDC_100%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Collector",
        "RecentDaemonCoreDutyCycle" : 1,
        "ExpectedDaemon" : "COLLECTOR",
        "ExpectedHP" : 0,
    },
    # Verify MyType Negotiator and 0.98 rdcdc -> 0 HP
    "NEGOTIATOR_RDCDC_98%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Negotiator",
        "RecentDaemonCoreDutyCycle" : 0.98,
        "ExpectedDaemon" : "NEGOTIATOR",
        "ExpectedHP" : 0,
    },
    # Verify MyType Startd and 0.95 rdcdc -> 95 HP
    "STARTD_RDCDC_95%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Startd",
        "RecentDaemonCoreDutyCycle" : 0.95,
        "ExpectedDaemon" : "STARTD",
        "ExpectedHP" : 95,
    },
    # Verify 0.33 rdcdc -> 98 HP
    "MASTER_RDCDC_33%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0.33,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 100,
    },
    # Verify 0.96 rdcdc -> 63 HP
    "MASTER_RDCDC_96%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0.96,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 63,
    },
    # Verify 0.97 rdcdc -> 31 HP
    "MASTER_RDCDC_97%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0.97,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 31,
    },
    # Verify MyType Schedd and 0 rdcdc -> 100 HP
    "SCHEDD_RDCDC_0%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 0,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 100,
    },
    # Verify Schedd 1 rdcdc -> 28 HP
    "SCHEDD_RDCDC_100%" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 1,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 28,
    },
    # Verify Schedd with 0 bytes per second Upload -> 93
    "SCHEDD_UPLOAD_0_BYTES_PER_SEC" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 0,
        "TransferQueueNumUploading" : 1,
        "FileTransferUploadBytesPerSecond_1m" : 0,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 93,
    },
    # Verify Download MB Waiting < 100 -> 100 HP
    "SCHEDD_VALID_DOWLOAD_WAITING" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 0,
        "FileTransferMBWaitingToDownload" : 10,
        "TransferQueueDownloadWaitTime" : 100000,
        "TransferQueueNumWaitingToDownload" : 3,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 100,
    },
    # Verify Download MB Waiting > 100 && Wait time < 5m -> 100 HP
    "SCHEDD_VALID_DOWLOAD_WAIT_TIME" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 0,
        "FileTransferMBWaitingToDownload" : 250,
        "TransferQueueDownloadWaitTime" : 20,
        "TransferQueueNumWaitingToDownload" : 3,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 100,
    },
    # Verify Download MB Waiting > 100 && Wait time > 5m -> 93 HP
    "SCHEDD_INVALID_DOWNLOAD_WAIT_TIME" : {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 0,
        "FileTransferMBWaitingToDownload" : 250,
        "TransferQueueDownloadWaitTime" : 100000,
        "TransferQueueNumWaitingToDownload" : 3,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 93,
    },
}

@action(params={name: name for name in UNIT_TEST_CASES})
def unit_tests(request):
    return (request.param, UNIT_TEST_CASES[request.param])

@action
def unit_test_name(unit_tests):
    return unit_tests[0]

@action
def unit_test_case(unit_tests):
    return unit_tests[1]

@action
def unit_test_result(unit_test_case):
    return _ad_to_daemon_status(unit_test_case)

#===============================================================================================
TEST_CASES = {
    "ACCESS_POINT" : (["htcondor", "ap", "status"], 5),
    "CENTRAL_MANAGER" : (["htcondor", "cm", "status"], 0),
    "SERVER" : (["htcondor", "server", "status"], 0),
}

@standup
def condor(test_dir):
    with Condor(local_dir=test_dir / "condor",
                config={
                    "COLLECTOR_UPDATE_INTERVAL" : "2",
                    "DAEMON_LIST" : "MASTER,COLLECTOR NEGOTIATOR,STARTD SCHEDD",
                }) as condor:
        yield condor

@action
def run_commands(condor):
    OUTPUT = {}
    for test, info in TEST_CASES.items():
        sleep(info[1])
        p = condor.run_command(info[0])
        OUTPUT[test] = p.stdout + p.stderr
    return OUTPUT

@action(params={name: name for name in TEST_CASES})
def test_commands(run_commands, request):
    return (request.param, run_commands[request.param])

#===============================================================================================
class TestHTCondorCLISytemStatus:
    def test_convert_ad_unit_tests(self, unit_test_name, unit_test_case, unit_test_result):
        if "MyType" not in unit_test_case or unit_test_case["MyType"] == "Shadow":
            assert unit_test_result is None
        else:
            assert unit_test_case["ExpectedDaemon"] == unit_test_result[0]
            assert unit_test_case["ExpectedHP"] == int(unit_test_result[1]["HealthPoints"])

    def test_htcondor_cli_sys_status(self, test_commands):
        # Ignore the first line (header)
        for line in test_commands[1].split("\n")[1:]:
            if line.strip() != "":
                # Assert all commands return good health for items
                assert re.match("^.+ Good", line, re.IGNORECASE)
                if test_commands[0] == "SERVER":
                    assert re.match("^(master|collector|negotiator|schedd|startd|overall) .+", line, re.IGNORECASE)
