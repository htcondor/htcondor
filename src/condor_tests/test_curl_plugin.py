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


@action
def start_log(test_dir):
    start_log = open(test_dir / "condor" / "log" / "StartLog", "r")
    contents = start_log.readlines()
    start_log.close()
    return contents


@action(params={"404": 404, "500": 500})
def bad_url(server, request):
    server.expect_request("/badurl").respond_with_data(status=request.param)
    return "http://localhost:{}/badurl".format(server.port)


@action
def good_url(server):
    server.expect_request("/goodurl").respond_with_data("Great success!")
    return "http://localhost:{}/goodurl".format(server.port)


@action
def multiple_urls(server):
    server.expect_request("/url1").respond_with_data("url1")
    server.expect_request("/url2").respond_with_data("url2")
    server.expect_request("/url3").respond_with_data("url3")
    return "http://localhost:{}/url1, http://localhost:{}/url2, http://localhost:{}/url3".format(server.port, server.port, server.port)


@action
def partial_content(server):
    server.expect_request("/partialcontent").r
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

@action
def job_with_multiple_urls(default_condor, multiple_urls, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "1",
            "log": (test_dir / "multiple_urls.log").as_posix(),
            "transfer_input_files": multiple_urls,
            "transfer_output_files": "url1, url2, url3",
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

    def test_job_with_multiple_urls_succeeds(self, job_with_multiple_urls):
        assert job_with_multiple_urls.state[0] == JobStatus.COMPLETED

    def test_job_with_multiple_urls_invokes_plugin_once(self, job_with_multiple_urls, start_log):
        plugin_invocations = 0
        for line in start_log:
            if "invoking" in line:
                plugin_invocations += 1
        # We actually expect to see 5 invocations of the curl_plugin because all the other tests are logged in the same file.
        # TODO: Figure out a better way to isolate this so we're only looking for a single invocation
        assert plugin_invocations == 5
