#!/usr/bin/env pytest

from ornithology import (
    action,
    ClusterState,
    Condor,
)

import socket

from pytest_httpserver import HTTPServer
from werkzeug.wrappers import Response

import time


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "ENABLE_URL_TRANSFERS": "TRUE",
            "FILETRANSFER_PLUGINS": "$(LIBEXEC)/curl_plugin",
        },
    ) as condor:
        yield condor


TEST_CASES = {
    "CURLE_URL_MALFORMAT": {
        "expected": {
            "ErrorType":        "Parameter",
            "PluginLaunched":   True,
            "PluginVersion":    "0.2",
        },
        "url":    "file://$(staging/match_metacompare/xac_plots.pdf",
    },
    # Let's not even and say we didn't.
    # CURLE_COULDNT_RESOLVE_PROXY
    "CURLE_COULDNT_RESOLVE_HOST": {
        "expected": {
            "ErrorType":        "Resolution",
            "FailedName":       "no-such.tld",
            "FailureType":      "Unknown",
        },
        "url":    "http://no-such.tld/file",
    },
    # I don't know how to generate this one, actually.
    # CURLE_REMOTE_ACCESS_DENIED
    "CURLE_FILE_COULDNT_READ_FILE": {
        "expected": {
            "ErrorType":        "Specification",
        },
        "url":    "file:///no-such.file",
    },
    "HTTP_400": {
        "expected": {
            "ErrorType":        "Specification",
            "FailedServer":     "localhost:{open_port}",
        },
        "url":    "http://localhost:{open_port}/400",
    },
    "HTTP_403": {
        "expected": {
            "ErrorType":        "Authorization",
            "FailureType":      "Authorization",
            "FailedServer":     "localhost:{open_port}",
        },
        "url":    "http://localhost:{open_port}/403",
    },
    "HTTP_404": {
        "expected": {
            "ErrorType":        "Specification",
            "FailedServer":     "localhost:{open_port}",
        },
        "url":    "http://localhost:{open_port}/404",
    },
    "CURLE_ABORTED_BY_CALLBACK": {
        "expected": {
            "ErrorType":        "Transfer",
            "FailedServer":     "Unknown",
            "FailureType":      "TooSlow",
        },
        "url":    "http://localhost:{slow_port}/slow",
    },

#    "CURLE_TOO_MANY_REDIRECTS": {
#        "expected": {
#            "ErrorType":        "Specification",
#            "FailedServer":     "Unknown",
#        },
#        "url":    "http://localhost:{redirect_port}/tmr",
#    },

    "CURLE_COULDNT_CONNECT": {
        "expected": {
            "ErrorType":        "Contact",
            "FailedServer":     "Unknown",
        },
        "url":    "http://localhost:{closed_port}/file",
    },

    # I have no idea what triggers these.
    # CURLE_SEND_ERROR
    # CURLE_RECV_ERROR
    # Let's not worry about setting https up in just the wrong way quite yet.
    # CURLE_PEER_FAILED_VERIFICATION
    # Let's not worry about setting https up correctly just yet.
    # CURLE_SSL_CONNECT_ERROR
}


@action
def the_job_description(path_to_sleep):
    return {
        "LeaveJobInQueue":              "true",
        "executable":                   path_to_sleep,
        "transfer_executable":          "true",
        "arguments":                    "20",
        "when_to_transfer_output":      "ON_EXIT",
        "should_transfer_files":        "true",
        "environment":                  "CURL_MAX_RETRY_ATTEMPTS=1;http_proxy="
    }


@action
def the_closed_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('localhost', 0))
    (address, port) = s.getsockname()
    yield port
    s.close()


def slow_reply_handler(request):
    time.sleep(32)
    return Response()


@action
def the_open_port():
    server = HTTPServer()
    if not server.is_running():
        server.start()

    # Arguably, all of these kind of lines throughout this whole test
    # should be in the corresponding test case, instead.
    server.expect_request("/400").respond_with_data("", status=400)
    server.expect_request("/403").respond_with_data("", status=403)
    server.expect_request("/404").respond_with_data("", status=404)


    yield server.port

    server.stop()


@action
def the_slow_port():
    server = HTTPServer()
    if not server.is_running():
        server.start()

    # This must be its own server, because otherwise the order of the requests
    # matter, since each server is single-threaded.
    server.expect_request("/slow").respond_with_handler(slow_reply_handler)

    yield server.port

    server.stop()


@action
def the_redirect_port():
    server = HTTPServer()
    if not server.is_running():
        server.start()

    # Joyously, curl doesn't respond to this consistently: some of the time
    # it reports error 78 (CURLE_REMOTE_FILE_NOT_FOUND) and some of the time
    # it reports error 47 (CURLE_TOO_MANY_REDIRECTS).  (This is assuming the
    # plug-in has been modified to set CURLOPT_MAXREDIRS, which doesn't have
    # a default value in older versions of libcurl.)

    server.expect_request("/tmr").respond_with_data("", status=302,
        headers=[{"Location", f"http://localhost:{server.port}/tms"}]
    )
    server.expect_request("/tms").respond_with_data("", status=302,
        headers=[{"Location", f"http://localhost:{server.port}/tmr"}]
    )

    yield server.port

    server.stop()


@action
def the_job_handles(test_dir, the_condor, the_job_description, the_closed_port, the_open_port, the_slow_port, the_redirect_port):
    job_handles = {}

    for name, test_case in TEST_CASES.items():
        complete_job_description = {
            ** the_job_description,
            "transfer_input_files": test_case["url"].format(
                closed_port=the_closed_port,
                open_port=the_open_port,
                slow_port=the_slow_port,
                redirect_port=the_redirect_port,
            ),
            "log": (test_dir / f"{name}.log").as_posix(),
        }
        job_handle = the_condor.submit(
            description = complete_job_description,
            count = 1
        )

        job_handles[name] = job_handle

    yield job_handles


@action(params={name: name for name in TEST_CASES})
def test_case_pair(request, the_job_handles):
    return (request.param, the_job_handles[request.param])


@action
def test_name(test_case_pair):
    return test_case_pair[0]


@action
def test_job(test_case_pair):
    return test_case_pair[1]


@action
def test_case(test_case_pair):
    return TEST_CASES[test_case_pair[0]]


@action
def expected_value(test_case, the_closed_port, the_open_port):
    expected_value = test_case["expected"]
    for key in expected_value.keys():
        if isinstance(expected_value[key], str):
            expected_value[key] = expected_value[key].format(
                closed_port=the_closed_port, open_port=the_open_port
            )
    return expected_value


@action
def actual_value(the_condor, test_job):
    assert test_job.wait(
        timeout=60,
        condition=ClusterState.all_held,
        fail_condition=ClusterState.any_complete,
    )

    history = the_condor.get_local_schedd().jobEpochHistory(
        f"ClusterId=={test_job.clusterid}",
        ad_type="INPUT",
    )

    return history[0]["InputPluginResultList"][0]["TransferErrorData"][0]


class TestCurlStructuredError:

    def test_epoch_log_as_expected(self, expected_value, actual_value):
        for key, value in expected_value.items():
            assert actual_value.get(key) == value
