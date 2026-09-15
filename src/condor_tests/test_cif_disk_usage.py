#!/usr/bin/env pytest

import re
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

# In this test, we submit four jobs with two common input files -- one in a
# subdirectory -- and make sure that the jobs' D_TEST-reported sizes are all
# correct.
# -----------------------------------------------------------------------------

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
        raw_config='''
            DISK_EXCEEDED = (DiskUsage =!= UNDEFINED && DiskUsage > Disk)
            HOLD_REASON_DISK_EXCEEDED = disk usage exceeded request_disk
            use POLICY : WANT_HOLD_IF( DISK_EXCEEDED, 103, $(HOLD_REASON_DISK_EXCEEDED) )
        ''',
    ) as the_condor:
        yield the_condor


@action
def the_common_files( test_dir ):
    local_dir = test_dir / "the_condor.d"
    common_files_d = local_dir / "common-files.d"
    common_files_d.mkdir(exist_ok=True, parents=True)
    common_file_a = common_files_d / "common-file.a"
    subdirectory = common_files_d / "subdirectory"
    subdirectory.mkdir(exist_ok=True, parents=True)
    common_file_b = common_files_d / subdirectory / "common-file.b"

    # Write files that are just over 4 MB in size, because the quantization
    # sometimes matters.
    contents = "1234567890abcdef" * ((64 * 128 * 8 * 4) + 1)
    common_file_a.write_text(contents)
    common_file_b.write_text(contents)

    return common_files_d


@action
def the_running_jobs( the_condor, the_common_files ):
    job_description = {
        "shell":                    "sleep 6",

        "universe":                 "vanilla",
        "should_transfer_files":    "YES",
        "request_cpus":             1,
        "request_memory":           1,

        # This is just wrong; we need at least enough space for the CFs.
        # Symptom: `data1_2: State change: PREEMPT is TRUE.` after cxfer.
        # "request_disk":             256,

        # Let's try twice as much as we need, plus 1M[iB] more.
        # Symptom: `data1_2: State change: PREEMPT is TRUE.` after the split.
        # "request_disk":             "17M",

        # This works.  It quantizes to 10240, but the difference between it
        # and the catalog size (8203) quantizes to 1024, leaving 9216 for the
        # 8203 common files.
        # "request_disk":             "9217K",

        # This quantizes to 10240 as well, but its difference with C will
        # quantize to 2048, not leaving enough space.
        # Symptom: `data1_2: State change: PREEMPT is TRUE.` after the split.
        # "request_disk":             "10101K",

        # This quantizes to 9216, but its difference with C quantizes to
        # 1024, leaving not enough space.
        # Symptom: `data1_2: State change: PREEMPT is TRUE.` after the split.
        # "request_disk":             "8204K",

        ##
        ## The above all applied to 01c396918c1bc6a2ef853e529660580820eb2e60.
        ##
        ## After changing MODIFY_REQUEST_EXPR_REQUESTDISK, we see the following.
        ##

        # Fails, but with `slot1_5: Slot Requirements not satisfied`, because
        # RequestDisk is negative, which is accurate as far as it goes.
        # "request_disk":             256,

        # Fails, but with `slot1_5: Machine Requirements check (state 0:NORMAL) failed!`.
        # citing WithinResourceLimits, which is unaware of quantization.  It
        # computes `R-C` and gets 9001, which is more than the 8MB left over
        # after quantizing.
        # "request_disk":             "17M",

        ##
        ## Retry again, with updated WithinResourceLimits.
        ##

        # Works.
        # "request_disk":             "17M",

        # (still) Works.
        # "request_disk":             "9217K",

        # Works.  Leaving this one in the test suite as the hardest.
        "request_disk":             "10101K",

        # Fails, but it should, because we're a disk quantum short.
        # "request_disk":             "8204K",

        ##
        ## We could try this without both fixes, but let's not.
        ##

        "log":                      "the_running_jobs.log.$(ClusterID)",
        "transfer_common_input":    f"{the_common_files.as_posix()}",
    }

    job_handle = the_condor.submit(
        description=job_description,
        count=4,
    )


    # This section is just for debuggery.
    assert job_handle.wait(
        timeout=120,
        condition=ClusterState.running_exactly(4),
        fail_condition=ClusterState.any_held,
    )

    c = the_condor.get_local_collector()
    r = c.query(
        projection=['Name', 'Disk', 'DiskUsage'],
    )
    print(r)


    assert job_handle.wait(
        timeout=120,
        # condition=ClusterState.running_exactly(3),
        condition=ClusterState.all_terminal,
        fail_condition=ClusterState.any_held,
    )

    return job_handle


class TestCIFDiskSize:

    def test_disk_size(self, test_dir, the_condor, the_running_jobs):
        # This should probably be a fixture.
        local_dir = test_dir / "the_condor.d"
        log_directory = local_dir / "log"

        num_starter_logs = 0
        raw_bytes = 16 * ((64 * 128 * 8 * 4) + 1)
        two_files = 2 * raw_bytes
        # The directory object doesn't count the size of directories.
        stagingSize = int( (two_files + 1023) / 1024 )
        print(f"Staging size expected to be {stagingSize}...")

        # The .chirp.config (48), the .machine.ad (6104), and the .job.ad
        # (3770) take up a noticeable # amount of space when we're being
        # precise -- about 10 KiB.  The size of both depend strongly on
        # unimportant details, so allow some slop.
        slop = 32

        for starter_log_path in log_directory.glob('StarterLog.*'):
            starter_log = starter_log_path.read_text()
            for line in starter_log.splitlines():
                if 'D_TEST' in line and 'cxfer: sizeOnDisk' in line:
                    print(starter_log_path)
                    num_starter_logs += 1

                    matches = re.search( r'cxfer: sizeOnDisk \(([^)]+)\) = (\d+) \(KiB\)', line )
                    if matches:
                        if matches[1] == 'mapping':
                            print(line)
                            sizeOnDisk = int(matches[2])
                            assert 0 <= sizeOnDisk
                            assert sizeOnDisk < slop
                        elif matches[1] == 'staging':
                            print(line)
                            sizeOnDisk = int(matches[2])
                            print(f"... actual size on disk {sizeOnDisk}.")
                            assert stagingSize <= sizeOnDisk
                            assert sizeOnDisk < (stagingSize + slop)
                        else:
                            assert False

        all_starter_logs = 5
        assert num_starter_logs == all_starter_logs
