#!/usr/bin/env pytest

import htcondor

from pathlib import Path
from ornithology import *

@action
def invoke_no_args(default_condor):
    p = default_condor.run_command(["htcondor", "eventlog"])
    return p

@action
def file_not_provided(default_condor):
    p = default_condor.run_command(["htcondor", "eventlog", "read"])
    return p

@action
def file_not_found(default_condor):
    p = default_condor.run_command(["htcondor", "eventlog", "read", "does_not_exist.log"])
    return p

@action
def submit_success(default_condor, test_dir):
    submit_file = open(test_dir / "helloworld.sub", "w")
    submit_file.write("""executable = /bin/echo
arguments = Hello, World!
log = helloworld.log
output = helloworld.out
request_disk = 10M

queue""")
    submit_file.close()
    p = default_condor.run_command(["htcondor", "job", "submit", test_dir / "helloworld.sub"])
    return p

# test success with default output
@action
def read_success(default_condor, test_dir):
    p = default_condor.run_command(["htcondor", "eventlog", "read", Path(test_dir / "helloworld.log")])
    return p

# test success with json output
@action
def read_success_json(default_condor, test_dir):
    p = default_condor.run_command(["htcondor", "eventlog", "read", Path(test_dir / "helloworld.log"), "-json"])
    return p

# test success with csv output
@action
def read_success_csv(default_condor, test_dir):
    p = default_condor.run_command(["htcondor", "eventlog", "read", Path(test_dir / "helloworld.log"), "-csv"])
    return p

# test groupby
# test follow
# potentially write tests for start time, wall time etc. but will ask greg about this


class TestHtcondorCliEventlog:
    def test_invoke_no_args(self, invoke_no_args):
        assert "Interacts with eventlogs" in invoke_no_args.stderr

    def test_file_not_provided(self, file_not_provided):
        expected_error = "htcondor eventlog read: error: the following arguments are required: log_file"
        assert  expected_error in file_not_provided.stderr
    
    def test_file_not_found(self, file_not_found):
        assert file_not_found.stderr == "Error while trying to run eventlog read:\nCould not find file: does_not_exist.log"

    def test_submit_success(self, test_dir, submit_success):
        assert submit_success.stderr == "Job 1 was submitted."
        jel = htcondor.JobEventLog((test_dir / "helloworld.log").as_posix())
        # Wait for the job to finish by watching its event log
        for event in jel.events(stop_after=None):
            if event.type == htcondor.JobEventType.JOB_TERMINATED:
                break
        assert Path(test_dir / "helloworld.out").read_text() == "Hello, World!\n"

    def test_read_success(self, test_dir, read_success):
        # submit a job and write to a log file
        expected_output = "Job   Host                           Start Time   Evict Time   Evictions   Wall Time    Good Time    CPU Usage"
        assert expected_output in read_success.stdout

    def test_read_success_json(self, test_dir, read_success_json):
        # assert that the output is in json format
        assert read_success_json.stdout.startswith("[")

    def test_read_success_csv(self, test_dir, read_success_csv):
        # assert that the output is in csv format
        expected_output = "Job ID,Host,Start Time,Evict Time,Evictions,Wall Time,Good Time,CPU Usage"
        assert expected_output in read_success_csv.stdout