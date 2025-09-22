#!/usr/bin/env pytest

import pytest

import os
from pathlib import Path
import subprocess

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    action,
)


TEST_CASES = {
    "on_and_on": {
        "env": {
            '_CONDOR_CONTAINER_IMAGES_COMMON_BY_DEFAULT': 'TRUE'
        },
        "sub": "container_is_common = true",
        "expected": "_x_catalog_condor_container_image",
    },
    "on_and_off": {
        "env": {
            '_CONDOR_CONTAINER_IMAGES_COMMON_BY_DEFAULT': 'TRUE'
        },
        "sub": "container_is_common = false",
        "expected": "TransferInput",
    },
    "on_and_unspec": {
        "env": {
            '_CONDOR_CONTAINER_IMAGES_COMMON_BY_DEFAULT': 'TRUE'
        },
        "sub": "",
        "expected": "_x_catalog_condor_container_image",
    },

    "off_and_on": {
        "env": {
            '_CONDOR_CONTAINER_IMAGES_COMMON_BY_DEFAULT': 'TRUE'
        },
        "sub": "container_is_common = true",
        "expected": "_x_catalog_condor_container_image",
    },
    "off_and_off": {
        "env": {
            '_CONDOR_CONTAINER_IMAGES_COMMON_BY_DEFAULT': 'FALSE'
        },
        "sub": "container_is_common = false",
        "expected": "TransferInput",
    },
    "off_and_unspec": {
        "env": {
            '_CONDOR_CONTAINER_IMAGES_COMMON_BY_DEFAULT': 'FALSE'
        },
        "sub": "",
        "expected": "TransferInput",
    },
}


@action(params={name: name for name in TEST_CASES})
def the_test_pair(request):
    return request.param, TEST_CASES[request.param]


@action
def the_test_name(the_test_pair):
    return the_test_pair[0]


@action
def the_test_case(the_test_pair):
    return the_test_pair[1]


@action
def the_test_result(the_test_name, the_test_case):
    subtext = f"""
        universe = vanilla
        container_image = busybox.sif
        shell = ls -la
        should_transfer_files = YES
        {the_test_case['sub']}
        queue
    """

    env = {
        ** os.environ,
        ** the_test_case['env'],
    }

    rv = subprocess.run(
        ['condor_submit', '-', '-dry', '-'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        input=subtext,
        text=True,
        env=env,
        timeout=2,
    )

    for line in rv.stdout.splitlines():
        if 'ContainerImage' in line:
            continue
        if 'busybox.sif' in line:
            return line.split('=')[0]


class TestCIFKnob:

    def test_correct_job_ad(self, the_test_case, the_test_result):
        assert the_test_result == the_test_case['expected']
