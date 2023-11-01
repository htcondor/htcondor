#!/usr/bin/env pytest

import os
from pathlib import Path

from ornithology import (
    config,
    standup,
    Condor,
    action,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@config(params={
    "on_by_default":  ( {},                                          (False, True) ),
    "explicitly_on":  ( {"HPC_ANNEX_REQUIRE_JOB": "TRUE"},           (False, True) ),
    "explicitly_off": ( {"HPC_ANNEX_REQUIRE_JOB": "FALSE"},          (True, True) ),
    "implicitly_on":  ( {"HPC_ANNEX_REQUIRE_JOB": "FILE NOT FOUND"}, (False, True) ),
})
def test_case(request):
    return request.param


@config
def test_config(test_case):
    return test_case[0]


@config
def test_expected(test_case):
    return test_case[1]


@standup
def the_condor(test_dir, test_config):
    with Condor(
        local_dir=test_dir / "condor",
        config={** test_config, 'HPC_ANNEX_ENABLED': 'TRUE'},
    ) as condor:
        yield condor


@action
def test_results(the_condor):
    (Path( os.environ['HOME'] ) / '.hpc-annex').mkdir(parents=True, exist_ok=True)
    (Path( os.environ['HOME'] ) / '.condor').mkdir(parents=True, exist_ok=True)

    # This test depends on test_htcondor_annex_create_constraints.py passing.
    args = ['htcondor', 'annex', 'create', 'example', '--token_file', '/dev/null', '--test', '2', 'wholenode@anvil', '--nodes', '2']

    pre_job = the_condor.run_command(args=args, timeout=20)

    handle = the_condor.submit(
        description={
            "executable":           "/bin/sleep",
            "hold":                 "true",
            "MY.TargetAnnexName":   '"example"',
        },
        count=1,
    )

    post_job = the_condor.run_command(args=args, timeout=20)

    return ( pre_job.returncode == 0 and pre_job.stdout == '',
             post_job.returncode == 0 and post_job.stdout == ''
           )

class TestHPCAnnexRequireJob:
    def test_success(self, test_results, test_expected):
        assert test_results == test_expected
