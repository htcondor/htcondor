#!/usr/bin/env pytest

#   test_dagman_node_status_formats.py
#
#   Verifies DAGMan's NODE_STATUS_FILE supports all four print formats
#   (CLASSAD/JSON, each pretty or COMPACT) and that each produces a
#   parseable, correct status file. Runs on all platforms.

import os
import json
from shutil import rmtree

from ornithology import *
import htcondor2 as htcondor
import classad2

DAG_FILENAME = "test.dag"
STATUS_FILENAME = "status.out"
TIMEOUT = 120

# case name -> (format keyword, compact keyword or "")
FORMAT_CASES = {
    "CLASSAD":         ("CLASSAD", ""),
    "CLASSAD_COMPACT": ("CLASSAD", "COMPACT"),
    "JSON":            ("JSON", ""),
    "JSON_COMPACT":    ("JSON", "COMPACT"),
}

#-----------------------------------------------------------------------------------------
def write_dag_file(fmt, compact_kw, path_to_sleep):
    contents = f"""
JOB A {{
    executable = {path_to_sleep}
    arguments  = 0
    universe   = local
    log        = node.log
    queue
}}
JOB B {{
    executable = {path_to_sleep}
    arguments  = 0
    universe   = local
    log        = node.log
    queue
}}
PARENT A CHILD B
NODE_STATUS_FILE {STATUS_FILENAME} 1 ALWAYS-UPDATE {fmt} {compact_kw}
"""
    with open(DAG_FILENAME, "w") as f:
        f.write(contents)
    return DAG_FILENAME

#-----------------------------------------------------------------------------------------
def read_json_ads(path):
    """Read a stream of one-or-more (possibly pretty-printed) JSON objects
    concatenated in a single file, as DAGMan writes for NODE_STATUS_FILE JSON."""
    text = open(path, "r").read()
    decoder = json.JSONDecoder()
    ads = []
    idx, n = 0, len(text)
    while idx < n:
        while idx < n and text[idx].isspace():
            idx += 1
        if idx >= n:
            break
        ad, end = decoder.raw_decode(text, idx)
        ads.append(ad)
        idx = end
    return ads

def read_status_ads(path, fmt):
    if fmt == "JSON":
        return read_json_ads(path)
    return list(classad2.parseAds(open(path, "r")))

#-----------------------------------------------------------------------------------------
@action
def run_dags(default_condor, test_dir, path_to_sleep):
    assert path_to_sleep is not None

    handles = dict()

    for case, (fmt, compact_kw) in FORMAT_CASES.items():
        case_dir = test_dir / case
        if case_dir.exists():
            rmtree(case_dir)
        os.mkdir(case_dir)

        with ChangeDir(case):
            dag_file = write_dag_file(fmt, compact_kw, path_to_sleep)

            with default_condor.use_config():
                dag = htcondor.Submit.from_dag(dag_file)

            handles[case] = default_condor.submit(dag)

    yield handles

#-----------------------------------------------------------------------------------------
@action(params={name: name for name in FORMAT_CASES})
def status_case(request) -> str:
    return request.param

@action
def status_fmt(status_case) -> str:
    return FORMAT_CASES[status_case][0]

@action
def dag_handle(status_case, run_dags):
    handle = run_dags[status_case]
    assert handle.wait(condition=ClusterState.all_complete, timeout=TIMEOUT)
    return handle

#===========================================================================================
class TestDAGManNodeStatusFormats:
    def test_status_file_parses_and_is_correct(self, status_case, status_fmt, dag_handle):
        with ChangeDir(status_case):
            ads = read_status_ads(STATUS_FILENAME, status_fmt)

        by_type = {}
        for ad in ads:
            by_type.setdefault(ad.get("Type"), []).append(ad)

        assert len(by_type.get("DagStatus", [])) == 1
        assert len(by_type.get("StatusEnd", [])) == 1
        assert len(by_type.get("NodeStatus", [])) == 2

        dag_ad = by_type["DagStatus"][0]
        assert dag_ad["DagStatusName"] == "STATUS_DONE"
        assert dag_ad["NodesTotal"] == 2
        assert dag_ad["NodesDone"] == 2
        assert dag_ad["NodesFailed"] == 0

        nodes = {ad["Node"]: ad for ad in by_type["NodeStatus"]}
        assert set(nodes.keys()) == {"A", "B"}
        for ad in nodes.values():
            assert ad["NodeStatusName"] == "STATUS_DONE"
