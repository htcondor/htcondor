#!/usr/bin/env pytest

# test_dagman_inherit_attrs.py
#
# 

from ornithology import *
import htcondor2
import os

#--------------------------------------------------------------------------------------------
@standup
def condor(test_dir):
    with Condor(local_dir=test_dir / "condor",
                config={
                    "DAGMAN_INHERIT_ATTRS" : "TestAttr1,TestAttr2",
                    "DAGMAN_INHERIT_ATTRS_PREFIX" : "Orig_"
                }) as condor:
        yield condor

#--------------------------------------------------------------------------------------------
@action
def write_dags(path_to_sleep) -> str:
    MAIN_DAG = "test.dag"
    SUB_DAG = "sub.dag"
    with open(MAIN_DAG, "w") as f:
        f.write(f"""
JOB A {{
    executable = {path_to_sleep}
    arguments  = 1
}}
SUBDAG EXTERNAL B {SUB_DAG}
""")

    with open(SUB_DAG, "w") as f:
        f.write(f"""
JOB C {{
    executable = {path_to_sleep}
    arguments  = 1
}}
""")

    return MAIN_DAG

#--------------------------------------------------------------------------------------------
@action
def run_dag(condor, test_dir, write_dags):
    DAG_SUBMIT = htcondor2.Submit().from_dag(write_dags)
    DAG_SUBMIT["My.TestAttr1"] = "1"
    DAG_SUBMIT["My.TestAttr2"] = "true"

    DAG = condor.submit(DAG_SUBMIT)
    assert DAG.wait(condition=ClusterState.all_complete, timeout=60)

    hist_itr = None
    with condor.use_config():
         hist_itr = htcondor2.Schedd().history(constraint="Orig_TestAttr1=!=UNDEFINED && Orig_TestAttr2=!=UNDEFINED",
                                               projection=["DAGNodeName", "Orig_TestAttr1", "Orig_TestAttr2"])
    return hist_itr

#============================================================================================
class TestDAGManInheritAttrs:
    def test_dagman_inherit_attrs(self, run_dag):
        EXPECTED = ["A", "B", "C"]
        for ad in run_dag:
            node_name = ad.get("DAGNodeName")
            assert node_name is not None
            assert node_name in EXPECTED
            EXPECTED.remove(node_name)
            assert ad.get("Orig_TestAttr1") is 1
            assert ad.get("Orig_TestAttr2") is True
        assert len(EXPECTED) == 0

