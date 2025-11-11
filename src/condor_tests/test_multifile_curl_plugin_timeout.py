#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# make sure that file transfer plugins are enabled (might be disabled by default)
	ENABLE_URL_TRANSFERS = true
	FILETRANSFER_PLUGINS = $(LIBEXEC)/curl_plugin $(LIBEXEC)/data_plugin
"""
#endtestreq


#   test_multifile_curl_plugin_timeout.py
#
#   Test that the mutlifile curl plugin upload
#   and download transfer does and doesn't timeout
#   in various transfer rate scenarios.
#   Written By: Cole Bollig
#   Written:    2022-11-16
#
#   ==========================How To Add Test==========================
#   1. Add data flow tuple to TEST_CASES dictionary with a key name that
#      starts with either 'pass_' or 'fail_'. Test will fail otherwise.
#           - 'pass_' is for test where http transfer succeeds
#           - 'fail_' is for test where http transfer times out

from ornithology import *
from pytest_httpserver import HTTPServer
from werkzeug.wrappers import Response
from pathlib import Path
from time import sleep,time
import os

#--------------------------------------------------------------------------
#Get transfer_history file
@action
def xfer_history(test_dir):
    f = open(test_dir / "condor" / "log" / "transfer_history", "r")
    contents = f.readlines()
    f.close()
    return contents

#--------------------------------------------------------------------------
# Unset HTTP_PROXY for correct operation in Docker containers
lowered = dict()
for k in os.environ:
    lowered[k.lower()] = k
os.environ.pop(lowered.get("http_proxy", "http_proxy"), None)

#--------------------------------------------------------------------------
#Generic strings of a specific data size
ONE_BYTE_STR   = 'n'
TEN_BYTE_STR   = 'n' * 10
HUND_BYTE_STR  = 'n' * 100
ONE_KB_STR     = 'n' * 1024

#Transfer rate test cases
#   -Transfer goes in order of top down
#   -To add a delay add "Delay={Delay length}"
#    Note: Did this rather than | lambda: str(sleep(num))
#          because it would sleep but also yield 'None'
#          thus adding 4 extra bytes of unwanted data transfer
#   -For bytes sent either use one of the generic strings above
#    or do | lambda: 'char' * num_bytes

#!!!!!!!Note that the plugin checks for >1byte/sec ~every 30 seconds!!!!!!!
#       Due to plugin check rate and systems being overloaded at times
#       all test data delays are intervals of 30 with a 2 second leeway
TEST_CASES = {
    #Successful xfer test cases
    "pass_trailing_data": (
        #Massive xfer, delay, small xfer, done (1024:28,1:fin)
        lambda: ONE_KB_STR,
        lambda: "Delay=28",
        lambda: ONE_BYTE_STR,
    ),
    "pass_normal_end": (
        #Massive xfer, delay, enough xfer, delay, done (1024:28,35:28)
        lambda: ONE_KB_STR,
        lambda: "Delay=28",
        lambda: 'n' * 35,
        lambda: "Delay=28",
    ),
    #Failure xfer test cases
    "fail_slow_middle": (
        #Massive xfer, very long delay, timeout (1024:30,0:32)
        lambda: ONE_KB_STR,
        lambda: "Delay=62",
        lambda: ONE_BYTE_STR,
    ),
    "fail_delayed_start": (
        #timeout (0:32)
        lambda: "Delay=32",
        lambda: TEN_BYTE_STR,
    ),
    "fail_slow_start": (
        #Not enough xfer, timeout (20:32)
        lambda: 'n' * 20,
        lambda: "Delay=32",
        lambda: ONE_BYTE_STR,
    ),
}

#--------------------------------------------------------------------------
#Internal Iterator class to send xfer data correctly
class SlowIterator:
    def __iter__(self):
        for data in (l() for l in self.res):
            #print(data)
            if "Delay" in data:
                time = data.split("=")
                sleep(int(time[1]))
                continue
            else:
                yield data
    def __init__(self,response):
        self.res = response

#Handler fuction to return response of iterator object data
def req_handler(request, response):
    return Response(SlowIterator(response))

#Server handling of an http request
def server_data_xfer(srv,response,key):
    srv.expect_request(f"/testserver/{key}").respond_with_handler(lambda r: req_handler(r,response))
    return "http://localhost:{0}/testserver/{1}".format(srv.port,key)

#--------------------------------------------------------------------------
#Submit all the test cases as job
@action
def submit_jobs(default_condor,test_dir,path_to_sleep):
    job_handles = {}
    #For each test submit the job
    for key in TEST_CASES.keys():
        #Make a pytest HTTPServer
        server = HTTPServer()
        #Start server
        if not server.is_running():
            server.start()
        #Submit job
        job = default_condor.submit(
            {
                "executable": path_to_sleep,
                "arguments": "1",
                "log": (test_dir / "ran.log").as_posix(),
                "transfer_input_files": server_data_xfer(server,TEST_CASES[key],key),
                "should_transfer_files": "YES",
            }
        )
        #Add job to job_handle dictionaries
        job_handles[key] = (server,job)
    yield job_handles

#--------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def job_info(request, submit_jobs):
    return (request.param, submit_jobs[request.param][1],submit_jobs[request.param][0])
@action
#Get Job Name
def job_name(job_info):
    return job_info[0]
#Get job handle
@action
def job_handle(job_info):
    return job_info[1]
#Get server port that job is running on
@action
def job_server(job_info):
    return job_info[2]
#Wait for the job to finish
@action
def job_wait(job_handle,job_server):
    job_handle.wait(condition=ClusterState.all_terminal,timeout=80)
    #Stop server if running
    if job_server.is_running:
        job_server.stop()
    return job_handle

#==========================================================================
class TestMultifileCurlTimeout:
    def test_xfer_timeout(self,job_name,job_wait):
        if job_name[:4] == "pass":
            job_wait.state[0] == JobStatus.COMPLETED
        elif job_name[:4] == "fail":
            assert job_wait.state[0] == JobStatus.HELD
        else:
            print(f"\nERROR: Invalid test name '{job_name}'. Please change to either 'pass_{job_name}' or 'fail_{job_name}'.")
            assert False

    #Verify test results
    def test_check_xfer_history(self,xfer_history):
        timeout_errors = 0
        for line in xfer_history:
            #Check found error
            if "TransferError =" in line:
                #If is timeout as expected increase count
                if "Aborted due to lack of progress" in line:
                    timeout_errors += 1
                #Else we aren't expecting any other error so write failure message and fail test
                else:
                    err = line.split("=")[1].strip()
                    print(f"\nERROR: Unexpected TransferError ({err}) found in transfer_history.")
                    assert False
        #Verify that the number of transfer timeout tests are equal to the number of timeout errors found
        fail_count = len({k for k in TEST_CASES.keys() if k[:4] == "fail"})
        assert fail_count == timeout_errors



