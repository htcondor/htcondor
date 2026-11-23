#!/usr/bin/env pytest

#
# Exercise the Kerberos "SendCredential" path end to end, without Kerberos:
#
#   condor_submit runs SEC_CREDENTIAL_PRODUCER (a script that prints a
#   fixed blob), stores the blob with the CredD (STORE_CRED), and sets
#   SendCredential = true.  At job start, the starter fetches the blob from
#   the shadow (CREDD_GET_CRED, a DAEMON-level command at the shadow, sent
#   over the claim's match session), stores it in its own credential
#   directory, waits for the credmon to produce <user>.cc, copies that into
#   the sandbox, and points KRB5CCNAME at the copy.
#
# A thread in this test plays the role of the Kerberos credmon: for each
# <user>.cred it sees, it writes <user>.cc containing CC_PREFIX + the
# .cred bytes.  The AP-side daemons (credd, shadow) and the starter use
# separate credential directories, so the ccache the job sees can only
# have been made from the bytes that crossed the shadow -> starter
# connection.
#
# Do NOT relax ALLOW_DAEMON here.  The starter talks to the shadow as
# execute-side@matchsession, which the default ALLOW_DAEMON does not
# match, so this test checks that the shadow authorizes that identity for
# CREDD_GET_CRED (see HTCONDOR-4104).
#

import logging
import os
import secrets
import threading
import time
from getpass import getuser
from pathlib import Path

import htcondor2

from ornithology import (
    standup,
    action,
    Condor,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


USER = getuser()
CC_PREFIX = b"FAKE-KRB5-CCACHE:"
# Printable, so that it survives the producer's stdout unchanged.
TOKEN = ("fake-krb-ticket-" + secrets.token_hex(16)).encode()

JOB_SCRIPT = """#!/bin/sh
ccfile="${KRB5CCNAME#FILE:}"
if [ -z "$ccfile" ]; then
    echo "KRB5CCNAME not set" >&2
    exit 1
fi
cat "$ccfile"
"""


class FakeKrbCredmon:
    """Turn every <user>.cred in the given directories into <user>.cc."""

    def __init__(self, cred_dirs):
        self.cred_dirs = [Path(d) for d in cred_dirs]
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        self.thread.join(timeout=5)

    def _run(self):
        while not self.stop_event.is_set():
            for cred_dir in self.cred_dirs:
                try:
                    self._process(cred_dir)
                except Exception as e:  # keep the credmon alive
                    logger.warning(f"fake credmon: error in {cred_dir}: {e}")
            self.stop_event.wait(0.2)

    @staticmethod
    def _process(cred_dir):
        # The master removes CREDMON_COMPLETE at startup, and the schedd
        # won't start until the credmon puts it back.
        complete = cred_dir / "CREDMON_COMPLETE"
        if not complete.exists():
            complete.touch()
        for cred in cred_dir.glob("*.cred"):
            cc = cred.with_suffix(".cc")
            try:
                if cc.exists() and cc.stat().st_mtime >= cred.stat().st_mtime:
                    continue
                data = cred.read_bytes()
            except FileNotFoundError:
                continue
            tmp = cc.with_suffix(".cc.tmp")
            fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
            with os.fdopen(fd, "wb") as f:
                f.write(CC_PREFIX + data)
            os.replace(tmp, cc)
            logger.info(f"fake credmon: wrote {cc} from {len(data)} byte {cred.name}")


@standup
def cred_dirs(test_dir):
    ap = test_dir / "krb_ap"
    ep = test_dir / "krb_ep"
    for d in (ap, ep):
        d.mkdir(parents=True, exist_ok=True)
        d.chmod(0o700)
    return ap, ep


@standup
def producer(test_dir):
    path = test_dir / "cred_producer.sh"
    path.write_text("#!/bin/sh\nprintf '%s' '" + TOKEN.decode() + "'\n")
    path.chmod(0o755)
    return path


@standup
def condor(test_dir, cred_dirs, producer):
    ap_dir, ep_dir = cred_dirs
    credmon = FakeKrbCredmon([ap_dir, ep_dir])
    credmon.start()
    try:
        with Condor(
            local_dir=test_dir / "condor",
            config={
                # AP side: condor_submit, the credd and the shadow.
                "SEC_CREDENTIAL_DIRECTORY_KRB": str(ap_dir),
                # EP side: the starter keeps its own copy.
                "STARTER.SEC_CREDENTIAL_DIRECTORY_KRB": str(ep_dir),
                "SEC_CREDENTIAL_PRODUCER": str(producer),
                "AUTO_INCLUDE_CREDD_IN_DAEMON_LIST": "True",
                "CREDD_POLLING_TIMEOUT": "20",
                "SHADOW_DEBUG": "D_SECURITY",
                "STARTER_DEBUG": "D_FULLDEBUG",
                "CREDD_DEBUG": "D_FULLDEBUG",
            },
        ) as condor:
            yield condor
    finally:
        credmon.stop()


def starter_log_text(condor):
    text = ""
    for log in sorted(condor.log_dir.glob("StarterLog*")):
        text += log.read_text(errors="replace")
    return text


def wait_for_job(condor, log_path, timeout):
    """Return 'terminated', 'held', 'aborted', 'starter-cred-failed' or 'timeout'."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if log_path.exists():
            with htcondor2.JobEventLog(str(log_path)) as jel:
                for event in jel.events(stop_after=0):
                    if event.type == htcondor2.JobEventType.JOB_TERMINATED:
                        return "terminated"
                    if event.type == htcondor2.JobEventType.JOB_HELD:
                        return "held"
                    if event.type == htcondor2.JobEventType.JOB_ABORTED:
                        return "aborted"
        # The starter gives up quickly if it can't get the credential, but
        # the job then just goes back to idle; don't wait for the timeout.
        if "getUserCredential failed" in starter_log_text(condor):
            return "starter-cred-failed"
        time.sleep(1)
    return "timeout"


@action
def job_result(condor, test_dir, cred_dirs):
    ap_dir, ep_dir = cred_dirs

    script = test_dir / "show_ccache.sh"
    script.write_text(JOB_SCRIPT)
    script.chmod(0o755)

    out = test_dir / "job.out"
    err = test_dir / "job.err"
    log = test_dir / "job.log"
    submit_file = test_dir / "job.sub"
    submit_file.write_text(
        f"""
universe   = vanilla
executable = {script}
output     = {out}
error      = {err}
log        = {log}
should_transfer_files = YES
queue
"""
    )

    # Use the condor_submit tool: SEC_CREDENTIAL_PRODUCER is handled there.
    rv = condor.run_command(["condor_submit", str(submit_file)], timeout=60)
    assert rv.returncode == 0, f"condor_submit failed: {rv.stdout}\n{rv.stderr}"

    outcome = wait_for_job(condor, log, timeout=180)

    shadow_log = condor.shadow_log.path.read_text(errors="replace") \
        if condor.shadow_log.path.exists() else ""
    return {
        "outcome": outcome,
        "stdout": out.read_bytes() if out.exists() else b"",
        "stderr": err.read_text(errors="replace") if err.exists() else "",
        "starter_log": starter_log_text(condor),
        "shadow_log": shadow_log,
        "ap_cred": (ap_dir / f"{USER}.cred"),
        "ep_cred": (ep_dir / f"{USER}.cred"),
    }


class TestKrbSendCredential:
    def test_credd_stored_credential(self, job_result):
        assert job_result["ap_cred"].read_bytes() == TOKEN

    def test_shadow_authorized_credd_get_cred(self, job_result):
        denied = [
            line for line in job_result["shadow_log"].splitlines()
            if "PERMISSION DENIED" in line and "CREDD_GET_CRED" in line
        ]
        assert denied == []

    def test_starter_fetched_credential(self, job_result):
        assert f"CREDMON: got a cred of size {len(TOKEN)} bytes" in job_result["starter_log"]
        assert job_result["ep_cred"].read_bytes() == TOKEN

    def test_job_completed(self, job_result):
        assert job_result["outcome"] == "terminated", job_result["stderr"]

    def test_job_sees_ccache_from_fetched_credential(self, job_result):
        assert job_result["stdout"] == CC_PREFIX + TOKEN
