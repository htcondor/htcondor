#!/usr/bin/env pytest

from ornithology import *
import htcondor
import os
import re

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
    SHADOW_DEBUG = $(SHADOW_DEBUG) D_TEST D_CAT
    PERIODIC_EXPR_INTERVAL = 1
    SYSTEM_PERIODIC_HOLD = (NumShadowExceptions >= 1)
"""
#endtestreq

NUM_INPUTS = 3000
INPUT_DIR = "input"
OUTPUT_DIR = "output"
ALWAYS_FILE = "always.txt"
TOTAL_BYTES = None

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
        fname = f"omg-why-is-this-filename-so-long-you-may-ask-well-it-is-to-increase-the-number-bytes-passed-in-the-pipe{i:010}.txt"

        input_file = os.path.join(INPUT_DIR, fname)
        if not os.path.exists(input_file):
            with open(input_file, "w") as f:
                f.write("\n")

        input_list += f",{input_file}"
        output_list += f",{fname}"

    TOTAL_BYTES = len(fname) * NUM_INPUTS

    submit = htcondor.Submit(f"""
        executable = {path_to_sleep}
        arguments  = 0
        log        = job.log

        transfer_input_files = {input_list}

        transfer_output_files = {output_list}
        My.OutputDirectory = "{OUTPUT_DIR}"

        should_transfer_files = YES
        transfer_executable = False

        queue
    """)

    return default_condor.submit(submit)

@action
def shadow_log(default_condor):
    return default_condor.shadow_log.open()

class TestFTOPipeFullRead:
    def test_no_shadow_exception(self, submit_job):
        assert submit_job.wait(
            condition=ClusterState.all_complete,
            fail_condition=ClusterState.any_held,
            timeout=45
        )

    def test_check_num_reads(self, shadow_log):
        found_msg = 0
        pattern = re.compile(r"PipeReadFullString\([0-9]+\) Total Reads: [0-9]+")
        for msg in shadow_log.read():
            if re.search(pattern, msg.line):
                analyze = msg.line[msg.line.find("PipeReadFullString"):]
                n_bytes, n_reads = re.findall(r"\d+", analyze)

                if int(n_bytes) >= TOTAL_BYTES:
                    found_msg += 1
                    assert int(n_reads) >= 2

        # We expect the large pipe message twice (input && output transfer)
        assert found_msg == 2
