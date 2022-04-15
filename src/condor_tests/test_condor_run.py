#! /usr/bin/env pytest

#-------------------------------------------------------------------------------------------------
#This is a very rudimentary test for condor_run since there was no original test provided
#Written 03-31-2022 by Cole Bollig
#Test 1: Make sure condor_run works with passing a python script
#Test 2: Test that a warning caused by a submit command being mistyped has its warning outputted.
#        The change to condor_run should effectively output all errors and warnings at submission.
#-------------------------------------------------------------------------------------------------

from ornithology import *

#submit 'condor_run script'
@action
def condor_run_sleep_succeeds(default_condor, path_to_sleep):
     p = default_condor.run_command(["condor_run", "{0} 1".format(path_to_sleep)])
     return p

#submit 'condor_run -a mispelled-submit-command script'
@action
def condor_run_warning_outputs(default_condor,path_to_sleep):
     p = default_condor.run_command(["condor_run", "-a", """requrements=(OpSys=="LINUX")""", "{0} 1".format(path_to_sleep)])
     return p

class TestCondorRun:
     def test_condor_run_sleep_succeeds(self, condor_run_sleep_succeeds):
     #check to make sure that 'condor_run script' works
          passed = True
          if "[0/1] sleeping for 1 second at" in condor_run_sleep_succeeds.stdout and "Done sleeping at" in condor_run_sleep_succeeds.stdout:
               print("\nJob completed.")
          else:
               passed = False
               print("\nJob output is incorrect.")

          if condor_run_sleep_succeeds.stderr != '':
               passed = False
               print("Errors occured.")
          assert passed

     def test_condor_run_stderr_contains_warnings(self, condor_run_warning_outputs):
     #check to make sure 'condor_run -a mispelled-submit-command script' yields a warning
          passed = True
          if "[0/1] sleeping for 1 second at" in condor_run_warning_outputs.stdout and "Done sleeping at" in condor_run_warning_outputs.stdout:
               print("\nJob completed.")
          else:
               passed = False
               print("\nJob output is incorrect.")

          if """WARNING: the line 'requrements = (OpSys=="LINUX")' was unused by condor_submit. Is it a typo?""" == condor_run_warning_outputs.stderr:
               print("Warning appeared as expected.")
          else:
               passed = False
               print("Warning did not appear in stderr.")
          print(condor_run_warning_outputs.stderr)
          assert passed
