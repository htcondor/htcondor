#!/usr/bin/env pytest

import logging
import pytest
import requests
import time

from conftest import config, standup, action
from pytest_httpserver import HTTPServer

from ornithology import (
    write_file,
    parse_submit_result,
    JobID,
    SetAttribute,
    SetJobStatus,
    JobStatus,
    in_order,
    SCRIPTS,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# MRC notes
 
"""
We want separate actions for the job that succeeds, and the job that fails
@action is something that Josh wrote
httpserver.expect_request should go in a fixture
Fixture is an argument to your test, it's an argument in python
Pre-conditions go into a fixture, if you want to verify this, assert it in the fixture
default_condor comes from ornithology, invoked from the parameter

"""



# MRC: I won't know the urls until we start the httpserver and bind to a port

@action
def submit_curl_plugin_job_cmd(test_dir, url, default_condor):
    sub_description = """
        executable = {exe}
        arguments = 1
        transfer_input_files = {url}
        should_transfer_files = YES
        
        queue
    """.format(
        exe=SCRIPTS["sleep"],
        url=url
    )
    submit_file = write_file(test_dir / "submit" / "job.sub", sub_description)

    return default_condor.run_command(["condor_submit", submit_file])


@action
def finished_curl_plugin_jobid(default_condor, submit_curl_plugin_job_cmd):
    clusterid, num_procs = parse_submit_result(submit_curl_plugin_job_cmd)

    jobid = JobID(clusterid, 0)

    default_condor.job_queue.wait_for_events(
        expected_events={jobid: [SetJobStatus(JobStatus.COMPLETED)]},
        unexpected_events={jobid: {SetJobStatus(JobStatus.HELD)}},
    )

    return jobid


@action
def job_queue_events_for_curl_plugin_job(default_condor, finished_curl_plugin_jobid):
    return default_condor.job_queue.by_jobid[finished_curl_plugin_jobid]

@action
def server():
    with HTTPServer() as httpserver: 
        yield httpserver

@action
def bad_url(server):
    server.expect_request("/badurl").respond_with_data(status = 500)
    return f"http://localhost:{server.port}/badurl"

@action
def good_url(server):
    server.expect_request("/goodurl").respond_with_data("Great success!")
    return f"http://localhost:{server.port}/goodurl"

@action
def job_with_good_url(default_condor, good_url, test_dir):
    job = default_condor.submit(
        {
            "executable": "/bin/sleep",
            "arguments": "1",
            "log": (test_dir / "good_url.log").as_posix(),
            "transfer_input_files": good_url,
            "should_transfer_files": "YES"
        }
    )
    job.wait(condition = lambda job: job.state.any_held() or job.state.any_complete())
    return job

@action
def job_with_bad_url(default_condor, bad_url, test_dir):
    job = default_condor.submit(
        {
            "executable": "/bin/sleep",
            "arguments": "1",
            "log": (test_dir / "bad_url.log").as_posix(),
            "transfer_input_files": bad_url,
            "should_transfer_files": "YES"
        }
    )
    job.wait(condition = lambda job: job.state.any_held() or job.state.any_complete())
    return job


class TestCanRunCurlPluginJob:
    def test_my_client(self, httpserver):
        # Set up the server to serve /foobar with the json
        print("\nHTTP server running on port {}\n".format(httpserver.port))
        httpserver.expect_request("/curlfilbade").respond_with_data("Great success!")
        #time.sleep(60)
        # Check that the request is served
        #assert requests.get(httpserver.url_for("/foobar")).json() == {'foo': 'bar'}
        assert httpserver

    def test_job_with_good_url_succeeds(self, job_with_good_url):
        assert job_with_good_url.state[0] == JobStatus.COMPLETED

    def test_job_with_bad_url_holds(self, job_with_bad_url):
        assert job_with_bad_url.state[0] == JobStatus.HELD

    """

    def test_submit_cmd_succeeded(self, submit_curl_plugin_job_cmd):
        assert submit_curl_plugin_job_cmd.returncode == 0

    def test_only_one_proc(self, submit_curl_plugin_job_cmd):
        _, num_procs = parse_submit_result(submit_curl_plugin_job_cmd)
        assert num_procs == 1

    def test_job_queue_events_in_correct_order(self, job_queue_events_for_curl_plugin_job):
        assert in_order(
            job_queue_events_for_curl_plugin_job,
            [
                SetJobStatus(JobStatus.IDLE),
                SetJobStatus(JobStatus.RUNNING),
                SetJobStatus(JobStatus.COMPLETED),
            ],
        )

    def test_job_executed_successfully(self, job_queue_events_for_curl_plugin_job):
        assert SetAttribute("ExitCode", "0") in job_queue_events_for_curl_plugin_job
    """
