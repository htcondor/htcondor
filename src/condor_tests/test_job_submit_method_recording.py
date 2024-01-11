#!/usr/bin/env pytest

#Job Ad attribute: JobSubmitMethod assignment testing
#
#Each sub-test submits job(s) and waits till all are hopefully finished.
#Then it collects the recent job ads from schedd history and returns
#them for evaluation.
#


from ornithology import *
import os
from glob import glob as find
from shutil import move
from time import sleep
from time import time

# Setup a personal condor 
@standup
def condor(test_dir):
    with Condor(test_dir / "condor", config={"USE_JOBSETS": True}) as condor:
        yield condor

#-----------------------------------------------------------------------------------------
#Function to wait until schedd has no jobs
def wait(schedd):
     t_end = time() + 180 #Give a 2 minute maximum for waiting
     while any(schedd.query(constraint='true',projection=["JobSubmitMethod"])) == True:
          sleep(0.1)
          if time() >= t_end:
               print("Job took longer than 2 minutes to complete")
               break
#-----------------------------------------------------------------------------------------
#Function to clean up files created by dags
def clean_up(t_dir, dir_name, dag_fname="simple.dag."):
     regex = dag_fname + "*"
     dir_path = os.path.join(str(t_dir),dir_name)
     #make directory dag submission tests to avoid errors
     os.mkdir(dir_path)
     #move test files into dag_test# directory
     for file_name in find(os.path.join(str(t_dir),regex)):
          move(os.path.join(str(t_dir),file_name),dir_path)
#-----------------------------------------------------------------------------------------
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
#-----------------------------------------------------------------------------------------
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
#-----------------------------------------------------------------------------------------
#Fixture to test if an unknown submission doesn't add JobSubmitMethod Attribute
@action
def test_unknown_submission(condor,test_dir,path_to_sleep):
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
     p = condor.run_command(["python3",test_dir / "test_unknown.py"])
     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     t_end = time() + 180 #Give a 2 minute maximum for waiting
     while any(schedd.query(constraint='true',projection=["JobStatus"])) == True:
          sleep(0.1)
          if time() >= t_end:
               print("Job took longer than 2 minutes to complete")
               break
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=1,
     )
     
     return job_ad
#-----------------------------------------------------------------------------------------
#Fixture to run condor_submit and check the JobSubmitMethod attr
@action
def run_condor_submit(condor,write_sub_file):
     #Submit job
     p = condor.run_command(["condor_submit", write_sub_file])
     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=1,
     )
     
     return job_ad
#-----------------------------------------------------------------------------------------
#Fixture to run condor_submit_dag and check the JobSubmitMethod attr
@action
def run_dagman_submission(condor,test_dir,write_dag_file):
     #Submit job 
     p = condor.run_command(["condor_submit_dag",write_dag_file])
     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=3,
     )
     
     clean_up(test_dir,"submit_command")

     return job_ad
#-----------------------------------------------------------------------------------------
#Fixture to run condor_submit_dag on inline submit description dag and check the JobSubmitMethod attr
@action
def run_dagman_inline_submission(condor,test_dir,path_to_sleep):
     #Submit job
     dag_filename = "inline.dag"
     with open(dag_filename, "w") as f:
         f.write(f"""
SUBMIT-DESCRIPTION sleep {{
    executable = {path_to_sleep}
    arguments  = 0
    log        = $(JOB).log
}}

JOB DESC sleep
JOB INLINE {{
    executable = {path_to_sleep}
    arguments  = 0
    log        = $(JOB).log
}}
""")
     p = condor.run_command(["condor_submit_dag",dag_filename])
     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=3,
     )

     clean_up(test_dir,"dagman_inline",dag_filename)

     return job_ad

#-----------------------------------------------------------------------------------------
#Fixture to run condor_submit_dag with direct submission off and check the JobSubmitMethod attr
@action
def run_dagman_direct_false_submission(condor,test_dir,write_dag_file):
     # Write custom DAG config to turn of direct submit
     config = "custom.conf"
     with open(config, "w") as f:
         f.write("DAGMAN_USE_DIRECT_SUBMIT = False\n")
     # Add custom config file to DAG description
     with open(str(write_dag_file), "a") as f:
         f.write(f"CONFIG {config}\n")
     #Submit job
     p = condor.run_command(["condor_submit_dag",write_dag_file])
     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=3,
     )
     # Stop gap: Empty config to not effect other tests
     with open(config, "w") as f:
         f.write("")
     clean_up(test_dir,"no_direct_submit")

     return job_ad
#-----------------------------------------------------------------------------------------
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
def run_python_bindings(condor,test_dir,path_to_sleep,request):
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
     p = condor.run_command(["python3",test_dir / filename])

     return p
#-----------------------------------------------------------------------------------------
#Fixture to run 'htcondor job submit' and check the JobSubmitMethod attr
@action
def run_htcondor_job_submit(condor,write_sub_file):
     p = condor.run_command(["htcondor","job","submit",write_sub_file])
     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=1,
     )
     
     return job_ad
#-----------------------------------------------------------------------------------------
#Fixture to run 'htcondor jobset submit' and check the JobSubmitMethod attr
@action
def run_htcondor_jobset_submit(condor,test_dir,path_to_sleep):
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

     p = condor.run_command(["htcondor","jobset","submit",test_dir / "job.set"])
     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=f"JobSetName == \"JobSubmitMethodTest\"",
          projection=["JobSubmitMethod"],
          match=2,
     )
     
     return job_ad
#-----------------------------------------------------------------------------------------
#Fixture to run 'htcondor dag submit' and check the JobSubmitMethod attr
@action
def run_htcondor_dag_submit(condor,write_dag_file,test_dir):

     #run second dag submission test 
     p = condor.run_command(["htcondor","dag","submit", write_dag_file])

     #Get the job ad for completed job
     schedd = condor.get_local_schedd()
     wait(schedd)
     job_ad = schedd.history(
          constraint=None,
          projection=["JobSubmitMethod"],
          match=3,
     )

     clean_up(test_dir,"htcondor_cli")
     
     return job_ad

#=========================================================================================
#JobSubmitMethod Tests
class TestJobSubmitMethod:

#-----------------------------------------------------------------------------------------
     #Test that a job submited with value < Minimum doestn't add JobSubmitMethod attr
     def test_job_submit_unknown_doesnt_define_attribute(self,test_unknown_submission):
          passed = False
          if any(test_unknown_submission) == False:
               passed = True
          assert passed
#-----------------------------------------------------------------------------------------
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
#-----------------------------------------------------------------------------------------
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
#-----------------------------------------------------------------------------------------
     #Test condor_submit yields 0 for dag and 1 for dag submitted jobs test inline submit descriptions
     def test_dagman_inline_submit_job_value(self,run_dagman_inline_submission):
          countDAG = 0
          countJobs = 0
          #Check that returned job ads
          for ad in run_dagman_inline_submission:
               if ad["JobSubmitMethod"] == 0:
                    countDAG += 1
               if ad["JobSubmitMethod"] == 1:
                    countJobs += 1
          assert countDAG == 1 and countJobs == 2

#-----------------------------------------------------------------------------------------
     #Test condor_submit with direct submit false yields 0 for all jobs in dag
     def test_dagman_direct_false_job_value(self,run_dagman_direct_false_submission):
          count = 0
          passed = False
          #Check that returned job ads 
          for ad in run_dagman_direct_false_submission:
               if ad["JobSubmitMethod"] == 0:
                    count += 1
          if count == 3:
               passed = True

          #If made it this far then the test passed
          assert passed
#-----------------------------------------------------------------------------------------
     #Test python bindings job submission yields 2 for normal submission and sets user set values correctly
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
#-----------------------------------------------------------------------------------------
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
#-----------------------------------------------------------------------------------------
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
#-----------------------------------------------------------------------------------------
     #Test 'htcondor jobset submit yields 5
     def test_htcondor_jobset_submit_method_value(self,run_htcondor_jobset_submit):
          i = 0
          passed = False
          #Check that returned job ads have a submission value of 5
          for ad in run_htcondor_jobset_submit:
               i += 1
               #If job ad submit method is not 5 then fail test
               if ad["JobSubmitMethod"] == 5:
                    passed = True
          if i != 2:
               passed = False
          #If made it this far then the test passed
          assert passed



