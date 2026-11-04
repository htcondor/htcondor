#!/usr/bin/env pytest

#
# Test ActivationRequestMemory (HTCONDOR-4062), which the schedd sets in the
# job ad to the value RequestMemory evaluates to when each activation starts,
# so that the memory each activation actually requested is recorded in the
# job ad and the job epoch history even when RequestMemory is an expression.
#

import logging

import htcondor2 as htcondor

from ornithology import (
    standup, action,
    Condor, ClusterState,
    write_file, format_script,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

ATTR = "ActivationRequestMemory"


@standup
def condor(test_dir):
    with Condor(local_dir=test_dir / "condor") as condor:
        yield condor


@action
def fail_script(test_dir):
    return write_file(test_dir / "fail.sh", format_script(
        """
        #!/bin/sh
        sleep 2
        exit 1
        """
    ))


def complete_job(condor, test_dir, executable, name, request_memory, extra=None, universe="vanilla"):
    description = {
        "log":                      test_dir / f"{name}.log",
        "leave_in_queue":           True,
        "universe":                 universe,
        "should_transfer_files":    True,
        "executable":               executable,
        "arguments":                2,
        "request_memory":           request_memory,
    }
    if extra:
        description.update(extra)
    handle = condor.submit(description=description, count=1)
    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        verbose=True,
        timeout=120,
    )
    return handle.query()[0]


def epoch_values(condor, ad):
    with condor.use_config():
        epoch_ads = htcondor.Schedd().jobEpochHistory(
            f"ClusterId == {ad['ClusterId']}",
            ["NumShadowStarts", ATTR],
        )
    epoch_ads = sorted(epoch_ads, key=lambda a: a["NumShadowStarts"])
    values = [a.get(ATTR) for a in epoch_ads]
    logger.info(f"{ATTR} in epoch records: {values}")
    return values


@action
def constant_ad(condor, test_dir, path_to_sleep):
    return complete_job(condor, test_dir, path_to_sleep, "constant", 512)


@action
def real_ad(condor, test_dir, path_to_sleep):
    return complete_job(condor, test_dir, path_to_sleep, "real", "128 * 1.5")


@action
def expression_ad(condor, test_dir, fail_script):
    # NumJobStarts is 0 when the first activation starts and 1 when the
    # second one does, so the two activations request 128 and 192, and
    # RequestMemory evaluates to 256 once the job is done.
    return complete_job(condor, test_dir, fail_script, "expression",
                        "128 + 64 * NumJobStarts", {"max_retries": 1})


@action
def expression_epoch_values(condor, expression_ad):
    return epoch_values(condor, expression_ad)


@action
def local_ad(condor, test_dir, path_to_sleep):
    return complete_job(condor, test_dir, path_to_sleep, "local", 512, universe="local")


class TestActivationRequestMemory:

    def test_constant(self, constant_ad):
        assert constant_ad[ATTR] == 512

    def test_real_valued(self, real_ad):
        assert isinstance(real_ad[ATTR], float)
        assert real_ad[ATTR] == 192.0

    def test_expression_records_last_activation(self, expression_ad):
        assert expression_ad["NumJobStarts"] == 2
        assert expression_ad[ATTR] == 192
        assert expression_ad.eval("RequestMemory") == 256

    def test_expression_in_epoch_history(self, expression_epoch_values):
        assert expression_epoch_values == [128, 192]

    def test_local_universe(self, local_ad):
        assert local_ad[ATTR] == 512
