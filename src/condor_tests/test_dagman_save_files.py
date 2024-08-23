#!/usr/bin/env pytest

#   test_dagman_save_files.py
#
#   This tests the DAGMan save point file capabilities by running test cases
#   twice:
#        1. The first time to make sure that save files are written,
#           and written correctly.
#        2. A second time to check that the test DAGs restart correctly,
#           and rotate the old save files properly.
#   Written   : 2023-04-03
#   Written by: Cole Bollig

from ornithology import *
import htcondor2 as htcondor
import os
from pathlib import Path

#-----------------------------------------------------------------------------------------------
#All DAG files will be written as DAG_FILENAME in the test case name subdir
DAG_FILENAME = "test_save_files.dag"
#All Test Cases to run
TEST_CASES = {
#Test Basic declaration of node as a save point
    "declared_save" : {
        "DAG" : """
JOB A ../sleep.sub
JOB B ../sleep.sub
PARENT A CHILD B
SAVE_POINT_FILE A
SAVE_POINT_FILE B
""",
        "SAVE_FILES" : { f"save_files/A-{DAG_FILENAME}.save" : [],
                         f"save_files/B-{DAG_FILENAME}.save" : ["A"]},
        "LOAD_SAVE" : f"B-{DAG_FILENAME}.save",
        "RESTART_DONE" : 1,
        "FINAL_SAVES" : [f"save_files/A-{DAG_FILENAME}.save",f"save_files/B-{DAG_FILENAME}.save",f"save_files/B-{DAG_FILENAME}.save"],
    },
#Test save point file with custom name written to save_files
    "custom_name" : {
        "DAG" : """
JOB A ../sleep.sub
JOB B ../sleep.sub
PARENT A CHILD B
SAVE_POINT_FILE A redundant.save
SAVE_POINT_FILE B important.txt
""",
        "SAVE_FILES" : { "save_files/redundant.save" : [],
                         "save_files/important.txt" : ["A"]},
        "LOAD_SAVE" : "important.txt",
        "RESTART_DONE" : 1,
        "FINAL_SAVES" : ["save_files/redundant.save","save_files/important.txt","save_files/important.txt.old"],
    },
#Test save point file written to custom location
    "custom_path" : {
        "DAG" : """
JOB A ../sleep.sub
JOB B ../sleep.sub
PARENT A CHILD B
SAVE_POINT_FILE A ./irrelevant.save
SAVE_POINT_FILE B ../custom-path.save
""",
        "SAVE_FILES" : { "./irrelevant.save" : [],
                         "../custom-path.save" : ["A"] },
        "LOAD_SAVE" : "../custom-path.save",
        "RESTART_DONE" : 1,
        "FINAL_SAVES" : ["./irrelevant.save","../custom-path.save","../custom-path.save.old"]
    },
#Test a more complex DAG with writing and loading a save file
    "complex_test" : {
        "DAG" : """
JOB ONE ../sleep.sub
JOB TWO ../sleep.sub
JOB THREE ../sleep.sub
JOB FOUR ../sleep.sub
JOB JOIN ../sleep.sub
JOB LAST ../sleep.sub
PARENT ONE TWO THREE FOUR CHILD JOIN
PARENT JOIN CHILD LAST
SAVE_POINT_FILE JOIN complex.save
""",
        "SAVE_FILES" : { "save_files/complex.save" : ["ONE","TWO","THREE","FOUR"]},
        "LOAD_SAVE" : "complex.save",
        "RESTART_DONE" : 4,
        "FINAL_SAVES" : ["save_files/complex.save","save_files/complex.save.old"]
    },
}

#-----------------------------------------------------------------------------------------------
#Simple sleep job submit file for all test nodes to use
def writeJobSubmit(test_dir,path_to_sleep):
    filename = "sleep.sub"
    os.chdir(str(test_dir))
    if os.path.exists(filename):
        os.remove(filename)
    with open(filename,"w") as f:
        f.write("""
executable = {}
arguments  = 0
universe   = local
log        = job.log
queue
""".format(path_to_sleep))

#-----------------------------------------------------------------------------------------------
#Run all DAGs the first time
@action
def runDAGs(default_condor,test_dir,path_to_sleep):
    run_info = {}
    writeJobSubmit(test_dir,path_to_sleep)
    for test,info in TEST_CASES.items():
        #Return to test directory
        os.chdir(str(test_dir))
        #Make new sub-directory name based on test case name
        new_dir = (test_dir / test).as_posix()
        #If this sub-driectory exists remove it
        if os.path.exists(new_dir):
            os.rmdir(new_dir)
        #Make new sub-directory and move into it
        os.mkdir(new_dir)
        os.chdir(new_dir)
        #Write DAG file
        with open(DAG_FILENAME,"w") as f:
            f.write(info["DAG"])
        #Make sure a 'test.dag' file was written
        assert os.path.exists(DAG_FILENAME)
        #Submit DAG job and add to job_handles dictionary
        dag = htcondor.Submit.from_dag(str(DAG_FILENAME))
        dagman_job = default_condor.submit(dag)
        run_info[test] = (dagman_job,info["SAVE_FILES"])
    yield run_info

#-----------------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def runTests(request, runDAGs):
    return (request.param, runDAGs[request.param][0], runDAGs[request.param][1])
@action
def run_name(runTests):
    return runTests[0]
@action
def run_handle(runTests):
    return runTests[1]
@action
def run_params(runTests):
    return runTests[2]
@action
def run_wait(run_handle):
    assert run_handle.wait(condition=ClusterState.all_complete,timeout=60)
    return run_handle

#-----------------------------------------------------------------------------------------------
#Rerun all DAGs from the specified save file
@action
def rerunDAGs(default_condor,test_dir):
    run_info = {}
    for test,info in TEST_CASES.items():
        #Return to test directory
        os.chdir(str(test_dir))
        #Go to test case subdir
        new_dir = (test_dir / test).as_posix()
        assert os.path.exists(new_dir)
        os.chdir(new_dir)
        #Make sure a 'test.dag' file exists
        assert os.path.exists(DAG_FILENAME)
        #Option to load DAG from save file
        opts = {}
        opts["load_save"] = info["LOAD_SAVE"]
        #Submit DAG job and add to job_handles dictionary
        dag = htcondor.Submit.from_dag(str(DAG_FILENAME),opts)
        dagman_job = default_condor.submit(dag)
        run_info[test] = (dagman_job,info["RESTART_DONE"])
    yield run_info

#-----------------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def rerunTests(request, rerunDAGs):
    return (request.param, rerunDAGs[request.param][0], rerunDAGs[request.param][1])
@action
def rerun_name(rerunTests):
    return rerunTests[0]
@action
def rerun_handle(rerunTests):
    return rerunTests[1]
@action
def rerun_params(rerunTests):
    return rerunTests[2]
@action
def rerun_wait(rerun_handle):
    assert rerun_handle.wait(condition=ClusterState.all_complete,timeout=60)
    return rerun_handle

#===============================================================================================
class TestDAGManSaveFiles:
    #Test writing save point files
    def test_writing_saves(self,test_dir,run_name,run_params,run_wait):
        #For each expected save file to be written
        for save,expected in run_params.items():
            #Make sure save file exists
            file_path = (test_dir / run_name / save).as_posix()
            assert os.path.exists(file_path)
            #Check that all expected 'DONE nodename' lines exists
            with open(file_path,"r") as f:
                found_nodes = 0
                for line in f:
                    line = line.strip()
                    if line[0:4] == "DONE":
                        node = line.split(" ",1)[1].strip()
                        if len(expected) != 0 and node in expected:
                            found_nodes += 1
                        else:
                            print(f"ERROR: Unexpected node ({node}) found in save file: {save}")
                            break
            assert found_nodes == len(expected)
    #Test rerunning dag from save file
    def test_load_save(self,test_dir,rerun_name,rerun_params,rerun_wait):
        #Check the dagman.out file exists
        dagman_out = (test_dir / rerun_name / DAG_FILENAME).as_posix()
        assert os.path.exists(dagman_out)
        found_load_save = False
        #Read dagman.out file for key lines
        with open(dagman_out,"r") as f:
            for line in f:
                line = line.strip()
                #If we have found 'Loading saved progress from' line then check for output of number of DONE nodes
                if found_load_save:
                    if "Number of pre-completed nodes:" in line:
                        done_nodes = line.rsplit(":",1)[1].strip()
                        assert int(done_nodes) == rerun_params
                        break
                elif "Loading saved progress from" in line:
                    found_load_save
        #Verify that all save files exist including rotated files
        save_files = TEST_CASES[rerun_name]["FINAL_SAVES"]
        for save in save_files:
            path = (test_dir / rerun_name / save).as_posix()
            assert os.path.exists(path)



