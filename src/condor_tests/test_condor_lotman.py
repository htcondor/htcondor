#!/usr/bin/env pytest

# Test that LotMan can run correctly, and that when
# it does run, it gets the correct accounting of 
# files at the different stages.

import htcondor
import logging
from math import isclose
import os,sys,stat
from pathlib import Path
import sqlite3
import time

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@standup
def condor(test_dir):
    with Condor(
        local_dir = test_dir / "condor",
        config={
            "ALLOW_WRITE": "*",
            "LOTMAN_DB": (test_dir).as_posix(),
            "LOTMAN_TRACK_SPOOL": "TRUE",
            "ALL_DEBUG": "D_FULLDEBUG"},
    ) as condor:
        yield condor

def write_job_submit_file(test_dir, input_files, output_files, exec_fname, fname):
    formatted_input_fnames = ", ".join(input_files)
    formatted_output_fnames = ", ".join(output_files)

    formatted_output_remaps = ""
    for out_name in output_files:
        new_path = (test_dir/out_name).as_posix()
        formatted_output_remaps += f'"{out_name} = {new_path},"'

    job_submit = f'''log =  {(test_dir / "lotman.log").as_posix()}
transfer_input_files= {formatted_input_fnames}
should_transfer_files= YES
when_to_transfer_output= ON_EXIT_OR_EVICT
transfer_output_files= {formatted_output_fnames}
#transfer_output_remaps = {formatted_output_remaps}
executable= {exec_fname}

queue
'''
    fp = (test_dir/fname).as_posix()
    write_file(fp, job_submit)

def write_job_exec_file(test_dir, fname):
    job_executable = '''#!/bin/bash
cat foo1.txt
echo "Making the log file bigger!!"
echo "In a foo file." > foo2.txt
'''
    fp = (test_dir/fname).as_posix()
    write_file(fp, job_executable)

def write_test_trnsfr_file(test_dir, fname):
    test_input = '''
In the first file, lorem ipsum dolor sit amet!
This is the second line
'''
    fpath = (test_dir/fname).as_posix()
    write_file(fpath, test_input)

def get_spool_size_and_count(dir_path, excluded_files=[]):
    total_size = 0
    total_count = 0

    for root, _, files in os.walk(dir_path):
        for file_name in files:
            if file_name not in excluded_files:
                file_path = os.path.join(root, file_name)
                total_count += 1
                total_size += os.path.getsize(file_path)

    total_size = total_size / (1024 ** 3)

    return total_size, total_count

def job_uid():
    return os.getuid()

def lotman_db_fpath(base_dir):
    lotDB_fpath = base_dir / ".lot" / "lotman_cpp.sqlite"
    return lotDB_fpath

def lotDB_exists(base_dir):
    return os.path.isfile(lotman_db_fpath(base_dir).as_posix())

def lotman_cumulative_f_sz(lotDB_path, job_uid):
    # Connect to the database
    conn = sqlite3.connect(lotDB_path)
    cursor = conn.cursor()

    # Execute the query
    cursor.execute("SELECT self_GB FROM lot_usage WHERE lot_name = ?", (str(job_uid),))
    cumulative_f_sz = cursor.fetchone()
    
    # Close the connection
    conn.close()
    return cumulative_f_sz[0]


def lotman_cumulative_f_count(lotDB_path, job_uid):
    # Connect to the database
    conn = sqlite3.connect(lotDB_path)
    cursor = conn.cursor()

    # Execute the query
    cursor.execute("SELECT self_objects FROM lot_usage WHERE lot_name = ?", (str(job_uid),))
    f_count = cursor.fetchone()

    # Close the connection
    conn.close()
    return f_count[0]

class TestLotManSpoolTracking:

    def test_lotman_tracking(
        self, test_dir, condor):

        run_fname = "run.sh"
        job_fname = "job.sub"
        input_test_fname = "foo.txt"
        output_test_fname = "foo2.txt"
        write_job_exec_file(test_dir, run_fname)
        write_test_trnsfr_file(test_dir, input_test_fname)
        write_job_submit_file(test_dir, [input_test_fname, run_fname], [output_test_fname], run_fname, job_fname)
        
        # Because we can't submit with regular condor.Submit() due to the -spool requirement,
        # we have to interact with job status in a different way
        my_schedd = condor.get_local_schedd()

        condor.run_command(["condor_submit", "-spool","job.sub"])

        # With the job submitted, we check its status and wait for it to
        # complete, up to a max timeout
        timeout = 30
        start_time = time.time()
        job_complete = False
        while my_schedd.query()[0]["JobStatus"] != 4:
            current_time = time.time()
            elapsed_time = current_time - start_time

            if elapsed_time >= timeout:
                print("Job timeout!")
                print("Job ad: ", my_schedd.query()[0])
                break 

        # Assert the job ended because of completion, not timeout
        assert my_schedd.query()[0]["JobStatus"] == 4

        # Check in the spool directory
        spool_path = (test_dir / "condor" / "spool" / "1" / "0" / "cluster1.proc0.subproc0").as_posix()
        log_name = "lotman.log"
        # We exclude the log file, because LotMan does not track this
        true_dir_size, true_f_count = get_spool_size_and_count(spool_path, excluded_files=[log_name])

        # Assert that the lot DB exists
        assert lotDB_exists(test_dir)

        # Assert that the total size is correct
        # First get uid, which is what the lot will be named
        my_uid = job_uid()
        lotman_dir_size = lotman_cumulative_f_sz(lotman_db_fpath(test_dir).as_posix(), my_uid)
        assert isclose(true_dir_size, lotman_dir_size)

        # Assert that the total count is correct
        lotman_f_count = lotman_cumulative_f_count(lotman_db_fpath(test_dir).as_posix(), my_uid)
        assert true_f_count == lotman_f_count

        # Now transfer files back and make sure LotMan decrements values,
        # reflecting that the spool for the job was deleted
        assert condor.run_command(["condor_transfer_data", "1.0"]).returncode == 0
        assert condor.run_command(["condor_rm", "1.0"]).returncode == 0

        # Wait for job removal/cleanup -- can't use the same type of loop as before
        # because the schedd clears this job from the list, so there's no JobStatus
        # to look at. 2 seconds should be good enough for waiting
        time.sleep(2)

        # Files should be gone, let's check
        true_dir_size, true_f_count = get_spool_size_and_count(spool_path, excluded_files=[log_name])
        assert true_dir_size == 0
        assert true_f_count == 0
        lotman_dir_size = lotman_cumulative_f_sz(lotman_db_fpath(test_dir).as_posix(), my_uid)
        assert isclose(true_dir_size, lotman_dir_size)
        lotman_f_count = lotman_cumulative_f_count(lotman_db_fpath(test_dir).as_posix(), my_uid)
        assert true_f_count == lotman_f_count
