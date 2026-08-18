#!/usr/bin/env pytest

from ornithology import *
import htcondor2

class FailTestException(Exception):
    pass

#------------------------------------------------------------------
TEST_DAG_FILE = "empty.dag"

TEST_CASES_PY = [
    {
        "SaveFile": "DNE.save",
        "DoRecovery": True,
    },
    {
        "SaveFile": "DNE.save",
        "DoRescueFrom": 1,
    },
    {
        "SaveFile": "DNE.save",
        "RescueFile": "DNE.rescue",
    },
    {
        "RescueFile": "DNE.rescue",
        "DoRecovery": True,
        "SaveFile": "DNE.save",
    },
    {
        "RescueFile": "DNE.rescue",
        "DoRescueFrom": 1,
    }
]

TEST_CASES_CLI = [
    ["-load_save", "DNE.save", "-DoRecovery"],
    ["-load_save", "DNE.save", "-DoRescueFrom", 1],
    ["-load_save", "DNE.save", "-RescueFile", "DNE.rescue"],
    ["-DoRecovery", "-load_save", "DNE.save"],
    ["-DoRescueFrom", 1, "-load_save", "DNE.save"],
    ["-RescueFile", "DNE.rescue", "-load_save", "DNE.save"],
    ["-DoRecovery", "-RescueFile", "DNE.rescue", "-load_save", "DNE.save"],
    ["-RescueFile", "DNE.rescue", "-DoRescueFrom", 1],
    ["-DoRescueFrom", 1, "-RescueFile", "DNE.rescue"],
]

#------------------------------------------------------------------
@action
def the_test_dag():
    with open(TEST_DAG_FILE, "w") as f:
        f.write("# This is an empty DAG file\n")

#------------------------------------------------------------------
@action(params={f"Test{i}": opts for i, opts in enumerate(TEST_CASES_PY)})
def the_python_test_case_options(the_test_dag, request):
    return request.param

#------------------------------------------------------------------
@action(params={f"Test{i}": opts for i, opts in enumerate(TEST_CASES_CLI)})
def the_cli_test_case_flags(the_test_dag, request):
    return request.param

#==================================================================
class TestDAGManOptionsMutualExclusion:
    def test_python_api(self, default_condor, the_python_test_case_options):
        try:
            with default_condor.use_config():
                # This should raise a RuntimeError with a specific message
                htcondor2.Submit.from_dag(TEST_DAG_FILE, the_python_test_case_options)
            raise FailTestException
        except Exception as e:
            assert isinstance(e, RuntimeError)
            assert "can not be used with the following option" in str(e)

    def test_condor_submit_dag(self, default_condor, the_cli_test_case_flags):
        CMD = ["condor_submit_dag"]
        CMD.extend(the_cli_test_case_flags)
        CMD.append(TEST_DAG_FILE)
        p = default_condor.run_command(CMD)
        assert "can not be used with the following flag" in p.stderr
