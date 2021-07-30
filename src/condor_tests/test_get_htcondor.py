#!/usr/bin/env pytest

#
# This is NOT a regression test.  This is a post-release acceptance test,
# verifying some minimal functionality of get_htcondor.
#

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
        "ubuntu:18.04",
        "ubuntu:20.04",
        "centos:7",
        "centos:8",
        "amazonlinux:2",
    ],
    "current": [
        "ubuntu:18.04",
        "ubuntu:20.04",
        "debian:10",
        "centos:7",
        "centos:8",
        "amazonlinux:2",
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
    },
    # "default": {
    #     "flag": "",
    # },
    # "no_dry_run": {
    #     "flag": "--no-dry-run",
    # },
    "minicondor": {
        "flag": "--minicondor --no-dry-run",
    },
}

PREFICES_BY_IMAGE = {
    "debian:9" : "apt-get update && apt-get install -y curl",
    "debian:10" : "apt-get update && apt-get install -y curl",
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


# This should avoid any potential problems with concurrently pulling the
# same container image... but it may count as a pull request even if you
# already have the latest image?
@action
def cached_container_image(container_image):
    # cp = subprocess.run(['docker', 'pull', container_image])
    # assert(cp.returncode == 0)
    return container_image


@action
def results_from_container(channel, cached_container_image, flag):
    platform_specific_prefix = ""
    if cached_container_image in PREFICES_BY_IMAGE:
        platform_specific_prefix = PREFICES_BY_IMAGE[cached_container_image] + " && "

    # The 'set -o pipefail' is bash magic to make the scriptlet return
    # the failure if any command in the pipe fails.  This is super-useful
    # for catching a failure in/of curl.
    args  = [ "docker", "run", "-t", cached_container_image, "/bin/bash", "-c",
              "set -o pipefail; " + platform_specific_prefix
              + "curl -fsSL " + THE_URL
              + " | /bin/bash -s -- " + f"--channel {channel} " + flag,
            ]
    logger.debug(args)

    return subprocess.run(args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=150)


# We can parameterize further to string(s) required to be in the log,
# or with functions which inspect the log.
class TestGetHTCondor:
    def test_results_from_container(self, results_from_container):
        logger.debug(results_from_container.stdout)
        assert(results_from_container.returncode == 0)
