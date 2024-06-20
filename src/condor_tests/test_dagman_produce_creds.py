#!/usr/bin/env pytest

from ornithology import *
import os
from shutil import rmtree, move
from getpass import getuser

import htcondor2

TOKEN_FILES = ["scitokens.top", "scitokens.use"]
USERNAME = getuser()
TEST_CONFIG = {
    "SEC_CREDENTIAL_DIRECTORY_OAUTH":       "$(LOCAL_DIR)/oauth_credentials",
    "CREDMON_OAUTH_LOG":                    "$(LOG)/CredMonOAuthLog",
    "DAEMON_LIST":                          "$(DAEMON_LIST),CREDMON_OAUTH",
    "AUTO_INCLUDE_CREDD_IN_DAEMON_LIST":    "True",
    "TRUST_CREDENTIAL_DIRECTORY":           "True",
    "LOCAL_CREDMON_PROVIDER_NAME":          "scitokens",
    "LOCAL_CREDMON_TOKEN_AUDIENCE":         "https://localhost",
    "CREDD_PORT":                           "-1",
    "ALLOW_DAEMON":                         "*",
    "LOCAL_CREDMON_PRIVATE_KEY":            "$(LOCAL_DIR)/trust_domain_ca_privkey.pem",
    "CREDD_DEBUG":                          "D_FULLDEBUG",
    "DAGMAN_MAX_SUBMIT_ATTEMPTS":           "1",
    "DAGMAN_PRODUCE_JOB_CREDENTIALS":       "True",
}

@standup
def condor(test_dir):
    with Condor(local_dir=test_dir / "condor", config=TEST_CONFIG) as condor:
        yield condor

def write_submit(exe: str):
    filename = "sleep.sub"
    with open(filename, "w") as f:
        f.write(
        f"""
        executable = {exe}
        arguments  = 1
        log        = job.log
        queue
        """)
    return filename

def write_dag(submit: str):
    filename = "test.dag"
    with open(filename, "w") as f:
        f.write(f"JOB ONLY {submit}\n")
    return filename

TEST_CASES = {
    "shell_condor_submit" : {
        "DAG-Options" : {"SubmitMethod" : 0},
    },
    "direct_submit" : {
        "DAG-Options" : {"SubmitMethod" : 1},
    },
}

@action(params={name: name for name in TEST_CASES})
def run_dag(condor, path_to_sleep, test_dir, request):
    os.chdir(str(test_dir))
    sub_dir = os.path.join(str(test_dir), request.param)
    if os.path.exists(sub_dir):
        rmtree(sub_dir)
    os.mkdir(sub_dir)
    os.chdir(sub_dir)

    submit_file = write_submit(str(path_to_sleep))
    dag_file = write_dag(submit_file)

    with condor.use_config():
        creds_dir = htcondor2.param["SEC_CREDENTIAL_DIRECTORY_OAUTH"]
        DAG = htcondor2.Submit.from_dag(dag_file, options=TEST_CASES[request.param]["DAG-Options"])

    for token_file in TOKEN_FILES:
        path = os.path.join(creds_dir, USERNAME, token_file)
        assert not os.path.exists(path)

    dag_handle = condor.submit(DAG)
    assert dag_handle.wait(condition=ClusterState.all_complete, timeout=60)

    for token_file in TOKEN_FILES:
        path = os.path.join(creds_dir, USERNAME, token_file)
        try:
            move(path, ".")
        except:
            pass

    return sub_dir


class TestDAGManCredentials:
    def test_dagman_produce_creds(self, run_dag):
        dir_contents = os.listdir(run_dag)
        for token_file in TOKEN_FILES:
            assert token_file in dir_contents
