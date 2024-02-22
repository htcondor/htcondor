#!/usr/bin/env pytest

#	Test condor_history ability to read Job Epoch files
#   -Run a 2 simple crondors and verify outputs for various
#    condor_history commands related to epoch files.
#   Written By: Cole Bollig
#   Written:    2022-09-16

from ornithology import *
import os
import htcondor
from pathlib import Path
from time import sleep
from time import time

#Global variables
epochDir = "epochs"
setUpSuccess = True
historyTestNum = -1
outputFiles = [
    "hist_file_epoch.txt",
    "hist_file_cluster.txt",
    "hist_file_cluster_proc.txt",
    "hist_file_jid_match_regression.txt",
    "base_epoch.txt",
    "cluster.txt",
    "cluster_proc.txt",
    "dlt_limit.txt",
    "dlt_match.txt",
    "dlt_scanlimit.txt",
    "delete.txt"
]
expectedOutput = {
    #Key=outfileName : Expected in file : count of lines expected
    #First check looks for epoch like history file
    "hist_file_epoch.txt":{"1.0 1.1 2.0 2.1":8},
    "hist_file_cluster.txt":{"1.0 1.1":4},
    "hist_file_cluster_proc.txt":{"1.0":2},
    #There was a bug in the condor history exit once all possible jobs
    #are found where if the last passed job is to be found after the
    #other passed jobs it would be skipped. The below test makes sure that
    #regression doesnt occur
    "hist_file_jid_match_regression.txt":{"1.0 2.1":4},
    #All others check based on directory
    "base_epoch.txt":{"1.0 1.1 2.0 2.1":8},
    "cluster.txt":{"1.0 1.1":4},
    "cluster_proc.txt":{"1.0":2},
    "dlt_limit.txt":{"Error: -epoch:d (delete) cannot be used with -limit or -match.":1},
    "dlt_match.txt":{"Error: -epoch:d (delete) cannot be used with -limit or -match.":1},
    "dlt_scanlimit.txt":{"Error: -epoch:d (delete) cannot be used with -scanlimit.":1},
    "delete.txt":{"1.0 1.1 2.0 2.1":8}
}
#--------------------------------------------------------------------------------------------
def clean_up(dir):
    #make epochs dir if not exist
    if not os.path.exists(dir / epochDir):
        os.mkdir(dir / epochDir)
    #delete any remaining epoch files
    epoch_files = os.listdir(dir / epochDir)
    for epoch in epoch_files:
        os.remove(dir / epoch)

#--------------------------------------------------------------------------------------------
def error(msg,test_passing):
    #error print function for condor_history tests (if run has already failed don't newline at start)
    if test_passing:
        print("\n{}".format(msg))
    else:
        print("{}".format(msg))

#--------------------------------------------------------------------------------------------
#Start a personal condor with JOB_EPOCH_HISTORY_DIR set
@standup
def condor(test_dir):
    with Condor(local_dir=test_dir / "condor",config={"SCHEDD_INTERVAL":1,
                                                      "JOB_EPOCH_HISTORY_DIR":"{0}".format(test_dir / epochDir),
                                                      "JOB_EPOCH_HISTORY":"{0}".format(test_dir / epochDir / "epoch_history")}) as condor:
        yield condor

#--------------------------------------------------------------------------------------------
@action
def run_crondor_jobs(condor,test_dir,path_to_sleep):
    #Clean up for reruns
    clean_up(test_dir)
    global setUpSuccess
    jobsRan = True #Assume jobs will run
    #Submit crondor job 2 times
    for i in range(2):
        #If first job failed don't bother submitting second one
        if not jobsRan:
            break
        job = condor.submit(
            description={"executable": path_to_sleep,
                "arguments": "0",
                "universe": "vanilla",
                "on_exit_remove": "false",
                "periodic_remove": "NumShadowStarts >= 2",
                "log": "job.log",
            },count=2,)
        jobsRan = job.wait(condition=ClusterState.all_terminal,timeout=60)
    if not jobsRan:
        print("\nERROR: Cron jobs timed out.")
        setUpSuccess = False
    else:
        #Verify that 4 epoch files were made 1.0 1.1 2.0 2.1
        epoch_files = os.listdir(test_dir / epochDir)
        if len(epoch_files) != 5:
            print("\nERROR: Failed to write all epoch files.")
            setUpSuccess = False
    return setUpSuccess

#--------------------------------------------------------------------------------------------
@action(params={ #Params dictionary for testing condor_history -epoch
#"Test param":"Command for os"

#Cannot test with no path because condor_history is using the parent condor tools and doesn't
#have JOB_EPOCH_HISTORY_DIR set and we don't want it because other jobs will write tests
#there messing up this test. So, check condor_history -epoch for Error message
"(file) history":"condor_history -epoch",
"(file) history w/ cluster":"condor_history -epoch 1",
"(file) history w/ cluster.proc":"condor_history -epoch 1.0",
"(file) history job id mathing regression":"condor_history -epoch 2.1 1.0",
"(dir) history":"condor_history -dir -epoch",
"(dir) history w/ cluster":"condor_history -dir -epoch 1",
"(dir) history w/ cluster.proc":"condor_history -dir -epoch 1.0",
"(dir) history w/ delete & limit":"condor_history -dir -epoch:d -limit 1",
"(dir) history w/ delete & match":"condor_history -dir -epoch:d -match 1",
"(dir) history w/ delete & scanlimit":"condor_history -dir -epoch:d -scanlimit 1",
"(dir) history w/ delete":"condor_history -dir -epoch:d",
})
def read_epochs(condor,test_dir,path_to_sleep,request):
    global historyTestNum
    #Run the command and write its output to a file
    historyTestNum += 1
    #If the crondor jobs/set up failed just fail these tests
    if not setUpSuccess:
        #If it is the first condor_history test then output message
        #otherwise just return false to prevent spamming of error message
        if historyTestNum == 0:
            print("\nERROR: Set up failed. Cannot test history tools.")
        return False

    #Split based param command line for specific test into array
    cmd = request.param.split()
    cp = condor.run_command(cmd)
    out_file = Path(test_dir / outputFiles[historyTestNum])
    with out_file.open("a") as f:
        f.write(cp.stdout + "\n" + cp.stderr + "\n")
    test_passed = True #naively assume test will pass
    #Verify output in written file
    if "dlt" in outputFiles[historyTestNum]:
        #Is a delete option with another flag that should cause an error or no_path without the knob set
        fd = open(outputFiles[historyTestNum],"r")
        matches = 0
        key = ""
        for line in fd:
            line = line.strip()
            #Skip header line (should be first)
            if "OWNER" in line or line == "\n":
                pass
            #Check that line is in expected output
            elif line in expectedOutput[outputFiles[historyTestNum]]:
                matches += 1
                key = line
            #There should only be 2 lines for files: Header and error so if anything else is found error
            else:
                error("ERROR: Unexpected line ({0}) in output file ({1})".format(line,outputFiles[historyTestNum]),test_passed)
                test_passed = False
                break
        #Verify that only one matching error line was found
        if key in expectedOutput[outputFiles[historyTestNum]]:
            if matches != expectedOutput[outputFiles[historyTestNum]][key]:
                error("ERROR: stderr message ({0}) occurred more than 1 time: ({1})".format(key,matches),test_passed)
                test_passed = False
        else:
            error("ERROR: Wrong error message found ({0})".format(key),test_passed)
            test_passed = False
        fd.close()
    else:
        fd = open(outputFiles[historyTestNum],"r")
        total_lines = 0
        jobIds = {}
        #Digest lines in successfull reading
        for line in fd:
            #Skip header line (should be first)
            if "OWNER" in line or line == "\n":
                pass
            else:
                #Get Job Id (cluster.proc) and increment dictionary count or add to dictionary
                job = line.split(" ",1)[0]
                total_lines += 1
                if job in jobIds:
                    jobIds[job] += 1
                else:
                    jobIds[job] = 1
        fd.close()
        count = 0
        #Each sub-dictionary contents of expected output (should only be 1) verify correct condor_history output
        for ids,total in expectedOutput[outputFiles[historyTestNum]].items():
            count += 1
            #More then 1 sub-dictionary items break out of loop and fail test
            if count > 1:
                error("WARNING (Internal ERROR): Expected output sub-dictionary holds more then 1 key value pair. Did not expect {{0}:{1}}".format(ids,total),test_passed)
                testPassed = False
                break
            matches = 0
            job_count = 0
            #Go through digest job ids dictionary
            for job,i in jobIds.items():
                matches += i
                #Make sure job id is expected
                if job not in ids:
                    error("ERROR: Unexpected Job Id ({0}). Expected IDs = {1}".format(job,ids),test_passed)
                    test_passed = False
                else:
                    job_count += 1
            #Check that total num of job ads match with the 2 counts
            if total != matches or total != total_lines:
                error("ERROR: Total matches don't equal. Expected={0}, Line Count={1}, Id Count={2}".format(total,total_lines,matches),test_passed)
                test_passed = False
            #Check that the number of jobs is accurate 
            if job_count != len(ids.split(" ")):
                error("ERROR: Found more job ids than expected.",test_passed)
                test_passed = False
    #If delete.txt then verify that the epoch dir is empty regardless of if we have failed already
    if "delete.txt" == outputFiles[historyTestNum]:
        epoch_files = os.listdir(test_dir / epochDir)
        if len(epoch_files) != 1 and epoch_file[0] != "epoch_history":
            error("ERROR: Delete option failed to remove all epoch files.",test_passed)
            test_passed = False
    #Return if the test has passed or not
    return test_passed

#--------------------------------------------------------------------------------------------
#Test getting epoch ads from the python bindings
@action
def runPyBindings(condor):
    proj = ["ClusterId", "ProcId"]
    hist_itr = None
    with condor.use_config():
        hist_itr = htcondor.Schedd().jobEpochHistory("ClusterId==1", proj)
    return hist_itr

#============================================================================================
#This test must run in order of test_run_jobs then test_condor_history_reads_epochs
class TestHistoryReadsEpochs:
    #Make Epoch files by running cron jobs
    #We expect the jobs to finish successfully
    def test_run_jobs(self,run_crondor_jobs):
        assert run_crondor_jobs is True

    #Test the various condor_history options that should pass 
    def test_condor_history_reads_epochs(self,read_epochs):
        assert read_epochs is True

    #Test the python bindings ability to query epoch history
    def test_python_bindings(self, runPyBindings):
        epoch_counts = {
            "total" : 0,
            "proc-0" : 0,
            "proc-1" : 0,
        }
        for ad in runPyBindings:
            assert ad.get("ClusterId", -1) == 1
            proc_id = ad.get("ProcId", -1)
            assert proc_id == 0 or proc_id == 1
            epoch_counts["total"] += 1
            epoch_counts[f"proc-{proc_id}"] += 1
        assert epoch_counts["total"] == 4
        assert epoch_counts["proc-0"] == 2
        assert epoch_counts["proc-1"] == 2

