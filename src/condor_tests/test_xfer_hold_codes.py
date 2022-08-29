#!/usr/bin/env pytest

import logging
import classad
from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

@action
def jobInputFailureAP(default_condor):
   job = default_condor.submit(    
        {
           "log": "job_ap_input.log",
           "executable": "/bin/sleep",
           "arguments": "0",
           "transfer_executable": "false",
           "should_transfer_files": "yes",
           "transfer_input_files": "not_there_in",
        }
    )
   assert job.wait(condition=ClusterState.all_held,timeout=60)
   return job.query()[0]

@action
def jobOutputFailureAP(default_condor):
   job = default_condor.submit(    
        {
           "log": "job_ap_output.log",
           "executable": "/bin/sleep",
           "arguments": "0",
           "transfer_executable": "false",
           "should_transfer_files": "yes",
           "transfer_input_files": "/bin/true",
           "transfer_output_files": "true",
           "transfer_output_remaps": classad.quote("true=not_there_dir/blah"),
        }
    )
   assert job.wait(condition=ClusterState.all_held,timeout=60)
   return job.query()[0]


@action
def jobInputFailureEP(default_condor):
   job = default_condor.submit(    
        {
           "log": "job_ep_input.log",
           "executable": "/bin/sleep",
           "arguments": "0",
           "transfer_executable": "false",
           "should_transfer_files": "yes",
           "transfer_input_files": "http://neversslxxx.com/index.html",
        }
    )
   assert job.wait(condition=ClusterState.all_held,timeout=60)
   return job.query()[0]


@action
def jobOutputFailureEP(default_condor):
   job = default_condor.submit(    
        {
           "log": "job_ep_output.log",
           "executable": "/bin/sleep",
           "arguments": "0",
           "transfer_executable": "false",
           "should_transfer_files": "yes",
           "transfer_output_files": "not_there_out",
        }
    )
   assert job.wait(condition=ClusterState.all_held,timeout=60)
   return job.query()[0]

class TestXferHoldCodes:
   def test_jobInputFailureAP(self, jobInputFailureAP):
      assert jobInputFailureAP["HoldReasonCode"] == 13
      assert "Transfer input files failure at access point" in jobInputFailureAP["HoldReason"] 

   def test_jobOutputFailureAP(self, jobOutputFailureAP):
      assert jobOutputFailureAP["HoldReasonCode"] == 12
      assert "Transfer output files failure at access point" in jobOutputFailureAP["HoldReason"]

   def test_jobInputFailureEP(self, jobInputFailureEP):
      assert jobInputFailureEP["HoldReasonCode"] == 13
      assert "Transfer input files failure at execution point" in jobInputFailureEP["HoldReason"]

   def test_jobOutputFailureEP(self, jobOutputFailureEP):
      assert jobOutputFailureEP["HoldReasonCode"] == 12
      assert "Transfer output files failure at execution point" in jobOutputFailureEP["HoldReason"]

