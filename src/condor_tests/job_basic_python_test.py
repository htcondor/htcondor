#!/usr/bin/env python

import os
import sys

from pytest.CondorPytest import CondorPytest
from pytest.CondorUtils import CondorUtils

def main():

    # Generate a submit file
    submit_filename = "job_basic_python_test.sub"
    submit_contents = """
        executable  = /bin/echo
        output      = job_basic_python_test.out
        error       = job_basic_python_test.err
        arguments   = \"Basic Python test submission, cluster $(cluster)\"
        queue
    """
    submit_file = open(submit_filename, "w")
    submit_file.write(submit_contents)
    submit_file.close()

    # Now instantiate a new test and send it the submit filename
    test = CondorPytest("job_basic_python_test")
    result = test.RunTest(submit_filename)
    CondorUtils.Debug("Test returned result: " + str(result))

if __name__ == "__main__":
    main()