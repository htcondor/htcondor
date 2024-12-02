#!/usr/bin/env pytest

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    action,
)

from htcondor2._schedd import (
    job_spec_hack
)

from classad2 import (
    ExprTree,
)


def f_success(* pargs):
    assert(True)


def f_failure(* pargs):
    assert(False)


class TestJobSpecTypes:

    def test_job_spec_int(self):
        job_spec_hack( "addr", 7, f_success, f_failure, [] )


    def test_job_spec_str(self):
        job_spec_hack( "addr", "6", f_success, f_failure, [] )
        job_spec_hack( "addr", "6.5", f_success, f_failure, [] )
        job_spec_hack( "addr", "ClusterID >= 4", f_failure, f_success, [] )


    def test_job_spec_ExprTree(self):
        constraint = ExprTree("ClusterID != 3")
        job_spec_hack( "addr", constraint, f_failure, f_success, [] )


    def test_job_spec_list(self):
        job_spec_hack( "addr", ["2", "1.1", "10"], f_success, f_failure, [] )


    # There's probably a way to mark this as expected to fail with a
    # particular exception, but for a test this simple, I won't bother.
    def test_job_spec_true_negative(self):
        try:
            job_spec_hack( "addr", 3.141, f_failure, f_failure, [] )
            assert(False)
        except TypeError:
            pass
