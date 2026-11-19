#!/usr/bin/env pytest

from ornithology import *
import htcondor2
import os
import re
import sys
import pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
    SHADOW_DEBUG = $(SHADOW_DEBUG) D_TEST D_CAT
    STARTER_DEBUG = $(STARTER_DEBUG) D_TEST D_CAT
    PERIODIC_EXPR_INTERVAL = 1
    SYSTEM_PERIODIC_HOLD = (NumShadowExceptions >= 1)
"""
#endtestreq

NUM_INPUTS = 3000
INPUT_DIR = "input"
OUTPUT_DIR = "output"
ALWAYS_FILE = "always.txt"
TOTAL_BYTES = None

# On Windows, keep the names short enough that the input and execute
# directory paths stay under MAX_PATH.  3000 of them still make the
# transfer status far larger than the default 64KiB pipe buffer.
if sys.platform == "win32":
    FNAME_PREFIX = "pipe-full-read-test-file-name"
else:
    FNAME_PREFIX = "omg-why-is-this-filename-so-long-you-may-ask-well-it-is-to-increase-the-number-bytes-passed-in-the-pipe"

@action
def submit_job(default_condor, test_dir, path_to_sleep):
    global TOTAL_BYTES

    input_list = ALWAYS_FILE
    output_list = ALWAYS_FILE

    if not os.path.exists(ALWAYS_FILE):
        with open(ALWAYS_FILE, "w") as f:
            f.write("This file is always the first transfer item\n")

    if not os.path.exists(INPUT_DIR):
        os.mkdir(INPUT_DIR)

    for i in range(NUM_INPUTS):
        fname = f"{FNAME_PREFIX}{i:010}.txt"

        input_file = os.path.join(INPUT_DIR, fname)
        if not os.path.exists(input_file):
            with open(input_file, "w") as f:
                f.write("\n")

        input_list += f",{input_file}"
        output_list += f",{fname}"

    TOTAL_BYTES = len(fname) * NUM_INPUTS

    submit = htcondor2.Submit(f"""
        executable = {path_to_sleep}
        arguments  = 0
        log        = job.log

        transfer_input_files = {input_list}

        transfer_output_files = {output_list}
        My.OutputDirectory = "{OUTPUT_DIR}"

        should_transfer_files = YES

        queue
    """)

    return default_condor.submit(submit)

@action
def shadow_log(default_condor):
    return default_condor.shadow_log.open()

@action
def starter_log_lines(default_condor, submit_job):
    lines = []
    for log in default_condor.log_dir.glob("StarterLog*"):
        lines.extend(log.read_text(errors="replace").splitlines())
    return lines

def count_large_pipe_reads(lines):
    found_msg = 0
    pattern = re.compile(r"PipeReadFullString\([0-9]+\) Total Reads: [0-9]+")
    for line in lines:
        if re.search(pattern, line):
            analyze = line[line.find("PipeReadFullString"):]
            n_bytes, n_reads = re.findall(r"\d+", analyze)

            if int(n_bytes) >= TOTAL_BYTES:
                found_msg += 1
                # Asserting on n_reads assumes the default Linux pipe buffer
                # size, but on machines with very large amounts of memory, the
                # pipe buffer size may be substantially larger. The assert
                # is not critical for this test, so we leave it out.
    return found_msg

class TestFTOPipeFullRead:
    def test_no_shadow_exception(self, submit_job):
        # On Windows, file transfers run in-process, so a transfer whose
        # status is sent through a pipe that nothing reads until the
        # transfer completes deadlocks once the status overflows the pipe
        # buffer, and the job never completes.
        assert submit_job.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=300
        )

    # On Windows the shadow and starter use blocking file transfers,
    # so neither reads transfer status from a pipe.
    @pytest.mark.skipif(sys.platform == "win32", reason="file transfer is blocking on Windows")
    def test_check_num_reads(self, shadow_log):
        # The large message is the spooled files list from the input upload
        assert count_large_pipe_reads(msg.line for msg in shadow_log.read()) == 1

    @pytest.mark.skipif(sys.platform == "win32", reason="file transfer is blocking on Windows")
    def test_check_starter_num_reads(self, starter_log_lines):
        # The large message is the spooled files list from the output upload
        assert count_large_pipe_reads(starter_log_lines) == 1
