#!/usr/bin/env pytest

import pytest

import os
import tempfile
import subprocess

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def test_filter_exe_results():
    rv = subprocess.run(["test_filter.exe"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=20)
    assert rv.returncode == 0
