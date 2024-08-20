#!/usr/bin/env pytest

import re
import subprocess

from ornithology import *
from htcondor_cli.convert_ad import _ad_to_daemon_status

TEST_CASES = {
    "SYSTEM" : ["htcondor", "system", "status"],
    "ACCESS_POINT" : ["htcondor", "ap", "status"],
    "CENTRAL_MANAGER" : ["htcondor", "cm", "status"],
}

UNIT_TEST_CASES = [
    # Verify No MyType -> None
    {
        "foo" : "bar"
    },
    # Verify Wrong MyType -> None
    {
        "MyType" : "Shadow"
    },
    # Verify MyType for master and 0 rdcdc -> 100 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 100,
    },
    # Verify MyType Collector and 1 rdcdc -> 0 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Collector",
        "RecentDaemonCoreDutyCycle" : 1,
        "ExpectedDaemon" : "COLLECTOR",
        "ExpectedHP" : 0,
    },
    # Verify MyType Negotiator and 0.98 rdcdc -> 0 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Negotiator",
        "RecentDaemonCoreDutyCycle" : 0.98,
        "ExpectedDaemon" : "NEGOTIATOR",
        "ExpectedHP" : 0,
    },
    # Verify MyType Startd and 0.95 rdcdc -> 95 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Startd",
        "RecentDaemonCoreDutyCycle" : 0.95,
        "ExpectedDaemon" : "STARTD",
        "ExpectedHP" : 95,
    },
    # Verify 0.33 rdcdc -> 98 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0.33,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 98,
    },
    # Verify 0.96 rdcdc -> 63 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0.96,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 63,
    },
    # Verify 0.97 rdcdc -> 31 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "DaemonMaster",
        "RecentDaemonCoreDutyCycle" : 0.97,
        "ExpectedDaemon" : "MASTER",
        "ExpectedHP" : 31,
    },
    # Verify MyType Schedd and 0 rdcdc -> 100 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 0,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 100,
    },
    # Verify Schedd 1 rdcdc -> 28 HP
    {
        "MyAddress" : "FILLER",
        "Machine" : "LOCAL_HOST",
        "MyType" : "Scheduler",
        "RecentDaemonCoreDutyCycle" : 1,
        "ExpectedDaemon" : "SCHEDD",
        "ExpectedHP" : 28,
    },
    # Verify Schedd with 0 bytes per second Upload -> 93
    {
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
    {
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
    {
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
    {
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
]

@action(params={name: name for name in TEST_CASES})
def run_commands(default_condor, request):
    with default_condor.use_config():
        # HTCondor cli logger outputs to stderr?
        p = subprocess.run(TEST_CASES[request.param], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
        output = p.stdout.rstrip().decode() + p.stderr.rstrip().decode()
        yield output

class TestHTCondorCLISytemStatus:
    def test_convert_ad_unit_tests(self):
        for test_ad in UNIT_TEST_CASES:
            display = "\nFailed Unit Test Input Ad: {{\n{0}\n}}\n".format('\n'.join([f'\t{key} : {val}' for key, val in test_ad.items()]))
            result = _ad_to_daemon_status(test_ad)
            if "MyType" not in test_ad or test_ad["MyType"] == "Shadow":
                assert result is None, display
            else:
                daemon, status = result
                assert test_ad["ExpectedDaemon"] == daemon, display
                assert test_ad["ExpectedHP"] == int(status["HealthPoints"]), display

    def test_htcondor_cli_sys_status(self, run_commands):
        # Ignore the first line (header)
        for line in run_commands.split("\n")[1:]:
            if line.strip() != "":
                # Common for Master Ad to not be reported -> UNKNOWN status
                if re.match("^.+ UNKNOWN", line, re.IGNORECASE):
                    assert re.match("^MASTER .+", line, re.IGNORECASE)
                    continue
                # Assert all commands return good health for items
                assert re.match("^.+ Good", line, re.IGNORECASE)
