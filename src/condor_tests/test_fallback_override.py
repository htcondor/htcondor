#!/usr/bin/env pytest

import pytest

import htcondor2


# The block between the lines is a speculative/test implementation and not at
# all necessary for this test.
# -----------------------------------------------------------------------------

TEST_JOBS = {}

def jobs(*args, params):
    # Record the name and contents of each test case.
    global TEST_JOBS
    TEST_JOBS.update(params)


    def decorator(func):
        return pytest.fixture(
            scope="class",
            ids=params.keys(),
            params=params.keys(),
        )(func)

    if len(args) == 1:
        return decorator(args[0])

    return decorator


TEST_JOB_HANDLES = None
def make_test_job(the_condor, test_job_ID):
    global TEST_JOB_HANDLES

    if TEST_JOB_HANDLES is None:
        TEST_JOB_HANDLES = {}
        schedd = the_condor.get_local_schedd()
        for id, description in TEST_JOBS.items():
            submit = htcondor2.Submit(description)
            handle = schedd.submit(
                description=submit,
                count=1,
            )
            TEST_JOB_HANDLES[id] = handle

    return TEST_JOB_HANDLES[test_job_ID]


# -----------------------------------------------------------------------------


the_job_description = """
    universe = vanilla
    shell = cat common-file input-file-$(ProcID) > output-file-$(ProcID); sleep 10
    MY.CommonInputFiles = "common-file"
    transfer_input_files = input-file-$(ProcID)
    transfer_output_files = output-file-$(ProcID)
    should_transfer_files = YES
    requirements = HasCommonFilesTransfer
    request_cpus = 1
    request_memory = 1024
    log = $(ClusterID).log
    hold = true
"""


@jobs(params={
    "undefined":    f"{the_job_description}",
    "false":        f"{the_job_description}\nMY.RequireCommonFilesTransfer = False\n",
})
def expect_missing_job(request, default_condor):
    return make_test_job(default_condor, request.param)


@jobs(params={
    "true":        f"{the_job_description}\nMY.RequireCommonFilesTransfer = True\n",
})
def expect_present_job(request, default_condor):
    return make_test_job(default_condor, request.param)


class TestSubmitCommand:

    def test_missing(self, expect_missing_job):
        requirements = str(expect_missing_job.clusterad()['Requirements'])
        assert not 'TARGET.HasCommonFilesTransfer >= 2' in requirements


    def test_present(self, expect_present_job):
        requirements = str(expect_present_job.clusterad()['Requirements'])
        assert 'TARGET.HasCommonFilesTransfer >= 2' in requirements
