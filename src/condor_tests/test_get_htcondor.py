#!/usr/bin/env pytest

#
# This is NOT a regression test.  This is a post-release acceptance test,
# verifying some minimal functionality of get_htcondor.  Running the
# entire suite takes quite a while, but you can use pytest's -k option
# to select a container image to test (and then test images concurrently):
#
#  pytest test_get_htcondor.py -k ubuntu:10
#
# You can also specify a channel to test:
#
#  pytest test_get_htcondor.py -k stable
#
# or both an image and a channel:
#
#  pytest test_get_htcondor.py -k "ubuntu:20.04 and stable"
#
# To get information about which tarball the download tests actually
# got, run the download tests with the INFO debug level:
#
#  pytest test_get_htcondor.py -k download --log-cli-level INFO
#
# If you're developing get_htcondor and have made the new version
# available via http at some-address.tld/get_htcondor, you can run
# set THE_URL in this test's environment to test the new version:
#
#  THE_URL=http://some-address.tld/get_htcondor pytest test_get_htcondor.py
#

import os
import subprocess
import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# This is completely unnecessary, but I'm used to using Ornithology's
# fixtures now.
from ornithology import (
    config,
    standup,
    action,
)


# You can change this to simplify testing new version of get_htcondor.
THE_URL = "https://get.htcondor.org"
if "THE_URL" in os.environ:
    THE_URL = os.environ["THE_URL"]

# PyTest gets the ordering wrong unless I make it explicit.  *sigh*
#
# IMAGES_BY_CHANNEL seems the most natural way to list those mappings.
#
# TESTS is currently too heavy-weight, but if the test(s) ever need to
# care about the log, it'll be easy to add a log-must-contain string or
# a flag-specific log-examining function.

IMAGES_BY_CHANNEL = {
    "stable": [
        "debian:9",
        "debian:10",
        "debian:11",
        "ubuntu:18.04",
        "ubuntu:20.04",
        "centos:7",
        "rockylinux:8",
        "amazonlinux:2",
        "scientificlinux/sl:7",
    ],
    "current": [
        "ubuntu:18.04",
        "ubuntu:20.04",
        "debian:10",
        "debian:11",
        "centos:7",
        "almalinux:8",
        "amazonlinux:2",
        "scientificlinux/sl:7",
        "arm64v8/almalinux:8",
        "ppc64le/almalinux:8",
    ],
}

#
# The --help test is essentially the null test; it can be helpful for
# testing the test, but doesn't tell us anything the other tests
# don't.  The --no-dry-run and --minicondor tests should be identical;
# if you're feeling paranoid, you can check, but it's not worth waiting
# around for if you're just smoke-testing a release.  The default test
# is also a no-op test, useful for debugging, but otherwise not worth
# the time.
#
TESTS = {
    # "help": {
    #    "flag": "--help",
    # },
    "download": {
        "flag": "--download",
        # Using 'head' screws up the exit code, so we can't just use
        # the name of the directory (that's printed on the first line).
        "postscript": "if command -v yum > /dev/null 2>&1; then " +
                          "yum install -y tar && yum install -y gzip; " +
                      "fi && " +
                      "tar -z -t -f condor.tar.gz | tail -1 | cut -d / -f 1",
        "postscript-lines": [-1],
    },
    # "default": {
    #     "flag": "",
    # },
    # "no_dry_run": {
    #     "flag": "--no-dry-run",
    # },
    "minicondor": {
        "flag": "--minicondor --no-dry-run",
        "postscript": "condor_version",
        "postscript-lines": [-2, -1],
    },
    # "distribution": {
    #     "flag": "--show-distribution",
    #     "postscript-lines": [-1],
    # },
}

PREFICES_BY_IMAGE = {
    "debian:9" : "apt-get update && apt-get install -y curl",
    "debian:10" : "apt-get update && apt-get install -y curl",
    "debian:11" : "apt-get update && apt-get install -y curl",
    "ubuntu:18.04": "apt-get update && apt-get install -y curl",
    "ubuntu:20.04": "apt-get update && apt-get install -y curl",
}

CHANNELS_BY_IMAGE = {}
for channel in IMAGES_BY_CHANNEL:
    for image in IMAGES_BY_CHANNEL[channel]:
        # This loop body feels un-Pythonic.
        if image not in CHANNELS_BY_IMAGE:
            CHANNELS_BY_IMAGE[image] = []
        CHANNELS_BY_IMAGE[image].append(channel)

PARAMS = {}
for image, channels in CHANNELS_BY_IMAGE.items():
    for channel in channels:
        for testname, test in TESTS.items():
            PARAMS[f"{image}_{channel}_{testname}"] = {
                "channel": channel,
                "image": image,
                "test": test,
            }


@config(params=PARAMS)
def the_test_case(request):
    return request.param


@config
def channel(the_test_case):
    return the_test_case["channel"]


@config
def container_image(the_test_case):
    return the_test_case["image"]


@config
def flag(the_test_case):
    return the_test_case["test"]["flag"]


@config
def postscript(the_test_case):
    if "postscript" in the_test_case["test"]:
        return the_test_case["test"]["postscript"]
    else:
        return None


@config
def postscript_lines(the_test_case):
    if "postscript-lines" in the_test_case["test"]:
        return the_test_case["test"]["postscript-lines"]
    else:
        return None


# This should avoid any potential problems with concurrently pulling the
# same container image... but it may count as a pull request even if you
# already have the latest image?
@action
def cached_container_image(container_image):
    # cp = subprocess.run(['docker', 'pull', container_image])
    # assert(cp.returncode == 0)
    return container_image


@action
def results_from_container(channel, cached_container_image, flag, postscript):
    platform_specific_prefix = ""
    if cached_container_image in PREFICES_BY_IMAGE:
        platform_specific_prefix = PREFICES_BY_IMAGE[cached_container_image]
        platform_specific_prefix += " && "

    # The 'set -o pipefail' is bash magic to make the scriptlet return
    # the failure if any command in the pipe fails.  This is super-useful
    # for catching a failure in/of curl.
    script = f"curl -fsSL {THE_URL} | /bin/bash -s -- --channel {channel} "
    script += flag
    if postscript is not None:
        script += f" && {postscript}"
    args  = [ "docker", "run", "--rm", "-t", cached_container_image, "/bin/bash", "-c",
              f"set -o pipefail; {platform_specific_prefix}{script}"
            ]
    logger.debug(args)

    return subprocess.run(args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=240)


# We can parameterize further to string(s) required to be in the log,
# or with functions which inspect the log.
class TestGetHTCondor:
    def test_results_from_container(self, results_from_container, postscript_lines):
        logger.debug(results_from_container.stdout)
        # There is undoubtedly a more-Pythonic way to do this, possibly
        # involving the parameter being a slice.
        if postscript_lines is not None:
            lines = results_from_container.stdout.splitlines()
            for line_number in postscript_lines:
                logger.info(lines[line_number])
        assert(results_from_container.returncode == 0)
