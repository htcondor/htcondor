#!/usr/bin/env pytest

import pytest

import os
import subprocess
from pathlib import Path

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
	in_order,
)


@pytest.fixture
def expected():
	return [
		"(D_TEST) Handler #0 called.",
		"(D_TEST) Handler #1 called.",
		"(D_TEST) --- separator ---",

		"(D_TEST) Handler #1 called.",
		"(D_TEST) --- separator ---",

		"(D_TEST) Handler #2 called.",
		"(D_TEST) Handler #1 called.",
		"(D_TEST) --- separator ---",

		"(D_TEST) Handler #2 called.",
		"(D_TEST) --- separator ---",

		"(D_TEST) Handler #2 called.",
		"(D_TEST) Handler #3 called.",
		"(D_TEST) --- separator ---",

		"(D_TEST) Passed test 00.",
	]


@pytest.fixture
def results():
	env = {
		** os.environ,
		'_CONDOR_TOOL_DEBUG': 'D_CATEGORY D_TEST',
	}

	# Daemon core REALLY wants to run in the LOG directory.
	log_directory = Path("./local_dir/log")
	log_directory.mkdir(parents=True, exist_ok=True)

	return subprocess.run(
		["./test_for_AwaitableDeadlineSignal.exe", "-p", "0", "-t"],
		env=env,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		universal_newlines=True,
		timeout=60
	)


@pytest.fixture
def returncode(results):
	return results.returncode


@pytest.fixture
def outputlines(results):
	return results.stdout.splitlines()


def test_AwaitableDeadlineSignal(returncode, outputlines, expected):
	for line in outputlines:
		logger.debug(line)

	assert returncode == 0

	output_line_suffices = ["".join(line.partition("(D_TEST)")[1:]) for line in outputlines]
	assert in_order(output_line_suffices, expected)
