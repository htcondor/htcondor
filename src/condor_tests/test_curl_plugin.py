#!/usr/bin/env pytest

import logging
import os
from pathlib import Path

from pytest_httpserver import HTTPServer

from ornithology import (
    config,
    standup,
    action,
    JobStatus,
    ClusterState,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Unset HTTP_PROXY for correct operation in Docker containers
lowered = dict()
for k in os.environ:
    lowered[k.lower()] = k
os.environ.pop(lowered.get("http_proxy", "http_proxy"), None)


@action
def server():
    with HTTPServer() as httpserver:
        yield httpserver


@action(params={"404": 404, "500": 500})
def bad_url(server, request):
    server.expect_request("/badurl").respond_with_data(status=request.param)
    return "http://localhost:{}/badurl".format(server.port)


@action
def good_url(server):
    server.expect_request("/goodurl").respond_with_data("Great success!")
    return "http://localhost:{}/goodurl".format(server.port)


@action
def job_with_good_url(default_condor, good_url, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "1",
            "log": (test_dir / "good_url.log").as_posix(),
            "transfer_input_files": good_url,
            "transfer_output_files": "goodurl",
            "should_transfer_files": "YES",
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job


@action
def job_with_bad_url(default_condor, bad_url, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "1",
            "log": (test_dir / "bad_url.log").as_posix(),
            "transfer_input_files": bad_url,
            "should_transfer_files": "YES",
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job


class TestCurlPlugin:
    def test_job_with_good_url_succeeds(self, job_with_good_url):
        assert job_with_good_url.state[0] == JobStatus.COMPLETED

    def test_job_with_good_url_file_contents_are_correct(
        self, job_with_good_url, test_dir
    ):
        assert Path("goodurl").read_text() == "Great success!"

    def test_job_with_bad_url_holds(self, job_with_bad_url):
        assert job_with_bad_url.state[0] == JobStatus.HELD
