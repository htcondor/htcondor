#!/usr/bin/env pytest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
    write_file,
    JobStatus,
)


#
# Sharing `CommonInputFiles` across a DAG is checked test_dagman_multiple_common_containers.py,
# but we also need to check the contrapositive: `CommonInputFiles` in two
# different clusters are _not_ shared.
#


@action
def the_condor( test_dir ):
    local_dir = test_dir / "the_condor.d"

    with Condor(
        local_dir=local_dir,
        config={
            "STARTER_DEBUG":    "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
            "SCHEDD_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_TEST",
        },
    ) as the_condor:
        yield the_condor


@action
def the_common_file( test_dir ):
    local_dir = test_dir / "the_condor.d"
    common_file = local_dir / "common-file"

    contents = "the common input file contents"
    common_file.write_text(contents)

    return common_file


@action
def the_submitted_jobs( the_condor, the_common_file ):
    job_description = {
        "shell":  "cat common-file > output-file-$(ClusterID)",

        "universe":                 "vanilla",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1,
        "request_disk":             256,
        "log":                      "the_running_jobs.log.$(ClusterID)",
        "transfer_common_input_files":  f"{the_common_file.as_posix()}",
        "hold":						True,
    }

    job_handles = [None] * 2

    job_handles[0] = the_condor.submit(
        description=job_description,
        count=1,
    )

    job_handles[1] = the_condor.submit(
        description=job_description,
        count=1,
    )

    return job_handles


class TestCatalogScopes:

    def test_catalog_scopes(self, the_condor, the_submitted_jobs):
        schedd = the_condor.get_local_schedd()
        results = schedd.query(
            projection=["ClusterID", "RequestedCatalogs"]
        )

        assert len(results) == 2
        assert results[0]['RequestedCatalogs'] != results[1]['RequestedCatalogs']
