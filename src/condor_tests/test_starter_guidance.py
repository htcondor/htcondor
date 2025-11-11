#!/usr/bin/env pytest

import os
import pytest
from pathlib import Path
import subprocess
import htcondor2

#
# Runs the C++ test_starter_guidance.exe, a unit test.
#
class TestStarterGuidance:

    def test_unit_test(self, test_dir):
        the_path = Path(htcondor2.param['LIBEXEC']) / "test_starter_guidance.exe"
        the_env = {
            ** os.environ,
            "_CONDOR_LOG": test_dir.as_posix(),
        }

        rv = subprocess.run(
            [the_path, "-f", "-t"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20,
            universal_newlines=True,
            env=the_env,
        )

        print(rv.stdout)
        print(rv.stderr)

        assert rv.returncode == 0
