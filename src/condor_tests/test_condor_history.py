#!/usr/bin/env pytest

#   ====test_condor_history====
#   This is a regression test for most of condor_history tools abilities.
#   This test uses a dry-run submit job to create various fake job ads written
#   to `HISTORY`, `STARTD_HISTORY`, a custom single file, and a set of custom
#   condor time rotated files.
#
#   Notes:
#       - This test will check for all expected header display words/attrs
#       - This test will check for an exact matching number of jobs displayed
#       - This test DOES NOT check for correct job order output (could be added)
#       - This test DOES NOT check epoch stuff or the following flag options:
#               xml,json,jsonl,long,attributes,format,pool,or autoformat
#       - This test will also test some various expected failures

#Configuration for personal condor. We add names for STARTD & SCHEDD to trick history for remote queries
#Note: I put set the config dir to the execute dir because we don't run any actual jobs still might be
#      nice to have a better config dir location.
#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
    # Configuration to assist testing condor_history tool
    SCHEDD_NAME = TEST_SCHEDD@
    STARTD_NAME = TEST_STARTD@
    LOCAL_CONFIG_DIR = $(EXECUTE)
"""
#endtestreq

from ornithology import *
from time import time
import os
import sys
import htcondor

#Custom class to help build job ads for history files
class HistAdsViaCluster():
    def __init__(self, ownr="mironlivny", clust=-1, procs=1, qdate=-1, cust_attrs={}):
        self.owner = ownr
        self.clusterId = clust
        #Procs = number of job procs for this cluster
        self.numProcs = procs
        self.QDate = qdate
        #cust_attrs = dictionary of key value pairs to add/change attrs for this cluster
        self.attrs = cust_attrs

#Custom class to set up specific test runs parameters for success
class TestReqs:
    #Remove pytest collection warning
    __test__ = False
    def __init__(self, cmd, exp_out=[], header=[]):
        self.cmd = cmd
        #List of exact job ids expected to be outputed
        self.expectedOutput = exp_out
        #List of exact words/attrs in header to expect in output
        self.header = header
#===============================================================================================
#Holds base dry run output to reset any changed ad
#attrs between clusters when making history files
ORIGINAL_AD = {}
#Directory to write tool output files to
OUTPUT_DIR = "cmd_output"
#Standar history header format
STD_HEADER = ["ID","OWNER","SUBMITTED","RUN_TIME","ST","COMPLETED","CMD"];
#Adding test cases:
#The key will become the name of the history tools output file i.e. OUTPUT_DIR/{key}.out
#Make TestReqs object
#Any test command that uses the flags -file, -userlog, or -search should put those last
#because the test structure checks for those flags and adds the appropriate file to the end of the command
TEST_CASES = {
    "base_history"      : TestReqs("condor_history", ["1.0","2.0","2.1","2.2","3.0","4.0","4.1","5.0","5.1","5.2","5.3","5.4"], STD_HEADER),
    "startd_history"    : TestReqs("condor_history -startd", ["1.0","2.0","2.1","2.2","3.0","4.0","4.1","5.0","5.1","5.2","5.3","5.4"], STD_HEADER),
    "search_cluster"    : TestReqs("condor_history 2", ["2.0","2.1","2.2"], STD_HEADER),
    "search_clust.proc" : TestReqs("condor_history 5.1", ["5.1"], STD_HEADER),
    "search_owner"      : TestReqs("condor_history unclegreg",["4.0","4.1"], STD_HEADER),
    "history_scanlimit" : TestReqs("condor_history 4 3 2 -scanlimit 3", ["4.0","4.1","3.0"], STD_HEADER),
    "history_match"     : TestReqs("condor_history 5 -match 3", ["5.4","5.3","5.2"], STD_HEADER),
    "use_file_flag"     : TestReqs("condor_history -file", ["100.0","100.1","100.2","100.3","100.4","100.5","100.6","100.7","100.8","100.9",
                                   "101.0","101.1","101.2","103.0","104.0","104.1","105.0","105.1","105.2","105.3","105.4"], STD_HEADER),
    "use_userlog_flag"  : TestReqs("condor_history -userlog", ["984.0"], STD_HEADER),
    "use_search_flag"   : TestReqs("condor_history -search", ["50.0","51.0","52.0","53.0"], STD_HEADER),
    "use_constraint"    : TestReqs("condor_history -const Beers>=3 -file", ["101.0","101.1","101.2","105.0","105.1","105.2","105.3","105.4"], STD_HEADER),
    "constraint_undef"  : TestReqs("condor_history -constraint Beers=?=UNDEFINED -file", ["100.0","100.1","100.2","100.3","100.4","100.5","100.6","100.7","100.8","100.9"], STD_HEADER),
    "search_forwards"   : TestReqs("condor_history -forwards -limit 2", ["1.0","2.0"], STD_HEADER),
    "since_jid"         : TestReqs("condor_history -since 4.0", ["4.1","5.0","5.1","5.2","5.3","5.4"], STD_HEADER),
    "competed_since"    : TestReqs("condor_history -completedsince 125 -file", ["104.0","104.1","105.0","105.1","105.2","105.3","105.4"], STD_HEADER),
    "af_headers"        : TestReqs("condor_history -af:jh Owner QDate DAGManJobId CMD -limit 1", ["5.4"], ["ID","Owner","QDate","DAGManJobId","CMD"]),
    "no_match_af_const" : TestReqs("condor_history -af:j Beers -const Beers>10 -file"),
    # Remote history checks
    "remote_schedd"     : TestReqs("condor_history -name TEST_SCHEDD@", ["1.0","2.0","2.1","2.2","3.0","4.0","4.1","5.0","5.1","5.2","5.3","5.4"], STD_HEADER),
    "remote_startd"     : TestReqs("condor_history -name TEST_STARTD@ -startd", ["1.0","2.0","2.1","2.2","3.0","4.0","4.1","5.0","5.1","5.2","5.3","5.4"], STD_HEADER),
    "remote_cluster"    : TestReqs("condor_history 5 -name TEST_SCHEDD@", ["5.0","5.1","5.2","5.3","5.4"], STD_HEADER),
    "remote_clust.proc" : TestReqs("condor_history 5.3 -name TEST_SCHEDD@", ["5.3"], STD_HEADER),
    "remote_match"      : TestReqs("condor_history 5 -name TEST_SCHEDD@ -limit 2", ["5.4","5.3"], STD_HEADER),
    "remote_since"      : TestReqs("condor_history -name TEST_SCHEDD@ -since 4.0", ["4.1","5.0","5.1","5.2","5.3","5.4"], STD_HEADER),
    "remote_forwards"   : TestReqs("condor_history 2 -name TEST_SCHEDD@", ["2.0","2.1","2.2"], STD_HEADER),
    "remote_scanlimit"  : TestReqs("condor_history -scanlimit 2 -name TEST_SCHEDD@", ["5.3","5.4"], STD_HEADER),
    # Special case requires reconfig
    # Test remote query works when history=  and schedd.history=file
    "remote_schedd.hist": TestReqs("condor_history 4 -name TEST_SCHEDD@", ["4.0","4.1"], STD_HEADER),
}
#-----------------------------------------------------------------------------------------------
# Test cases where we expect to get failures
# add case: test name : { cmd:command to run, err: expected error message, config: None does nothing otherwise write dictionary options}
TEST_ERROR_CASES = {
    "fail_remote_no_history" : { "cmd":"condor_history -af ClusterId -name TEST_SCHEDD@",
                                 "err":"undefined in remote configuration. No such related history to be queried.",
                                 "config":{"SCHEDD.HISTORY":"","HISTORY":""}},
    "fail_multi_source"      : { "cmd":"condor_history -startd -epoch",
                                 "err":"can not be used in association with",
                                 "config":{}},
}
#===============================================================================================
# Reconfig condor to how we want
def reconfig(condor, test_name, test_dir, config={}):
    reconfig_cmd = ["condor_reconfig","-schedd"]
    p = condor.run_command(["condor_config_val","LOCAL_CONFIG_DIR"])
    #Move any old config dir files to the test dir for archiving of test config
    for tmp_file in os.listdir(p.stdout):
        old_path = os.path.join(p.stdout,tmp_file)
        new_path = os.path.join(str(test_dir),tmp_file)
        os.rename(old_path,new_path)
    #If no config is needed then run reconfig and return
    if len(config) == 0:
        p = condor.run_command(reconfig_cmd)
        return
    #Else make a config file = test_name.conf and write key=value config information
    filename =  os.path.join(p.stdout,f"{test_name}.conf")
    with open(filename, "w") as f:
        for knob,value in config.items():
            f.write(f"{knob}={value}\n")
    #Reconfig
    p = condor.run_command(reconfig_cmd)
#-----------------------------------------------------------------------------------------------
#Write cluster(s) job history to specified file
def writeAdToHistory(filename, clusters, ad, is_schedd=True):
    #If the file exists already then remove it
    if os.path.exists(filename):
        os.remove(filename)
    used_clusters = []
    changed_attrs = []
    offset = 0
    fd = open(filename, "a")
    ad["HistoryType"] = '"SCHEDD"' if is_schedd else '"STARTD"'
    for clust in clusters:
        #Make sure the desired clusters id has not been used yet (per file for now)
        while clust.clusterId in used_clusters:
            clust.clusterId += 1
        #Add new clusterid ot used clusters
        used_clusters.append(clust.clusterId)
        #Reset/Remove custom attrs from last clusters job ads
        for attr in changed_attrs:
            if attr in ORIGINAL_AD:
                ad[attr] = ORIGINAL_AD[attr]
            elif attr in ad:
                del ad[attr]
            changed_attrs.clear()
        #Add/change any custom attrs for this clusters job ads
        if len(clust.attrs) > 0:
            for key,value in clust.attrs.items():
                ad[key] = value
                changed_attrs.append(key)
        #For each desired proc in cluster
        for i in range(clust.numProcs):
            #Update clusterid, owner, and proc id
            ad["ClusterId"] = clust.clusterId
            ad["Owner"] = f"\"{clust.owner}\""
            ad["ProcId"] = i
            #Update QDate if specified
            if clust.QDate != -1:
                ad["QDate"] = clust.QDate
            else:
                clust.QDate = ad["QDate"]
            #Set completion date to 60 seconds after qdate
            compDate = clust.QDate + 60
            ad["CompletionDate"] = compDate
            ad_str = ""
            #sort attrs to mimic regular history ad (dry run is a mess)
            myKeys = list(ad.keys())
            myKeys.sort()
            sorted_attrs = {i: ad[i] for i in myKeys}
            #Construct string for full job ad to write to file
            for key,value in sorted_attrs.items():
                ad_str += f"{key}={value}\n"
            #Add banner at end
            ad_str += f"*** Offset = {offset} ClusterId = {clust.clusterId} ProcId = {i} Owner = \"{clust.owner}\" CompletionDate = {compDate}\n"
            fd.write(ad_str)
            offset += sys.getsizeof(ad_str)
    fd.close()

#-----------------------------------------------------------------------------------------------
#Get a job ad via condor_submit dry run
@action
def jobAd(default_condor, test_dir, path_to_sleep):
    global ORIGINAL_AD
    ad = {}
    #Create super generic sleep job submit file to use for dry run
    filename = os.path.join(str(test_dir),"sleep.sub")
    if not os.path.exists(filename):
        fd = open(filename, "w")
        fd.write("""
executable = {0}
arguments  = 1
log        = sleep.log
queue
""".format(path_to_sleep))
        fd.close()
    #Make file to hold dry run output
    out_file = os.path.join(str(test_dir),"dryRun.ad")
    #Run condor_submit with dry run option for sleep job
    default_condor.run_command(["condor_submit","-dry-run",out_file,filename])
    #Open the dry run output and parse key=value pairs into a python dictionary
    fd = open(out_file,"r")
    for line in fd:
        line = line.strip()
        if "=" in line:
            parts = line.split("=",1)
            ad.update({parts[0]:parts[1]})
    fd.close()
    #Set global original to use for reseting/removing custom attrs
    ORIGINAL_AD.update(ad)
    return ad

#-----------------------------------------------------------------------------------------------
#Write job ads to `HISTORY` and `STARTD_HISTORY`
#History in order Top (oldest) -> Bottom(newest):
#Example t=1000
#JID Ownr      QDat Comp Cust_Attr
#1.0 cole      1000 1060  -
#2.0 tj        1010 1070  -
#2.1 tj        1010 1070  -
#2.2 tj        1010 1070  -
#3.0 tj        1015 1075  -
#4.0 unclegreg 1020 1080  -
#4.1 unclegreg 1020 1080  -
#5.0 cole      1025 1085  -
#5.1 cole      1025 1085  -
#5.2 cole      1025 1085  -
#5.3 cole      1025 1085  -
#5.4 cole      1025 1085  -
@action
def writeHistoryFile(default_condor, test_dir, jobAd):
    #Get current time for QDate
    t = int(time())
    #Defined clusters to exist in these files
    clusters = [
        HistAdsViaCluster("cole",1,1,t),
        HistAdsViaCluster("tj",2,3,t+10),
        HistAdsViaCluster("tj",3,1,t+15),
        HistAdsViaCluster("unclegreg",4,2,t+20),
        HistAdsViaCluster("cole",5,5,t+25),
    ]
    std_hist = ["HISTORY","STARTD_HISTORY"]
    #Write clusters info to history files
    for hist in std_hist:
        p = default_condor.run_command(["condor_config_val",hist])
        writeAdToHistory(p.stdout, clusters, jobAd, hist=="HISTORY")
    return ""

#-----------------------------------------------------------------------------------------------
#Global list of files to append to command to search specific files as opposed to knob locations
#Keep in line with later REQUEST_FILES dictionary and tuple returned by setUpFiles()
SPECIAL_FILES = ["single_hist", "sleep.log", "search_hist"]
#Write a special singular file with historical job ads for -file option
#History in order Top (oldest) -> Bottom(newest):
#JID   Ownr      QDat Comp Cust_Attr
#100.0 cole      0    60   -
#100.1 cole      0    60   -
#100.2 cole      0    60   -
#100.3 cole      0    60   -
#100.4 cole      0    60   -
#100.5 cole      0    60   -
#100.6 cole      0    60   -
#100.7 cole      0    60   -
#100.8 cole      0    60   -
#100.9 cole      0    60   -
#101.0 toddt     30   90   Beers=5
#101.1 toddt     30   90   Beers=5
#101.2 toddt     30   90   Beers=5
#103.0 miron     60   120  Beers=0
#104.0 unclegreg 90   150  Beers=2
#104.1 unclegreg 90   150  Beers=2
#105.0 cole      120  180  Beers=7
#105.1 cole      120  180  Beers=7
#105.2 cole      120  180  Beers=7
#105.3 cole      120  180  Beers=7
#105.4 cole      120  180  Beers=7
@action
def writeSingleFile(test_dir, jobAd):
    #Set t=0 to make a consistent time for checking -completedsince
    t = 0
    filename = os.path.join(str(test_dir), SPECIAL_FILES[0])
    #Defined clusters to write to this file
    clusters = [
        HistAdsViaCluster("cole",100,10,t),
        HistAdsViaCluster("toddt",101,3,t+30,{"Beers":5}),
        HistAdsViaCluster("miron",103,1,t+60,{"Beers":0}),
        HistAdsViaCluster("unclegreg",104,2,t+90,{"Beers":2}),
        HistAdsViaCluster("cole",105,5,t+120,{"Beers":7}),
    ]
    writeAdToHistory(filename, clusters, jobAd)
    return filename

#-----------------------------------------------------------------------------------------------
#Write a userlog (copy and pasted from a test one) for -userlog option
#Yield info for JID=984
@action
def writeULog(test_dir):
    ulog_name = os.path.join(str(test_dir), SPECIAL_FILES[1])
    fd = open(ulog_name, "w")
    fd.write("""000 (984.000.000) 2023-01-24 09:42:38 Job submitted from host: <127.0.0.1:51045?addrs=127.0.0.1-51045&alias=test.example&noUDP&sock=schedd_52651_5b06>
...
001 (984.000.000) 2023-01-24 09:42:45 Job executing on host: <127.0.0.1:51045?addrs=127.0.0.1-51045&alias=test.example&noUDP&sock=startd_52651_5b06>
...
006 (984.000.000) 2023-01-24 09:42:53 Image size of job updated: 150
    2  -  MemoryUsage of job (MB)
    2000  -  ResidentSetSize of job (KB)
...
005 (984.000.000) 2023-01-24 09:43:00 Job terminated.
    (1) Normal termination (return value 0)
        Usr 0 00:00:00, Sys 0 00:00:00  -  Run Remote Usage
        Usr 0 00:00:00, Sys 0 00:00:00  -  Run Local Usage
        Usr 0 00:00:00, Sys 0 00:00:00  -  Total Remote Usage
        Usr 0 00:00:00, Sys 0 00:00:00  -  Total Local Usage
    0  -  Run Bytes Sent By Job
    0  -  Run Bytes Received By Job
    0  -  Total Bytes Sent By Job
    0  -  Total Bytes Received By Job
    Partitionable Resources :    Usage  Request Allocated
        Cpus                 :                 1         1
        Disk (KB)            :      150      150  45831539
        Memory (MB)          :        2        1      4096

    Job terminated of its own accord at 2023-01-24T15:43:00Z with exit-code 0.
...""")
    fd.close()
    return ulog_name

#-----------------------------------------------------------------------------------------------
#Write 4 files with condor time rotation stamps (1 job ad each) for -search option
#History in order Top (oldest) -> Bottom(newest):
#JID   Ownr      QDat Comp Cust_Attr FileName
#50.0  cole      ???  ???  -         `base search filename`.19980514T000001
#51.0  cole      ???  ???  -         `base search filename`.19980514T153906
#52.0  cole      ???  ???  -         `base search filename`.20220214T000001
#53.0  cole      ???  ???  -         `base search filename`
@action
def writeSearchFiles(test_dir, jobAd):
    rotation = [".19980514T000001",".19980514T153906",".20220214T000001",""]
    filename = os.path.join(str(test_dir), SPECIAL_FILES[2])
    clusters = [ HistAdsViaCluster("cole",50,1) ]
    for rota in rotation:
        tmp = filename + rota
        writeAdToHistory(tmp, clusters, jobAd)
        clusters[0].clusterId += 1
        clusters[0].QDate += 10
    return filename

#-----------------------------------------------------------------------------------------------
#Dictionary to map flags to specific file to search
REQUEST_FILES = {"-file":0,"-userlog":1,"-search":2}
#Set up action to write all the ads to appropriate files and return special files
@action
def setUpFiles(writeHistoryFile, writeSearchFiles, writeSingleFile, writeULog):
    return (writeSingleFile, writeULog, writeSearchFiles)
#Actualy run commands/test cases (set up to all run concurrently)
@action
def runHistoryCmds(default_condor, test_dir, setUpFiles):
    out_dir = os.path.join(str(test_dir),OUTPUT_DIR)
    #If output dir exists already just empty it otherwise mkdir
    if os.path.exists(out_dir):
        for tmp_file in os.listdir(out_dir):
            file_path = os.path.join(out_dir,tmp_file)
            os.remove(file_path)
    else:
        os.mkdir(out_dir)
    #Dictionary to hold test information for analysis
    ran_cmds ={}
    #Run each test
    for key in TEST_CASES.keys():
        if key == "remote_schedd.hist":
            p = default_condor.run_command(["condor_config_val","HISTORY"])
            reconfig(default_condor, key, test_dir, {"SCHEDD.HISTORY":p.stdout,"HISTORY":""})
        #Split command into list for .run_command()
        cmd = TEST_CASES[key].cmd.split()
        #For each special flag that needs a file appended
        for k in REQUEST_FILES.keys():
            #Search for that flag in the test command
            if k in cmd:
                #If flag found then append the file to the command
                cmd.append(setUpFiles[REQUEST_FILES[k]])
                #Update test case command string to complete command for output purposes
                TEST_CASES[key].cmd += f" test_dir/{SPECIAL_FILES[REQUEST_FILES[k]]}"
        #Run the command
        p = default_condor.run_command(cmd)
        #Make file in output dir with as `test name`.out
        filename = f"{key}.out"
        out_file = os.path.join(out_dir,filename)
        #Write command processes stdout and stderr to test output file
        with open(out_file,"a") as f:
            f.write(p.stdout + p.stderr)
        ran_cmds[key] = (TEST_CASES[key],out_file)
    yield ran_cmds
#-----------------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def runTests(request, runHistoryCmds):
    return (request.param, runHistoryCmds[request.param][0], runHistoryCmds[request.param][1])
@action
def testName(runTests):
    return runTests[0]
@action
def testInfo(runTests):
    return runTests[1]
@action
def testOutputFile(runTests):
    return runTests[2]
#===============================================================================================
@action
def runPyBindings(default_condor):
    query = {}
    proj = ["ClusterId", "ProcId", "HistoryType"]
    const = "ClusterId==4"
    with default_condor.use_config():
        #cluster id == 4
        query["Startd"] = htcondor.Startd().history(const, proj)
        query["Schedd"] = htcondor.Schedd().history(const, proj)
    return query
#===============================================================================================
@action
def runErrorCmds(default_condor, test_dir):
    #Creat path to output directory
    out_dir = os.path.join(str(test_dir),OUTPUT_DIR)
    ran_cmds = {}
    #For each error test reconfig if needed then run command
    for key in TEST_ERROR_CASES.keys():
        config = TEST_ERROR_CASES[key]["config"]
        if config != None:
            reconfig(default_condor, key, test_dir, config)
        cmd = TEST_ERROR_CASES[key]["cmd"].split()
        p = default_condor.run_command(cmd)
        #Make file in output dir with as `test name`.out
        filename = f"{key}.out"
        out_file = os.path.join(out_dir,filename)
        #Write command processes stdout and stderr to test output file
        with open(out_file,"a") as f:
            f.write(p.stdout + p.stderr)
        ran_cmds[key] = (TEST_ERROR_CASES[key]["err"], out_file)
    yield ran_cmds
#-----------------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_ERROR_CASES})
def runFailed(request, runErrorCmds):
    return (request.param,runErrorCmds[request.param][0], runErrorCmds[request.param][1])
#===============================================================================================
class TestCondorHistory:

    def test_condor_history(self, testName, testInfo, testOutputFile):
        passed = True
        #We always expect a file to be created. If file DNE fail
        if not os.path.exists(testOutputFile):
            print(f"\nERROR: {testInfo.cmd} failed to produce out test_dir/{OUTPUT_DIR}/{testName}.out.")
            passed = False
        #If file is empty make sure we aren't expecting any output
        elif os.path.getsize(testOutputFile) == 0:
            if len(testInfo.expectedOutput) != 0 or len(testInfo.header) != 0:
                print(f"\nERROR: {testName} is expecting output but test_dir/{OUTPUT_DIR}/{testName}.out is empty.")
                passed = False
        #Otherwise it is time to analyze the output file
        else:
            with open(testOutputFile,"r") as f:
                #Are we expecting a header?
                if len(testInfo.header) != 0:
                    first_line = f.readline()
                    first_line = first_line.strip()
                    #We do not expect any errors set to fail test
                    if "Error" in first_line:
                        print(f"\nERROR: Unexpected error message found ({first_line})")
                        passed = False
                    else:
                        #Verify that all expected header names/attrs exist in the line
                        missing = ""
                        for name in testInfo.header:
                            if name not in first_line:
                                missing += f"{name} "
                        #Not all header attrs found write error and set test to faile
                        if len(missing) != 0:
                            missing = missing.strip()
                            print(f"\nERROR: test_dir/{OUTPUT_DIR}/{testName}.out is missing header display names [{missing}].")
                            passed = False
                #Assert that we haven't already failed due to the header check
                assert passed == True
                found = []
                unexpected = []
                for line in f:
                    line = line.strip()
                    #We do not expect any errors so fail test
                    if "Error" in line or "malformed" in line:
                        print(f"\nERROR: Unexpected error message found ({line})")
                        assert False
                    #Skip blank/empty lines
                    elif line == "\n":
                        pass
                    #Grab 1st item (should be jid) of space seperated job information
                    jid = line.split(" ")[0]
                    #Search for jid in expected output and add to appropriate list of info
                    if jid in testInfo.expectedOutput:
                        found.append(jid)
                    else:
                        unexpected.append(jid)
                #If we found an unexpected jid then error out
                if len(unexpected) != 0:
                    extra = " ".join(unexpected)
                    print(f"\nERROR: {testName} found unexpected job ids ({extra}) in '{testInfo.cmd}' output.")
                    passed = False
                #Else make sure all we found all of the expected jid's
                elif len(found) != len(testInfo.expectedOutput):
                    missing = ""
                    for jid in testInfo.expectedOutput:
                        if jid not in found:
                            missing += f"{jid} "
                    missing = missing.strip()
                    print(f"\nERROR: {testName} output from '{testInfo.cmd}' is missing job ids ({missing}).")
                    passed = False
        assert passed == True

    def test_python_bindings(self, runPyBindings):
        for test,ads in runPyBindings.items():
            proc_count = 0
            type_match = "SCHEDD" if test.startswith("Schedd") else "STARTD"
            for ad in ads:
                assert ad.get("HistoryType", "UNKNOWN") == type_match
                assert ad.get("ClusterId", -1) == 4
                assert ad.get("ProcId") is not None
                proc_count += 1
            assert proc_count == 2

    def test_history_errors(self, runFailed):
        error_found = False
        #Open output file and check for given error message
        with open(runFailed[2], "r") as f:
            for line in f:
                line = line.strip()
                if runFailed[1] in line:
                    error_found = True
            if not error_found:
                print(f"\nERROR: {runFailed[0]} failed to produce error message '{runFailed[1]}'")
        assert error_found == True



