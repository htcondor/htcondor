#!/usr/bin/env pytest

#Job Ad attribute: JobSubmitMethod assignment testing
#
#Each sub-test submits job(s) and waits till all are hopefully finished.
#Then it collects the recent job ads from schedd history and returns
#them for evaluation.
#
#Written by: Cole Bollig @ March 2022

from ornithology import *
import os
from time import sleep
from time import time


#Function to wait until schedd has no jobs
def wait(schedd):
     t_end = time() + 180 #Give a 2 minute maximum for waiting
     while any(schedd.query(constraint='true',projection=["JobSubmitMethod"])) == True:
          sleep(0.1)
          if time() >= t_end:
               break

#Fixture to write simple .sub file for general use
@action
def write_sub_file(test_dir,path_to_sleep):
     submit_file = open( test_dir / "simple_submit.sub","w")
     submit_file.write("""executable={0}
arguments=1
should_transfer_files=Yes
queue
""".format(path_to_sleep))
     submit_file.close()
     return test_dir / "simple_submit.sub"

#Fixture to write simple .dag file using write_sub_file fixture for general dag use
@action
def write_dag_file(test_dir,write_sub_file):
     dag_file = open( test_dir / "simple.dag", "w")
     dag_file.write("""JOB A {0}
JOB B {0}

PARENT A CHILD B
""".format(write_sub_file))
     dag_file.close()
     return test_dir / "simple.dag"

#Fixture to test if an unknown submission doesn't add JobSubmitMethod Attribute
@action
def test_unknown_submission(default_condor,test_dir,path_to_sleep):
     #This test works by making a fake unknown job submission via python bindings
     #To do this the python file uses the non-documented _setSubmitMethod()
     #for internal use to set the submit method value to -1 thus indicating 
     #an unknown submission method
     python_file = open(test_dir / "test_unknown.py", "w")
     python_file.write(
     """import htcondor
import classad

job = htcondor.Submit({{
     "executable":"{0}"
}})

job.setSubmitMethod(-1,True)
schedd = htcondor.Schedd()
submit_result = schedd.submit(job)
""".format(path_to_sleep))
     python_file.close()
     #^^^Make python file for submission^^^

     #Submit file to run job
     p = default_condor.run_command(["python3",test_dir / "test_unknown.py"])
     #Get the job ad for completed job
     schedd = default_condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=1,
     )
     
     return job_ad

#Fixture to run condor_submit and check the JobSubmitMethod attr
@action
def run_condor_submit(default_condor,write_sub_file):
     #Submit job
     p = default_condor.run_command(["condor_submit", write_sub_file])
     #Get the job ad for completed job
     schedd = default_condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=1,
     )
     
     return job_ad

#Fixture to run condor_submit_dag and check the JobSubmitMethod attr
@action
def run_dagman_submission(default_condor,write_dag_file):
     #Submit job 
     p = default_condor.run_command(["condor_submit_dag",write_dag_file])
     #Get the job ad for completed job
     schedd = default_condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=3,
     )
     
     return job_ad

subTestNum = 0
#Fixture to run python bindings with and without user set value and check the JobSubmitMethod attr
@action(params={
"normal":'',
"user_set(1)":"job.setSubmitMethod(-3)",
"user_set(2)":"job.setSubmitMethod(69)",
"user_set(3)":"job.setSubmitMethod(69,True)",
"user_set(4)":"job.setSubmitMethod(100)",
"user_set(5)":"job.setSubmitMethod(666)",
})#Parameter tests: Standard, User-set(-3), User-set(53), User-set(100), and User-set(666)
def run_python_bindings(default_condor,test_dir,path_to_sleep,request):
     global subTestNum
     filename = "test{}.py".format(subTestNum)
     python_file = open(test_dir / filename, "w")
     subTestNum += 1
     python_file.write(
     """import htcondor
import classad

job = htcondor.Submit({{
     "executable":"{0}"
}})

{1}
schedd = htcondor.Schedd()
submit_result = schedd.submit(job)
print(job.getSubmitMethod())
""".format(path_to_sleep,request.param))
     python_file.close()
     #^^^Make python file for submission^^^

     #Submit file to run job
     p = default_condor.run_command(["python3",test_dir / filename])

     return p

#Fixture to run 'htcondor job submit' and check the JobSubmitMethod attr
@action
def run_htcondor_job_submit(default_condor,write_sub_file):
     p = default_condor.run_command(["htcondor","job","submit",write_sub_file])
     #Get the job ad for completed job
     schedd = default_condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=1,
     )
     
     return job_ad

#Fixture to run 'htcondor jobset submit' and check the JobSubmitMethod attr
@action
def run_htcondor_jobset_submit(default_condor,test_dir,path_to_sleep):
     jobset_file = open(test_dir / "job.set", "w")
     jobset_file.write(
     """name = JobSubmitMethodTest

iterator = table var {{
     Job1
     Job2
}}

job {{
     executable = {}
     queue
}}
""".format(path_to_sleep))
     jobset_file.close()

     p = default_condor.run_command(["htcondor","jobset","submit",test_dir / "job.set"])
     #Get the job ad for completed job
     schedd = default_condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=f"JobSetName == \"JobSubmitMethodTest\"",
          projection=["JobSubmitMethod"],
          match=2,
     )
     
     return job_ad

#Fixture to run 'htcondor dag submit' and check the JobSubmitMethod attr
@action
def run_htcondor_dag_submit(default_condor,write_dag_file,test_dir):
     #List of files needed to be moved
     files_to_move = ["simple.dag.condor.sub","simple.dag.dagman.log","simple.dag.dagman.out","simple.dag.lib.err","simple.dag.lib.out","simple.dag.metrics","simple.dag.nodes.log"]
     #make directories for each dag submission test to avoid errors
     p = default_condor.run_command(["pwd"])
     p = default_condor.run_command(["mkdir",test_dir / "dag_test1"])
     p = default_condor.run_command(["mkdir",test_dir / "dag_test2"])
     #move first test files into dag_test1 directory
     for name in files_to_move:
          p = default_condor.run_command(["mv",test_dir / name, test_dir / "dag_test1"])
     #run second dag submission test 
     p = default_condor.run_command(["htcondor","dag","submit", write_dag_file])
     #move secodn test files into dag_test2 directory
     for name in files_to_move:
          p = default_condor.run_command(["mv",test_dir / name, test_dir / "dag_test2"])
     #Get the job ad for completed job
     schedd = default_condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=3,
     )
     
     return job_ad


#JobSubmitMethod Tests
class TestJobSubmitMethod:


     #Test that a job submited with value < Minimum doestn't add JobSubmitMethod attr
     def test_job_submit_unknown_doesnt_define_attribute(self,test_unknown_submission):
          passed = False
          if any(test_unknown_submission) == False:
               passed = True
          assert passed

     #Test condor_submit yields 0
     def test_condor_submit_method_value(self,run_condor_submit):
          i = 0
          passed = False
          #Check that returned job ads have a submission value of 0
          for ad in run_condor_submit:
               i += 1
               #If job ad submit method is not 0 then fail test
               if ad["JobSubmitMethod"] == 0:
                    passed = True
          if i != 1:
               passed = False
          #If made it this far then the test passed
          assert passed

     #Test condor_submit yields 0 for dag and 1 for dag submitted jobs
     def test_dagman_submit_job_value(self,run_dagman_submission):
          countDAG = 0
          countJobs = 0
          passed = False
          #Check that returned job ads 
          for ad in run_dagman_submission:
               if ad["JobSubmitMethod"] == 0:
                    countDAG += 1
               if ad["JobSubmitMethod"] == 1:
                    countJobs += 1
          if countDAG == 1 and countJobs == 2:
               passed = True

          #If made it this far then the test passed
          assert passed

     #Test python bindings job submission yields 2 for normal submission and 6 for when a user sets the value to 6
     def test_python_bindings_submit_method_value(self,run_python_bindings):
          passed = False
          if run_python_bindings.stdout == '2':#Check normal
               passed = True
          elif run_python_bindings.stdout == "-3":#Check user-set(1)
               passed = True
          elif run_python_bindings.stdout == "69":#Check user-set(3)
               passed = True
          elif run_python_bindings.stdout == "100":#Check user-set(4)
               passed = True
          elif run_python_bindings.stdout == "666":#Check user-set(5)
               passed = True
          elif "htcondor.HTCondorValueError: Submit Method value must be" in run_python_bindings.stderr:#Check user-set(2)
               if "or greater. Or allow_reserved_values must be set to True." in run_python_bindings.stderr:
                    passed = True
          assert passed

     #Test 'htcondor job submit' yields 3
     def test_htcondor_job_submit_method_value(self,run_htcondor_job_submit):
          i = 0
          passed = False
          #Check that returned job ads have a submission value of 3
          for ad in run_htcondor_job_submit:
               i += 1
               #If job ad submit method is not 3 then fail test
               if ad["JobSubmitMethod"] == 3:
                    passed = True
          if i != 1:
               passed = False
          #If made it this far then the test passed
          assert passed

     #Test 'htcondor dag submit yields 4
     def test_htcondor_dag_submit_method_value(self,run_htcondor_dag_submit):
          countDAG = 0
          countJobs = 0
          passed = False
          #Check that returned job ads 
          for ad in run_htcondor_dag_submit:
               if ad["JobSubmitMethod"] == 4:
                    countDAG += 1
               if ad["JobSubmitMethod"] == 1:
                    countJobs += 1
          if countDAG == 1 and countJobs == 2:
               passed = True

          #If made it this far then the test passed
          assert passed

     #Test 'htcondor jobset submit yields 5
     def test_htcondor_jobset_submit_method_value(self,run_htcondor_jobset_submit):
          i = 0
          passed = False
          #Check that returned job ads have a submission value of 4
          for ad in run_htcondor_jobset_submit:
               i += 1
               #If job ad submit method is not 4 then fail test
               if ad["JobSubmitMethod"] == 5:
                    passed = True
          if i != 2:
               passed = False
          #If made it this far then the test passed
          assert passed





