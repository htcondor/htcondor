#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# make sure that file transfer plugins are enabled (might be disabled by default)
	ENABLE_URL_TRANSFERS = true
	FILETRANSFER_PLUGINS = $(LIBEXEC)/curl_plugin $(LIBEXEC)/data_plugin
	# test expects at least 2 different SlotIds
	use FEATURE : StaticSlots
	# make sure invoking line is printed
	STARTER_DEBUG = $(STARTER_DEBUG) D_ALWAYS:2
"""
#endtestreq


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
def slot2_starter_log(test_dir):
    start_log = open(test_dir / "condor" / "log" / "StarterLog.slot2", "r")
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
def good_file_url(test_dir):
    ( test_dir / "file_url_tester" ).write_text("curl supports file urls")
    return "file://" + str(test_dir) + "/file_url_tester"

@action
def multiple_good_urls(server):
    server.expect_request("/goodurl1").respond_with_data("goodurl1")
    server.expect_request("/goodurl2").respond_with_data("goodurl2")
    server.expect_request("/goodurl3").respond_with_data("goodurl3")
    return "http://localhost:{}/goodurl1, http://localhost:{}/goodurl2, http://localhost:{}/goodurl3".format(server.port, server.port, server.port)


@action
def multiple_bad_urls(server):
    # We want at least one url to work correctly
    server.expect_request("/badurl1").respond_with_data("badurl1")
    # But the rest of them should return a 500 error
    server.expect_request("/badurl2").respond_with_data(status=500)
    server.expect_request("/badurl3").respond_with_data(status=500)
    return "http://localhost:{}/badurl1, http://localhost:{}/badurl2, http://localhost:{}/badurl3".format(server.port, server.port, server.port)


@action
def job_with_good_url(default_condor, good_url, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": (test_dir / "good_url.log").as_posix(),
            "transfer_input_files": good_url,
            "transfer_output_files": "goodurl",
            "should_transfer_files": "YES",
            "requirements": "(SlotID == 1)"
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job

@action
def job_with_good_file_url(default_condor, good_file_url, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": (test_dir / "good_file_url.log").as_posix(),
            "transfer_input_files": good_file_url,
            "transfer_output_files": "file_url_tester",
            "should_transfer_files": "YES",
            "requirements": "(SlotID == 1)"
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job

@action
def job_with_bad_url(default_condor, bad_url, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": (test_dir / "bad_url.log").as_posix(),
            "transfer_input_files": bad_url,
            "should_transfer_files": "YES",
            "requirements": "(SlotID == 1)"
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job


@action
def job_with_multiple_good_urls(default_condor, multiple_good_urls, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": (test_dir / "multiple_good_urls.log").as_posix(),
            "transfer_input_files": multiple_good_urls,
            "transfer_output_files": "goodurl1, goodurl2, goodurl3",
            "should_transfer_files": "YES",
            # We want to run this specific test on slot2 so that all invocations
            # of curl_plugin are isolated from the other tests
            "requirements": "(SlotID == 2)"
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job


@action
def job_with_multiple_bad_urls(default_condor, multiple_bad_urls, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": (test_dir / "multiple_bad_urls.log").as_posix(),
            "transfer_input_files": multiple_bad_urls,
            "transfer_output_files": "badurl1, badurl2, badurl3",
            "should_transfer_files": "YES",
            "requirements": "(SlotID == 1)"
        }
    )
    assert job.wait(condition=ClusterState.all_terminal)
    return job


@action
def job_with_bad_remaps(default_condor, test_dir, path_to_sleep):
    job = default_condor.submit(
        {
            "executable": path_to_sleep,
            "arguments": "0",
            "log": (test_dir / "job_with_bad_remap.log").as_posix(),
            "transfer_output_files": "missing_file",
            "transfer_output_remaps": '"missing_file = file:///tmp/missing_file"',
            "should_transfer_files": "YES",
            # Should this be 2?
            "requirements": "(SlotID == 1)"
        }
    )
    assert job.wait(
        timeout=60,
        condition=ClusterState.all_held,
    )
    return job


class TestCurlPlugin:
    def test_job_with_good_url_succeeds(self, job_with_good_url):
        assert job_with_good_url.state[0] == JobStatus.COMPLETED

    def test_job_with_good_url_file_contents_are_correct(
        self, job_with_good_url, test_dir
    ):
        assert Path("goodurl").read_text() == "Great success!"

    def test_job_with_good_file_url_succeeds(self, job_with_good_file_url):
        assert job_with_good_file_url.state[0] == JobStatus.COMPLETED

    def test_job_with_good_file_url_file_contents_are_correct(
        self, job_with_good_file_url, test_dir
    ):
        assert Path("file_url_tester").read_text() == "curl supports file urls"

    def test_job_with_bad_url_holds(self, job_with_bad_url):
        assert job_with_bad_url.state[0] == JobStatus.HELD

    def test_job_with_multiple_good_urls_succeeds(self, job_with_multiple_good_urls):
        assert job_with_multiple_good_urls.state[0] == JobStatus.COMPLETED

    def test_job_with_multiple_good_urls_invokes_plugin_once(self, job_with_multiple_good_urls, slot2_starter_log):
        plugin_invocations = 0
        for line in slot2_starter_log:
            if "FILETRANSFER: invoking:" in line:
                plugin_invocations += 1
        assert plugin_invocations == 1

    def test_job_with_multiple_bad_urls_holds(self, job_with_multiple_bad_urls):
        assert job_with_multiple_bad_urls.state[0] == JobStatus.HELD


    def test_job_with_bad_remaps(self, job_with_bad_remaps):
        assert job_with_bad_remaps.state[0] == JobStatus.HELD

        hold_reasons = job_with_bad_remaps.query(
            projection=["HoldReason"],
        )
        assert len(hold_reasons) == 1

        the_hold_reason = hold_reasons[0]['HoldReason']
        assert "did not produce valid response" not in the_hold_reason
        assert "could not open local file" in the_hold_reason
