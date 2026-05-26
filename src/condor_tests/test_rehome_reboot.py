#!/usr/bin/env pytest

# Tests for "condor_ep rehome --reboot".  Rather than actually rebooting the
# test machine, we point STARTD_REBOOT_COMMAND at a script that just touches a
# sentinel file, so we can observe whether (and when) the startd would have
# rebooted.  The reboot is gated by the STARTD_REHOME_ALLOW_REBOOT guard
# expression:
#
#   * When the guard is unset (the default), a rehome that asks to reboot must
#     be refused outright, and no jobs may be evicted.
#   * When the guard is true, the startd evicts all jobs and then runs
#     STARTD_REBOOT_COMMAND once the last claim is gone.
#
# We drive the startd through the Python bindings (htcondor2.Startd.rehome),
# which exercises the whole command path; the CLI only adds argument parsing on
# top of this.

import time
import logging

import pytest
import htcondor2 as htcondor

from ornithology import (
    standup,
    Condor,
    ClusterState,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def _setup(test_dir, name, allow_reboot):
    """Write a stand-in reboot script and build a pool config.

    Returns (config, sentinel_path).  The script touches the sentinel file
    (passed as $1) instead of rebooting.
    """
    script = test_dir / f"fake_reboot_{name}.sh"
    sentinel = test_dir / f"rebooted_{name}.sentinel"
    script.write_text('#!/bin/sh\ntouch "$1"\n')
    script.chmod(0o755)

    # rehome persists STARTD_DIRECT_ATTACH_SCHEDD_NAME via the runtime
    # persistent config, which requires these to be enabled and a writable dir.
    persistent_dir = test_dir / f"persistent_{name}"
    persistent_dir.mkdir(parents=True, exist_ok=True)

    config = {
        "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR SCHEDD STARTD",
        "STARTD_REBOOT_COMMAND": f"{script} {sentinel}",
        "ENABLE_PERSISTENT_CONFIG": "true",
        "PERSISTENT_CONFIG_DIR": str(persistent_dir),
    }
    if allow_reboot is not None:
        config["STARTD_REHOME_ALLOW_REBOOT"] = allow_reboot
    return config, sentinel, persistent_dir


@standup
def deny_pool(test_dir):
    # STARTD_REHOME_ALLOW_REBOOT left unset -> reboot must be refused.
    config, sentinel, persistent_dir = _setup(test_dir, "deny", allow_reboot=None)
    with Condor(local_dir=test_dir / "deny", config=config) as condor:
        yield condor, sentinel, persistent_dir


@standup
def allow_pool(test_dir):
    config, sentinel, persistent_dir = _setup(test_dir, "allow", allow_reboot="true")
    with Condor(local_dir=test_dir / "allow", config=config) as condor:
        yield condor, sentinel, persistent_dir


def _submit_running_sleep(condor, path_to_sleep, test_dir, name):
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "3600",
            "transfer_executable": False,
            "should_transfer_files": True,
            "universe": "vanilla",
            "log": str(test_dir / f"sleep_{name}.log"),
            "request_cpus": "1",
            "request_memory": "128",
            "request_disk": "100",
        },
        count=1,
    )
    assert handle.wait(condition=ClusterState.all_running, timeout=120)
    return handle


def _rehome(condor, reboot):
    with condor.use_config():
        collector = htcondor.Collector()
        schedd_ad = collector.locate(htcondor.DaemonType.Schedd)
        startd_ad = collector.locate(htcondor.DaemonType.Startd)
        startd = htcondor.Startd(startd_ad)
        startd.rehome(schedd_name=schedd_ad["Name"], reboot=reboot)


class TestRehomeReboot:
    def test_reboot_refused_when_guard_unset(self, deny_pool, path_to_sleep, test_dir):
        condor, sentinel, _persistent_dir = deny_pool
        handle = _submit_running_sleep(condor, path_to_sleep, test_dir, "deny")

        # The guard expression is unset, so a reboot request must be refused.
        with pytest.raises(htcondor.HTCondorException):
            _rehome(condor, reboot=True)

        # The reboot command must not have run...
        time.sleep(5)
        assert not sentinel.exists()

        # ...and because we refuse before evicting, the job keeps running.
        assert handle.wait(condition=ClusterState.all_running, timeout=20)

    def test_reboot_runs_when_allowed(self, allow_pool, path_to_sleep, test_dir):
        condor, sentinel, persistent_dir = allow_pool
        _submit_running_sleep(condor, path_to_sleep, test_dir, "allow")

        # Guard permits it: the rehome should be accepted...
        _rehome(condor, reboot=True)

        # ...the direct-attach config should be persisted to disk so the host
        # comes back attached after the (simulated) reboot...
        persisted = persistent_dir / ".config.STARTD.rehome"
        assert persisted.exists()
        assert "STARTD_DIRECT_ATTACH_SCHEDD_NAME" in persisted.read_text()

        # ...and once the job is evicted, the startd runs STARTD_REBOOT_COMMAND,
        # creating the sentinel.
        deadline = time.time() + 120
        while time.time() < deadline and not sentinel.exists():
            time.sleep(1)
        assert sentinel.exists()
