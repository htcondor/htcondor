#!/usr/bin/env python

import os
import sys

from pytest.CondorPytest import CondorPytest
from pytest.CondorUtils import CondorUtils

def main():

    # Setup submit arguments
    submit_args = {
        "executable":   "/bin/echo",
        "output":       "job_basic_python_test.out",
        "error":        "job_basic_python_test.err",
        "arguments":    "\"Basic Python test submission, cluster $(cluster)\""
    }

    # Now instantiate a new test and send it the submit args
    test = CondorPytest("job_basic_python_test")
    result = test.RunTest(submit_args)
    CondorUtils.Debug("Test returned result: " + str(result))

if __name__ == "__main__":
    main()