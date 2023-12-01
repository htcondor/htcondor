#!/usr/bin/env pytest

#  =======test_dagman_proper_env=======
#   This test will check that the DAGMan proper manager
#   job has its envrionment setting controls working correctly.
#   With this we test -insert_env, -include_env, the config
#   knob DAGMAN_MANAGER_JOB_APPEND_GETENV, and the DAG command: ENV
#           -Cole Bollig 2023-02-21

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# Set DAGMan append getenv to test vars
	DAGMAN_MANAGER_JOB_APPEND_GETENV = Beer,Donuts
"""
#endtestreq

from ornithology import *
import htcondor2 as htcondor
import os
from shutil import copy
from time import time

#-------------------------------------------------------------------------------------------------------
#Global variable for expected variables to be found in getenv in the submit file
GETENV_VALUES = {
    #Base always added values
    "BASE":["CONDOR_CONFIG","_CONDOR_*","PATH","PYTHONPATH","PERL*","PEGASUS_*","TZ","HOME","USER","LANG","LC_ALL","BEARER_TOKEN","BEARER_TOKEN_FILE","XDG_RUNTIME_DIR"],
    #getenv added values from the above configuration
    "CONFIG":["Beer","Donuts"],
    #When config knob is set to True then getenve should be set to just true
    "TRUE":["true"],
}
#Enviroment variables always expected to exist in the DAGMan jobs enviroment
BASE_ENV_VALUES = ["_CONDOR_DAGMAN_LOG","_CONDOR_MAX_DAGMAN_LOG","_CONDOR_SCHEDD_DAEMON_AD_FILE","_CONDOR_SCHEDD_ADDRESS_FILE"]
#Environment variables added when a custom dagman config is added
CONF_ENV_VALUES = ["_CONDOR_DAGMAN_CONFIG_FILE"]

#-------------------------------------------------------------------------------------------------------
#Class to hold information regarding how to write dag files
class DAGManTestInfo:
    def __init__(self,filename,description="",sub=None,conf={}):
        #Dag name: test.dag
        self.name = filename
        #Dag description: JOB A {} -> this will be formated later with a simple sleep submit job
        self.desc = description
        #Another class of Dag information to write a subdag
        self.subdag = sub
        #Dictionary of key=value config options for custom configuration
        self.config = conf

#-------------------------------------------------------------------------------------------------------
#All Test Cases:
#   Test Name : {"dag":DAGManTestInfo,
#                "include":list of variable names to be appended to getenv and later checked for
#                "insert":list of key=value pairs to be inserted into the environment
#                "Delim":A character to change the delimiter used in -insert_env flag
#                "check_getenv":Key string for expected getenv variables that are expected and not explicitly added with 'include'
#               }
#Note: All but the subdag test will be ran as -no_submit because we just need the produced
#      .condor.sub file to read and make sure things were set correctly
TEST_CASES = {
    #Test for just a normal dag submit no flags and the config knob set
    "base_dag"      : {"dag":DAGManTestInfo("simple.dag"),
                       "check_getenv":"ALL"},
    #Test dag submit with -include_env flag
    "include_flag"  : {"dag":DAGManTestInfo("simple.dag"),
                       "include":["Matrix","ZKM","CANADA"],
                       "check_getenv":"ALL"},
    #Test dag submit with -insert_env flag
    "insert_flag"   : {"dag":DAGManTestInfo("simple.dag"),
                       "insert":["Logical=False","WORK_HOURS=8","DOOR=Open","DnD=Awesome"],
                       "check_getenv":"ALL"},
    #Test dag submit with -insert_env flag with passed key=value pairs delimited with different character
    "cust_delim"    : {"dag":DAGManTestInfo("simple.dag"),
                       "insert":["VIDEO_GAMES=Borderlands;COD;DOOM;Minecraft;etc;","Memes=Dank"],
                       "Delim":"|",
                       "check_getenv":"ALL"},
    #Test dag submit with both -include_env flag and -insert_env flag (This .condor.sub file & info will be used again for python bindings test)
    "both_flags"    : {"dag":DAGManTestInfo("simple.dag","JOB TEST {0}"),
                       "include":["Purple","MUSIC","WW2","TalkingHeads"],
                       "insert":["_CONDOR_DEBUG=D_ALWAYS","Covid=19","Sanity=None"],
                       "check_getenv":"ALL"},
    #Test dag submit with subdag externals and both -include_env flag and -insert_env flag
    "sub_dags"      : {"dag":DAGManTestInfo("main.dag","",DAGManTestInfo("mid.dag","JOB A {0}",DAGManTestInfo("deep.dag","JOB B {0}"))),
                       "include":["Money","MAGIC","SheepsHead"],
                       "insert":["Pokemon=Classic","Elvis=Iconic","RANDOM_INT=102847392","Quotient=27a-B"],
                       "check_getenv":"ALL"},
    #Test dag submit overriding the config knob being set for a pure dag
    "config_none"   : {"dag":DAGManTestInfo("simple.dag","",None,{"DAGMAN_MANAGER_JOB_APPEND_GETENV":""}),
                       "check_getenv":"BASE"},
    #Test dag submit with config knob = true to result in 'getenv = true' in the .condor.sub file
    "config_true"   : {"dag":DAGManTestInfo("simple.dag","",None,{"DAGMAN_MANAGER_JOB_APPEND_GETENV":"True"}),
                       "include":["SHOULD_DISAPEAR"],
                       "insert":["Water=Wet","Sleep=False"],
                       "check_getenv":"TRUE"},
}

#-------------------------------------------------------------------------------------------------------
#Helper function to write out a custom config file for a dag to use
def write_dag_config(dir_name,knobs):
    path = os.path.join(str(dir_name),"dag.conf")
    f = open(path,"w")
    for key,value in knobs.items():
        f.write(f"{key}={value}\n")
    f.close()
    return path

#Helper function to write a dag file to run condor_submit_dag on
SUB_DAG_NUM = 0
def write_dags(sleep,dag_info,is_subdag=False):
    global SUB_DAG_NUM
    ret = []
    #If we have a subdag make a sub-dag dir and change to it
    if is_subdag:
        curr_path = str(os.getcwd())
        sub_dir = os.path.join(curr_path,"sub-dag")
        if not os.path.exists(sub_dir):
            os.mkdir(sub_dir)
        os.chdir(sub_dir)
    curr_path = str(os.getcwd())
    #path to expected .condor.sub file for this dag
    dag_sub_path = os.path.join(curr_path,dag_info.name+".condor.sub")
    ret.append(dag_sub_path)
    #Write dag file
    f = open(dag_info.name,"w")
    if len(dag_info.config) > 0:
        #If we have specified configuration then write file and add path to dag file
        conf_path = write_dag_config(curr_path,dag_info.config)
        f.write(f"CONFIG {conf_path}\n")
    if len(dag_info.desc) > 0:
        #If we have a dag description then format all {} with path to sleep job and write to dag file
        tmp = dag_info.desc.format(str(sleep))
        f.write(f"{tmp}\n")
    if dag_info.subdag is not None:
        #Recursively write subdags and add the path for this subdag to the dag file
        subdags = write_dags(sleep,dag_info.subdag,True)
        f.write(f"SUBDAG EXTERNAL INTERNAL_DAG-{SUB_DAG_NUM} {subdags[0][:-11]}")
        SUB_DAG_NUM += 1
        ret.extend(subdags)
    f.close()
    return ret

#-------------------------------------------------------------------------------------------------------
#Fixture to submit all the tests
@action
def submit_dags(default_condor,test_dir,path_to_sleep):
    #Write very basic sleep job submit file
    sub_path = os.path.join(str(test_dir),"sleep.sub")
    f = open(sub_path,"w")
    f.write("""#Simple Sleep Job Submit
executable = {}
arguments  = 0
log        = job.log
queue
""".format(path_to_sleep))
    f.close()
    test_output = {}
    getenv_chk = []
    env_chk = []
    #For each test case
    for key in TEST_CASES.keys():
        #Start creating command line args
        cmd = ["condor_submit_dag","-f"]
        #Change back to main test dir
        os.chdir(str(test_dir))
        #Make test case sub dir and change to it
        dir_path = os.path.join(str(test_dir),key)
        if not os.path.exists(dir_path):
            os.mkdir(dir_path)
        os.chdir(dir_path)
        #Write test case dag files
        dag_files = write_dags(sub_path,TEST_CASES[key]["dag"])
        #If subdags exist change back to test case dir
        if len(dag_files) > 1:
            os.chdir(dir_path)
        #Else add -no_submit to command line args
        else:
            cmd.append("-no_submit")
        #Construct -include_env flag/args and add to command line
        if "include" in TEST_CASES[key]:
            getenv_chk = TEST_CASES[key]["include"]
            cmd.append("-include_env")
            include = ",".join(TEST_CASES[key]["include"])
            cmd.append(include)
        #Construct -insert_env flag/args and add to command line
        if "insert" in TEST_CASES[key]:
            env_chk = TEST_CASES[key]["insert"]
            cmd.append("-insert_env")
            d = ";"
            insert = ""
            #Using a custom delim for -insert_env passed key=value pairs
            if "Delim" in TEST_CASES[key]:
                d = TEST_CASES[key]["Delim"]
                insert += d
            insert += d.join(TEST_CASES[key]["insert"])
            cmd.append(insert)
        #Add test dag file to command line args
        cmd.append(TEST_CASES[key]["dag"].name)
        #print(" ".join(cmd))
        #Run test command
        p = default_condor.run_command(cmd)
        #If we have subdags then we have to wait for internal submission for .condor.sub files
        if len(dag_files) > 1:
            #Willing to wait a max of 10 sec per dag
            max_time = 10*len(dag_files)
            start = time()
            #Loop checking for all expected .condor.sub files ot exists. If not found by max time fail
            while True:
                exist = True
                for submit in dag_files:
                    if not os.path.exists(submit):
                        exist = False
                if exist:
                    break
                assert time() - start < max_time
        #Craft lists of expected outout to check for in .condor.sub file
        env_key = TEST_CASES[key]["check_getenv"]
        if env_key == "TRUE":
            getenv_chk = GETENV_VALUES[env_key]
        elif env_key == "ALL":
            getenv_chk.extend(GETENV_VALUES["BASE"])
            getenv_chk.extend(GETENV_VALUES["CONFIG"])
        else:
            getenv_chk.extend(GETENV_VALUES[env_key])
        env_chk.extend(BASE_ENV_VALUES)
        if len(TEST_CASES[key]["dag"].config) > 0:
            env_chk.extend(CONF_ENV_VALUES)
        test_output[key] = (dag_files,getenv_chk,env_chk)
        getenv_chk = []
        env_chk = []
    yield test_output

#-------------------------------------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def runTests(request, submit_dags):
    return (request.param, submit_dags[request.param][0], submit_dags[request.param][1], submit_dags[request.param][2])
@action
def testName(runTests):
    return runTests[0]
@action
def testDagPaths(runTests):
    return runTests[1]
@action
def testGetEnv(runTests):
    return runTests[2]
@action
def testEnv(runTests):
    return runTests[3]

#-------------------------------------------------------------------------------------------------------
#Test python bindings ability to set info for insert_env and include_env
@action
def testPyBindings(default_condor,test_dir):
    dag_name = TEST_CASES["both_flags"]["dag"].name
    #Return to test dir
    os.chdir(str(test_dir))
    #Make and move to py bindings test case subdir
    path = os.path.join(str(test_dir),"py_bindings")
    if not os.path.exists(path):
        os.mkdir(path)
    os.chdir(path)
    #Copy dag file from other test dir
    other_path = os.path.join(str(test_dir),"both_flags")
    filename = os.path.join(path,dag_name)
    mv_name = os.path.join(other_path,dag_name)
    copy(mv_name,filename)
    #Make custom test parameters and make a python submit object from a dag
    find_getenv = ["Aurora_Borealis"]
    find_env = ["Lang=C++","_CONDOR_DAGMAN_LOG","_CONDOR_MAX_DAGMAN_LOG"]
    dag = htcondor.Submit.from_dag(filename,{"force":True,"include_env":"Aurora_Borealis","insert_env":"Lang=C++"})
    find_getenv.extend(GETENV_VALUES["BASE"])
    find_getenv.extend(GETENV_VALUES["CONFIG"])
    return (dag,find_getenv,find_env)

#-------------------------------------------------------------------------------------------------------
@action
def textENVcmd(default_condor, test_dir):
    dag_name = "test.dag"
    os.chdir(str(test_dir))
    #Make and move to py bindings test case subdir
    path = os.path.join(str(test_dir),"env_cmd")
    if not os.path.exists(path):
        os.mkdir(path)
    os.chdir(path)
    with open(dag_name,"w") as f:
        f.write("""
ENV GET EL_WISCORICAN SLOW_FOOD
ENV GET COMP_SCI,BIOLOGY,MATHEMATICS
ENV SET LOCATION='Madison, WI';AI=TRUE
ENV SET |GEN1_POKEMON_GAMES=Blue;Red;Yellow;|GEN2_POKEMON_GAMES=Silver;Gold;Crytal;
""")
    cmd = ["condor_submit_dag","-f","-no_submit",dag_name]
    p = default_condor.run_command(cmd)
    submit = os.path.join(path, f"{dag_name}.condor.sub")
    start = time()
    #Loop checking for .condor.sub files to exists. If not found after 10 sec fail
    while True:
        if os.path.exists(submit):
            break
        assert time() - start < 10
    get_env = ["EL_WISCORICAN","SLOW_FOOD","COMP_SCI","BIOLOGY","MATHEMATICS"]
    get_env.extend(GETENV_VALUES["BASE"])
    get_env.extend(GETENV_VALUES["CONFIG"])
    set_env = ["LOCATION","AI","GEN1_POKEMON_GAMES","GEN2_POKEMON_GAMES"]
    set_env.extend(BASE_ENV_VALUES)
    return (submit, get_env, set_env)

#=======================================================================================================
class TestDAGManSetsEnv:
    def test_dagman_proper_sets_env(self,testName,testDagPaths,testGetEnv,testEnv):
        found_env = []
        found_getenv = []
        unexpected = []
        #If true is in expected getenv output then make sure that is all we are expecting
        if "true" in testGetEnv:
            assert len(testGetEnv) == 1
        #For each .condor.sub file
        for sub_path in testDagPaths:
            #Make sure file exists
            assert os.path.exists(sub_path)
            #Read file
            f = open(sub_path,"r")
            for line in f:
                line = line.strip()
                #Read getenv line
                if line[:6] == "getenv":
                    pos = line.find("=")
                    getenv_vals = line[pos+1:].split(",")
                    for var in getenv_vals:
                        var = var.strip()
                        if var in testGetEnv:
                            found_getenv.append(var)
                        else:
                            unexpected.append(var)
                #Read Environment line
                elif line[:11] == "environment":
                    env_vals = line.split("=",1)[1]
                    for info in testEnv:
                        if info in env_vals:
                            found_env.append(info)
            #Error if we found unexpected vars in getenv
            if len(unexpected) > 0:
                tmp = ",".join(unexpected)
                print(f"ERROR: Unexpected env vars ({tmp}) found in getenv.")
                assert False
            #Assert that we found all expected items in getenv
            assert len(found_getenv) == len(testGetEnv)
            #Assert that we found all expected items in environment
            assert len(found_env) == len(testEnv)
            found_env = []
            found_getenv = []
    def test_py_bindings_set_env(self,testPyBindings):
        found_env = []
        found_getenv = []
        unexpected = []
        #Check getenv line
        getenv_vals = testPyBindings[0]["getenv"].split(",")
        for var in getenv_vals:
            var = var.strip()
            if var in testPyBindings[1]:
                found_getenv.append(var)
            else:
                unexpected.append(var)
        #Check environment line
        env_vals = testPyBindings[0]["environment"]
        for info in testPyBindings[2]:
            if info in env_vals:
                found_env.append(info)
        #Error if we found unexpected vars in getenv
        if len(unexpected) > 0:
            tmp = ",".join(unexpected)
            print(f"ERROR: Unexpected env vars ({tmp}) found in getenv.")
            assert False
        #Assert that we found all expected items in getenv
        assert len(found_getenv) == len(testPyBindings[1])
        #Assert that we found all expected items in environment
        assert len(found_env) == len(testPyBindings[2])
    def test_dag_env_cmd(self, textENVcmd):
        found_env = []
        found_getenv = []
        unexpected = []
        #Make sure file exists
        assert os.path.exists(textENVcmd[0])
        #Read file
        f = open(textENVcmd[0],"r")
        for line in f:
            line = line.strip()
            #Read getenv line
            if line[:6] == "getenv":
                pos = line.find("=")
                getenv_vals = line[pos+1:].split(",")
                for var in getenv_vals:
                    var = var.strip()
                    if var in textENVcmd[1]:
                        found_getenv.append(var)
                    else:
                        unexpected.append(var)
            #Read Environment line
            elif line[:11] == "environment":
                env_vals = line.split("=",1)[1]
                for info in textENVcmd[2]:
                    if info in env_vals:
                        found_env.append(info)
        #Error if we found unexpected vars in getenv
        if len(unexpected) > 0:
            tmp = ",".join(unexpected)
            print(f"ERROR: Unexpected env vars ({tmp}) found in getenv.")
            assert False
        #Assert that we found all expected items in getenv
        assert len(found_getenv) == len(textENVcmd[1])
        #Assert that we found all expected items in environment
        assert len(found_env) == len(textENVcmd[2])

