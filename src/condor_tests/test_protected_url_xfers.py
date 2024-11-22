#!/usr/bin/env pytest

#   test_protected_url_xfers.py
#
#   Test to check admin mapped protected URLs to specific queues
#   create the appropriate job ad attributes, and grab/release
#   transfer queue slots at the appriopriate times.
#   Written by: Cole Bollig
#   Written:    2023-09-21


from ornithology import *
import htcondor2 as htcondor
import os
from time import time
from time import sleep
from pathlib import Path


HISTORY_PROJECTION = [
    "ProcId",
    "TransferInput",
    "TransferInputFrom_GLUE",
    "TransferQueueInputList",
]

INPUT_FILES = {
    "local"    : ["file-0.in", "file-1.in", "file-2.in", "file-3.in"],
    "waves_n"  : ["alpha.wav", "beta.wav", "gamma.wav"],
    "remote"   : ["a.in", "b.in"],
    "normal"   : ["foo.txt", "bar.txt", "baz.txt"],
    "specific" : {
        "glue"  : ["data-0.zip", "data-2.zip"],
        "zork"  : ["data-1.zip"],
        "local" : ["data-3.zip", "data-4.zip"],
        "phaser": ["data-5.zip"],
    },
}

BAD_EVENTS = {
    htcondor.JobEventType.JOB_HELD,
    htcondor.JobEventType.JOB_ABORTED,
}

def make_input_files(t_dir):
    ret = {}
    ignore_queues = ["normal", "specific"]
    input_dir = os.path.join(t_dir, "input_files")

    if not os.path.exists(input_dir):
        os.mkdir(input_dir)

    for queue, data in INPUT_FILES.items():
        os.chdir(input_dir)
        dir_path = os.path.join(input_dir, queue)
        if not os.path.exists(dir_path):
            os.mkdir(dir_path)
        os.chdir(dir_path)

        if type(data) is list:
            for filename in data:
                with open(filename, "w") as f:
                    f.write(f"{queue} Input file!\n")
            if queue not in ignore_queues:
                ret.update({queue : [dir_path]})
        elif type(data) is dict:
            for specific_q, files in data.items():
                for filename in files:
                    with open(filename, "w") as f:
                        f.write(f"Specific Input File for {specific_q}\n")
                    i = filename.rfind(".")
                    specific_path = os.path.join(dir_path, filename[:i])
                    if specific_q in ret:
                        ret[specific_q].append(specific_path)
                    else:
                        ret.update({specific_q : [specific_path]})

    return ret


@standup
def make_map(test_dir):
    protected_urls = make_input_files(str(test_dir))
    map_file = os.path.join(str(test_dir), "protected_url.map")

    with open(map_file, "w") as f:
        for queue, paths in protected_urls.items():
            for path in paths:
                f.write(f"null  {path:<175}  {queue}\n")
        special_case = os.path.join(str(test_dir), "input_files", "normal", "foo")
        f.write(f"null  {special_case:<175}  *\n")

    return map_file


@standup
def condor(test_dir, make_map):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "PROTECTED_URL_TRANSFER_MAPFILE" : make_map,
            "DAGMAN_USE_DIRECT_SUBMIT" : True,
        }
    ) as condor:
        yield condor


class XferTestInfo:
    def __init__(self, submit_func, expected_attrs, xfer_in=[]):
        self.submit = submit_func
        self.expected = expected_attrs
        self.in_files = xfer_in


def write_submit(test_name, desc, num_procs=1):
    file_name = f"{test_name}.sub"

    with open(file_name, "w") as f:
        for key, val in desc.items():
            f.write(f"{key} = {val}\n")
        f.write(f"\nqueue {num_procs}\n")

    return file_name


def standard_submit(condor, test, job_desc):
    submit_file = write_submit(test, job_desc)
    p = condor.run_command(["condor_submit", submit_file])
    return parse_submit_result(p)


def submit_dag(condor, test, job_desc):
    job_sub = write_submit(test, job_desc)
    dag_filename = "test.dag"
    with open(dag_filename, "w") as f:
        f.write(f"JOB XFER {job_sub}\n")
    dag_handle = condor.submit_dag(dag_filename)
    assert dag_handle.wait(condition=ClusterState.all_running, timeout=30)
    start = time()
    while True:
        assert time() - start < 15
        try:
            with open("job.log", "r") as f:
                pass
            break
        except FileNotFoundError:
            pass
        sleep(1)
    return (dag_handle.clusterid+1, 1)


def submit_python(condor, test, job_desc):
    handle = None
    with condor.use_config():
        schedd = htcondor.Schedd()
        job = htcondor.Submit(job_desc)
        handle = schedd.submit(job)
    assert handle is not None
    return (handle.cluster(), handle.num_procs())


def submit_python_old(condor, test, job_desc):
    clusterid = -1
    with condor.use_config():
        schedd = htcondor.Schedd()
        job = htcondor.Submit(job_desc)
        result = schedd.submit(job, count=1)
        clusterid = result.cluster()
    assert clusterid > 0
    return (clusterid, 1)


def submit_py_old_itr(condor, test, job_desc):
    handle = None
    with condor.use_config():
        schedd = htcondor.Schedd()
        per_proc_xfer = [{"transfer_input_files" : f"../input_files/normal/baz.txt,{i}"} for i in job_desc["transfer_input_files"].split(",")]
        del job_desc["transfer_input_files"]
        job = htcondor.Submit(job_desc)
        handle = schedd.submit(job, itemdata=iter(per_proc_xfer))
    assert handle is not None
    return (handle.cluster(), handle.num_procs())


def submit_late_materialization(condor, test, job_desc):
    job_desc.update({"max_idle" : 6})
    submit_file = write_submit(test, job_desc, num_procs=6)
    p = condor.run_command(["condor_submit", submit_file])
    return parse_submit_result(p)


TEST_CASES = {
    "condor_submit" : XferTestInfo(standard_submit,
                                   { "TransferIn" : {
                                       0 : {
                                           "local" : ["foo.txt"],
                                           "DEFAULT" : ["bar.txt"],
                                           "remote" : ["a.in", "b.in"]
                                       }
                                   }},
                                   xfer_in=[
                                       "null://{}/input_files/normal/foo.txt",
                                       "../input_files/normal/bar.txt",
                                       "null://{}/input_files/remote/a.in",
                                       "null://{}/input_files/remote/b.in"
                                   ]),
    "dagman_direct" : XferTestInfo(submit_dag,
                                   { "TransferIn" : {
                                       0 : {
                                           "DEFAULT" : ["baz.txt", "gamma.wav"],
                                           "local"   : ["file-2.in", "file-1.in"],
                                           "waves_n" : ["alpha.wav"],
                                       }
                                   }},
                                   xfer_in=[
                                       "../input_files/normal/baz.txt",
                                       "../input_files/waves_n/gamma.wav",
                                       "null://{}/input_files/local/file-2.in",
                                       "null://{}/input_files/local/file-1.in",
                                       "null://{}/input_files/waves_n/alpha.wav",
                                   ]),
    "py_bindings" : XferTestInfo(submit_python,
                                { "TransferIn" : {
                                    0 : {
                                        "DEFAULT" : ["foo.txt", "file-3.in"],
                                        "remote" : ["b.in"],
                                        "waves_n" : ["beta.wav", "gamma.wav"],
                                    }
                                }},
                                xfer_in=[
                                    "../input_files/normal/foo.txt",
                                    "../input_files/local/file-3.in",
                                    "null://{}/input_files/waves_n/beta.wav",
                                    "null://{}/input_files/waves_n/gamma.wav",
                                    "null://{}/input_files/remote/b.in",
                                ]),
    "old_py_bindings" : XferTestInfo(submit_python_old,
                                    { "TransferIn" : {
                                        0 : {
                                            "DEFAULT" : ["data-0.zip"],
                                            "zork" : ["data-1.zip"],
                                            "local": ["foo.txt", "file-0.in", "data-3.zip"],
                                            "glue" : ["data-2.zip"],
                                        }
                                    }},
                                    xfer_in=[
                                        "../input_files/specific/data-0.zip",
                                        "null://{}/input_files/normal/foo.txt",
                                        "null://{}/input_files/local/file-0.in",
                                        "null://{}/input_files/specific/data-2.zip",
                                        "null://{}/input_files/specific/data-1.zip",
                                        "null://{}/input_files/specific/data-3.zip"
                                    ]),
    "old_itr_python" : XferTestInfo(submit_py_old_itr,
                                   { "TransferIn" : {
                                       0 : {
                                           "DEFAULT" : ["baz.txt"],
                                           "local" : ["file-2.in"],
                                       },
                                       1 : {
                                           "DEFAULT" : ["baz.txt"],
                                           "local" : ["foo.txt"],
                                       },
                                       2 : {
                                           "DEFAULT" : ["baz.txt"],
                                           "zork" : ["data-1.zip"],
                                       },
                                       3 : {
                                           "DEFAULT" : ["baz.txt"],
                                           "waves_n" : ["alpha.wav"],
                                       },
                                   }},
                                   xfer_in=[
                                       "null://{}/input_files/local/file-2.in",
                                       "null://{}/input_files/normal/foo.txt",
                                       "null://{}/input_files/specific/data-1.zip",
                                       "null://{}/input_files/waves_n/alpha.wav",
                                   ]),
    "late_materialization" : XferTestInfo(submit_late_materialization,
                                         { "TransferIn" : {
                                             proc : {
                                                 "DEFAULT" : ["bar.txt"],
                                                 "local" : ["file-1.in", "file-2.in"],
                                                 "waves_n" : ["beta.wav"],
                                                 "remote" : ["b.in"],
                                             } for proc in range(6)
                                         }},
                                         xfer_in=[
                                             "../input_files/normal/bar.txt",
                                             "null://{}/input_files/local/file-1.in",
                                             "null://{}/input_files/local/file-2.in",
                                             "null://{}/input_files/waves_n/beta.wav",
                                             "null://{}/input_files/remote/b.in",
                                         ]),
    "lat_mat_var_input" : XferTestInfo(submit_late_materialization,
                                      { "TransferIn" : {
                                          0 : {
                                              "DEFAULT" : ["baz.txt"],
                                              "local" : ["foo.txt"],
                                              "glue" : ["data-0.zip"],
                                          },
                                          1 : {
                                              "DEFAULT" : ["baz.txt"],
                                              "local" : ["foo.txt"],
                                              "zork"  : ["data-1.zip"],
                                              "EMPTY" : ["glue"],
                                          },
                                          2 : {
                                              "DEFAULT" : ["baz.txt"],
                                              "local" : ["foo.txt"],
                                              "glue" : ["data-2.zip"],
                                          },
                                          3 : {
                                              "DEFAULT" : ["baz.txt"],
                                              "local" : ["foo.txt", "data-3.zip"],
                                              "EMPTY" : ["glue"],
                                          },
                                          4 : {
                                              "DEFAULT" : ["baz.txt"],
                                              "local" : ["foo.txt", "data-4.zip"],
                                              "EMPTY" : ["glue"],
                                          },
                                          5 : {
                                              "DEFAULT" : ["baz.txt"],
                                              "local" : ["foo.txt"],
                                              "phaser": ["data-5.zip"],
                                              "EMPTY" : ["glue"],
                                          },
                                      }},
                                      xfer_in=[
                                          "../input_files/normal/baz.txt",
                                          "null://{}/input_files/normal/foo.txt",
                                          "null://{}/input_files/specific/data-$(ProcId).zip",
                                      ]),
}


@action
def path_to_job(test_dir):
    job_path = os.path.join(str(test_dir), "birb.py")
    with open(job_path, "w") as f:
        f.write("""#!/usr/bin/python3

from time import sleep
import os
import sys

with open("data.txt", "w") as f:
    f.write("foo\\n")

with open("data2.txt", "w") as f:
    f.write("bar\\n")

for file in os.listdir():
    print(f"{file}")

if len(sys.argv) > 1:
    sleep(int(sys.argv[1]))
""")
    os.chmod(job_path, 0o755)
    return job_path


@action
def submit_jobs(condor, test_dir, path_to_job):
    test_runs = {}
    LOG_NAME = "job.log"
    JOB_DESC_COMMON = {
        "executable": str(path_to_job),
        "universe"  : "vanilla",
        "output" : "ls.out",
        "log"    : LOG_NAME,
        "should_transfer_files"  : "YES",
        "transfer_output_files"  : "data.txt,data2.txt",
        "transfer_output_remaps" : '"data.txt=null:///dev/null;data2.txt=null:///dev/null"',
    }

    for case, info in TEST_CASES.items():
        os.chdir(str(test_dir))
        test_path = os.path.join(str(test_dir), case)
        log = os.path.join(test_path, LOG_NAME)

        if not os.path.exists(test_path):
            os.mkdir(test_path)
        os.chdir(test_path)

        if os.path.exists(log):
            os.remove(log)

        desc = JOB_DESC_COMMON
        desc.update({
            "transfer_input_files" : ",".join([f if not f.startswith("null://") else f.format(str(test_dir)) for f in info.in_files]),
        })

        job_info = info.submit(condor, case, desc)
        test_runs[case] = (job_info, log, info.expected)

    yield test_runs


@action(params={name: name for name in TEST_CASES})
def run_jobs(request, submit_jobs):
    return (request.param, submit_jobs[request.param][0], submit_jobs[request.param][1], submit_jobs[request.param][2])


@action
def test_name(run_jobs):
    return run_jobs[0]


@action
def test_wait(condor, run_jobs):
    clusterid, num_procs = run_jobs[1]
    jobids = [f"{clusterid}.{n}" for n in range(num_procs)]
    jel = htcondor.JobEventLog(run_jobs[2])
    start_t = time()
    while True:
        assert time() - start_t <= 180
        if len(jobids) == 0:
            break
        for event in jel.events(stop_after=1):
            assert event.cluster == clusterid
            assert event.proc < num_procs
            assert event.type not in BAD_EVENTS
            if event.type == htcondor.JobEventType.JOB_TERMINATED:
                jobids.remove(f"{event.cluster}.{event.proc}")
    ads = None
    with condor.use_config():
        ads = htcondor.Schedd().history(constraint=f"ClusterId=={clusterid}", projection=HISTORY_PROJECTION)
    return ads


@action
def test_check(run_jobs):
    return run_jobs[3]


class TestProtectedUrlXfers:
    def test_attribute_creation(self, test_wait, test_check):
        for ad in test_wait:
            proc = ad.get("ProcId")
            assert proc is not None
            if "TransferIn" in test_check:
                list_attrs = [str(attr) for attr in ad.get("TransferQueueInputList", [])]
                assert proc in test_check["TransferIn"]
                for queue, files in test_check["TransferIn"][proc].items():
                    if queue == "DEFAULT":
                        assert "TransferInput" in ad
                        for xfer in ad["TransferInput"].split(","):
                            xfer.replace("$(ProcId)", str(proc))
                            assert Path(xfer).name in files
                    elif queue == "EMPTY":
                        for q in files:
                            attr = "TransferInputFrom_" + q.upper()
                            assert attr in ad
                            assert ad[attr] == ""
                    else:
                        list_name = "TransferInputFrom_" + queue.upper()
                        assert list_name in list_attrs
                        list_attrs.remove(list_name)
                        assert list_name in ad
                        for xfer in ad[list_name].split(","):
                            xfer.replace("$(ProcId)", str(proc))
                            assert Path(xfer).name in files
                assert len(list_attrs) == 0


