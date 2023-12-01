#!/usr/bin/env pytest

#   test_dagman_futile_nodes.py
#
#   Test to verify the setting of futile nodes upon the failure of a parent node.
#   The test cases are set up to test correct setting of node statuses with various
#   expected failure and successful DAGs along with one test to verify the setting
#   of distant descendats in a more 'complex' DAG structure
#   Written by: Cole Bollig
#   Written:    2022-11-30


from ornithology import *
import htcondor2 as htcondor
import os
from pathlib import Path

#------------------------------------------------------------------
#Standup a personal condor for always run post scripts in DAGMan
@standup
def post_condor(test_dir):
    with Condor(
      local_dir=test_dir / "arp_condor",
      config={
        "DAGMAN_ALWAYS_RUN_POST": "True",
        "DAGMAN_USER_LOG_SCAN_INTERVAL": 1,
        "DAGMAN_MAX_SUBMIT_ATTEMPTS": 1,
        "DAGMAN_VERBOSITY": 7,
        "DAGMAN_USE_STRICT": 0,
      }
    ) as condor:
        yield condor

#------------------------------------------------------------------
#Contents of a python script that will fail due to exit code
@action
def fail_script():
    script = f"""#!/usr/bin/env python3
print("Script Failed")
exit(1)
"""
    return script

#Contents of a python script that will succeed due to exit code
@action
def pass_script():
    script = f"""#!/usr/bin/env python3
print("Script Succeeded")
exit(0)
"""
    return script

#Job submit contents file that will be aborted
@action
def abort_job_desc(path_to_sleep):
    description = f"""
executable = {path_to_sleep}
arguments  = 0
log        = test.log
#This test needs to be in vanilla universe to have a shadow
universe   = vanilla
periodic_remove = NumShadowStarts >= 1
queue
"""
    return description

#Job submit contents file that will end in failure due to invalid input
@action
def failed_job_desc(path_to_sleep):
    description = f"""
executable = {path_to_sleep}
arguments  = bad_input
universe   = local
log        = test.log
queue
"""
    return description

#Job submit file contents that should run and end normally/successfully
@action
def good_job_desc(path_to_sleep):
    description = f"""
executable = {path_to_sleep}
arguments  = 0
universe   = local
log        = test.log
queue
"""
    return description

#------------------------------------------------------------------
#Dictionary of all test cases
#Set up as Test_Name:Dictionary of files{filename:content/content_specifier}
#The test DAG file should always be named 'test.dag'
#Also make sure a node status file called 'status.out' is added
#Note: ARP stands for 'Always Run Post'. We expect these DAGS to finish normally.
#      In ARP cases if there is actually a post script then it should always
#      use the 'pass_script' contents. As it defeats the purpose to fail one
#      way and then just fail a different way.
TEST_CASES = {
    #Normal post script failure
    "Post_Script_Failed":{
        "post.py":"Post Fail",
        "job.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B job.sub
JOB C job.sub
SCRIPT POST A post.py
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Job aborted for some reason
    "Job_Aborted":{
        "job.sub":"Abort Desc",
        "good.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B good.sub
JOB C good.sub
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Job aborted but will always run post script
    "ARP_Job_Aborted":{
        "post.py":"Post Pass",
        "job.sub":"Abort Desc",
        "good.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B good.sub
JOB C good.sub
SCRIPT POST A post.py
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Job failed
    "Job_Failed":{
        "job.sub":"Error Desc",
        "good.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B good.sub
JOB C good.sub
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Job failed but will always run post script
    "ARP_Job_Failed":{
        "post.py":"Post Pass",
        "job.sub":"Error Desc",
        "good.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B good.sub
JOB C good.sub
SCRIPT POST A post.py
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Failed job submission due to non-existant submit file
    "Failed_Submission":{
        "job.sub":"Good Desc",
        "test.dag":"""
JOB A DNE.sub
JOB B job.sub
JOB C job.sub
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Failed job submission but will always run post script
    "ARP_Failed_Submit":{
        "post.py":"Post Pass",
        "job.sub":"Good Desc",
        "test.dag":"""
JOB A DNE.sub
JOB B job.sub
JOB C job.sub
SCRIPT POST A post.py
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Pre script failure
    "Pre_Script_Failed":{
        "pre.py":"Pre Fail",
        "job.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B job.sub
JOB C job.sub
SCRIPT PRE A pre.py
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Pre script failure but with PRE_SKIP enabled
    #Cheaty marking of 'ARP' to designate successful DAG
    "ARP_Pre_Skip":{
        "pre.py":"Pre Fail",
        "job.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B job.sub
JOB C job.sub
SCRIPT PRE A pre.py
PRE_SKIP A 1
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Pre script failure but will always run post script
    "ARP_Pre_Script_Failed":{
        "pre.py":"Pre Fail",
        "post.py":"Post Pass",
        "job.sub":"Good Desc",
        "test.dag":"""
JOB A job.sub
JOB B job.sub
JOB C job.sub
SCRIPT PRE A pre.py
SCRIPT POST A post.py
PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Complex DAG structure where one node fails to check all
    #descendants are set to futile
    "Count_Complex_DAG":{
        "bad.sub":"Error Desc",
        "good.sub":"Good Desc",
        "test.dag":"""
JOB A good.sub
JOB B good.sub
JOB C bad.sub
JOB D good.sub
JOB E good.sub
JOB F good.sub
JOB G good.sub
JOB H good.sub
JOB I good.sub
JOB J good.sub
JOB K good.sub

PARENT A CHILD E F
PARENT B CHILD F
PARENT C CHILD F G
PARENT D CHILD I
PARENT E CHILD H
PARENT F CHILD I
PARENT H CHILD K
PARENT I CHILD J
PARENT J CHILD K
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
    #Make sure user defined DONE nodes aren't overwritten to FUTILE
    "Pre_Done_Node":{
        "bad.sub":"Error Desc",
        "good.sub":"Good Desc",
        "test.dag":"""
JOB A bad.sub
JOB B good.sub
JOB C good.sub DONE

PARENT A CHILD B
PARENT B CHILD C
NODE_STATUS_FILE status.out 5 ALWAYS-UPDATE""",
    },
}

#------------------------------------------------------------------
@action
def submit_jobs(post_condor, test_dir,
                fail_script, pass_script, abort_job_desc,
                failed_job_desc, good_job_desc):
    #Dictionary of DAG job handles to yield
    job_handles = {}
    for key,info in TEST_CASES.items():
        #Return to test directory
        os.chdir(str(test_dir))
        #Make new sub-directory name based on test case name
        new_dir = (test_dir / key).as_posix()
        #If this sub-driectory exists remove it
        if os.path.exists(new_dir):
            os.rmdir(new_dir)
        #Make new sub-directory and move into it
        os.mkdir(new_dir)
        os.chdir(new_dir)
        dag_file = ""
        for name,data in info.items():
            filename = (test_dir / key / name).as_posix()
            #Write 'test.dag' file with specified data
            if name == "test.dag":
                dag_file = write_file(filename, data)
            else:
                #Make a new file based on filename
                f = open(filename,"w")
                #If data states Fail write 'fail_script' contents
                if "Fail" in data:
                    f.write(fail_script)
                    os.chmod(filename, 0o777)
                #If data states pass write 'pass_script' contents
                elif "Pass" in data:
                    f.write(pass_script)
                    os.chmod(filename, 0o777)
                #If data contains keyword "Desc" then write submit file
                elif "Desc" in data:
                    #Data states "Good" so use 'good_job_desc' contents
                    if "Good" in data:
                        f.write(good_job_desc)
                    #Data states "Abort" so use 'abort_job_desc' contents
                    elif "Abort" in data:
                        f.write(abort_job_desc)
                    #Data states "Error" so use 'fail_job_desc' contents
                    #Can't use keyword "Fail" because it will be confused as a pre/post script
                    elif "Error" in data:
                        f.write(failed_job_desc)
                f.close()
        #Make sure a 'test.dag' file was written
        assert dag_file != ""
        #Submit DAG job and add to job_handles dictionary
        with post_condor.use_config():
            dag = htcondor.Submit.from_dag(str(dag_file))
        dagman_job = post_condor.submit(dag)
        job_handles[key] = dagman_job
    print("")
    yield job_handles

#------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def job_info(request, submit_jobs):
    return (request.param, submit_jobs[request.param])
#Get dag job test name
@action
def job_name(job_info):
    return job_info[0]
#Get dag job handle
@action
def job_handle(job_info):
    return job_info[1]
#Wait for the dag job to finish
@action
def job_wait(job_handle):
    assert job_handle.wait(condition=ClusterState.all_complete,timeout=80)
    return job_handle

#==================================================================
class TestDAGManFutileNodes:
    def test_dag(self,test_dir,job_name,job_wait):
        #Make path to this tests node status file
        path = (test_dir / job_name / "status.out").as_posix()
        status = ""
        #Attempt to open file and read contents
        try:
            f = open(path,"r")
            status = f.readlines()
            f.close()
        except:
            #Failed somewhere in opening and reading node status file
            print(f"Error: Failed to read {path}")
            assert False
        #Dictionary of counts for expected node status
        counts = {
            "done":0,
            "error":0,
            "futile":0,
        }
        #Read file content for node status and update counts
        for line in status:
            if "NodeStatus =" in line:
                if "STATUS_FUTILE" in line:
                    counts["futile"] += 1
                elif "STATUS_DONE" in line:
                    counts["done"] += 1
                elif "STATUS_ERROR" in line:
                    counts["error"] += 1
                #If non-of the expected status' appear then must be unexpected
                #so fail the test
                else:
                    line = line.split(";")[1].strip()
                    print(f"Error: Unexpected node status found ({line})")
                    assert False
        #Sanity check out put of Test->Node status counts
        print(f"\t{job_name} node status counts: {counts}")
        #Check node status counts
        if job_name[:3] == "ARP":
            #An 'ARP' test should result in 3 Done and no Error or Futile nodes
            assert counts["done"] == 3 and counts["error"] == 0 and counts["futile"] == 0
        elif job_name == "Count_Complex_DAG":
            #Special complex DAG case should result in 5 done, 1 error, and 5 futile nodes
            assert counts["done"] == 5 and counts["error"] == 1 and counts["futile"] == 5
        elif job_name == "Pre_Done_Node":
            #Special case: make sure pre set done nodes aren't overwritten to futile
            assert counts["done"] == 1 and counts["error"] == 1 and counts["futile"] == 1
        else:
            #All other tests should result in 0 Done, 1 Error, and 2 futile nodes
            assert counts["done"] == 0 and counts["error"] == 1 and counts["futile"] == 2




