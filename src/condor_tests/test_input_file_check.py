#!/usr/bin/env pytest

import os

from pytest_httpserver import HTTPServer
from werkzeug.wrappers import Response

from ornithology import (
    action,
    Condor,
    ClusterState,
)


# Unset HTTP_PROXY for correct operation in Docker containers
lowered = dict()
for k in os.environ:
    lowered[k.lower()] = k
os.environ.pop(lowered.get("http_proxy", "http_proxy"), None)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "STARTD_ENVIRONMENT":           ";http_proxy=;https_proxy=",
            "SHADOW_DEBUG":                 "D_TEST D_CATEGORY",
        },
    ) as the_condor:
        yield the_condor

@action
def the_server():
    with HTTPServer() as server:
        yield server


def the_response_function():
    for word in ['the', 'sizeless', 'response']:
        yield word


@action
def the_URLs(the_server):
    the_server.expect_request("/absent").respond_with_data(status=404)
    the_server.expect_request("/present").respond_with_data("Great success!")

    # You MUST invoke a generator here; the documenation lies when it says
    # that any iterable will "switch into streaming mode", which for our
    # purposes just means "will stop generating a Content-Length header".
    r = Response(
        response=the_response_function(),
    )
    the_server.expect_request("/sizeless").respond_with_response(r)

    return (
        f"http://localhost:{the_server.port}/absent",
        f"http://localhost:{the_server.port}/present",
        f"http://localhost:{the_server.port}/sizeless",
    )


# This function is rather magical.
@action
def the_expected_results(test_dir, the_URLs):
    (absentURL, presentURL, presentSizelessURL) = the_URLs
    input_text = test_dir / "input.text"

    the_list = [
        [input_text.as_posix(), "true", "0", "true"],
        [absentURL,             "false", None, "false"],
        [presentURL,            "true", "14", "true"],
        [presentSizelessURL,    "true", None, "false"],
    ]

    return { entry[0]: entry for entry in the_list }


@action
def the_held_job(the_condor, test_dir, path_to_sleep, the_URLs):
    (absentURL, presentURL, presentSizelessURL) = the_URLs
    input_text = test_dir / "input.text"
    input_text.touch()

    the_description = {
        "universe":                 "vanilla",
        "executable":               path_to_sleep,
        "arguments":                1,
        "transfer_executable":      False,
        "should_transfer_files":    True,
        "log":                      test_dir / "sleep.log",
        "request_cpus":             1,
        "request_memory":           1,
        "transfer_input_files":     ",".join([
            input_text.as_posix(),
            # We can't presently reliably test osdf or https.
            absentURL,
            presentURL,
            presentSizelessURL,
        ]),
    }
    job_handle = the_condor.submit(the_description, count=1)

    assert job_handle.wait(
        condition=ClusterState.all_held,
        fail_condition=ClusterState.any_complete,
        timeout=60,
    )

    return job_handle


class TestInputFileCheck:

    def test_check_table(self, the_held_job, the_condor, the_expected_results, the_URLs):
        the_shadow_log = the_condor.shadow_log.open()
        for line in the_shadow_log.read():
            if "D_TEST" in line.tags:
                if "checkInputFileTransfer():" in line.line:
                    # Remove the prefix from the list.
                    actual = line.line.split("\t")[1:]
                    expected = the_expected_results[actual[0]]

                    # If the size isn't known, change the reported size so
                    # this code doesn't have to know what (size_t)-1 is.
                    if expected[3] != "true":
                        actual[2] = None

                    logger.debug(f"  actual = {actual}")
                    logger.debug(f"expected = {expected}")

                    assert actual == expected
