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

@action
def server():
    with HTTPServer() as httpserver: 
        yield httpserver

@action(params={"404":404, "500":500})
def bad_url(server, request):
    server.expect_request("/badurl").respond_with_data(status = request.param)
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


class TestCurlPlugin:
    def test_server(self, httpserver):
        assert httpserver

    def test_job_with_good_url_succeeds(self, job_with_good_url, test_dir):
        assert job_with_good_url.state[0] == JobStatus.COMPLETED

    def test_job_with_bad_url_holds(self, job_with_bad_url):
        assert job_with_bad_url.state[0] == JobStatus.HELD
