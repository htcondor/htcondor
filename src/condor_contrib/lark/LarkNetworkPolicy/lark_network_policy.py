#!/usr/bin/python

import os
import sys

condor_collector_path = os.path.join(os.getcwd(), "../../../condor_collector.V6")
condor_schedd_path = os.path.join(os.getcwd(),"../../../condor_schedd.V6")
condor_master_path = os.path.join(os.getcwd(),"../../../condor_master.V6")
condor_tool_path = os.path.join(os.getcwd(), "../../../condor_tools")
original_path = os.environ["PATH"]
updated_path = condor_collector_path + ":" + condor_schedd_path + ":" + condor_master_path + ":" + condor_tool_path + ":" + original_path
os.environ["PATH"] = updated_path

condor_utils_path = os.path.join(os.getcwd(), "../../../condor_utils")
classad_path = os.path.join(os.getcwd(), "../../../classad")
bld_external_classad_path = os.path.join(os.getcwd(), "../../../../bld_external/classads-7.9.5/install/lib")
ld_library_path = condor_utils_path + ":" + classad_path + ":" + bld_external_classad_path
os.environ["LD_LIBRARY_PATH"] = ld_library_path

sys.path.append(os.path.join(os.getcwd(), "../../python-bindings"))

import classad

job_ad_str = str()
machine_ad_str = str()

for line in sys.stdin:
	if line == '----------\n':
		break
	else:
		job_ad_str = job_ad_str + line

for line in sys.stdin:
	machine_ad_str = machine_ad_str + line

job_ad = classad.parseOld(job_ad_str)
machine_ad = classad.parseOld(machine_ad_str)
machine_ad_update = classad.ClassAd()

# flags used to indicate whether the network attributes are in the job classad
# by default it is false (not existed)
flag1 = False
flag2 = False
flag3 = False

if "InboundConnectivity" in job_ad.keys():
    flag1 = True
    expr1 = job_ad.lookup("InboundConnectivity")
if "OutboundConnectivity" in job_ad.keys():
    flag2 = True
    expr2 = job_ad.lookup("OutboundConnectivity")
if "NetworkAccounting" in job_ad.keys():
    flag3 = True
    expr3 = job_ad.lookup("NetworkAccounting")
if "OpenflowSupport" in job_ad.keys():
    flag4 = True
    expr4 = job_ad.lookup("OpenflowSupport")

if  flag1 == True and flag2 == True:
    if expr1.eval() == False and expr2.eval() == True:
        machine_ad_update["LarkNetworkType"] = 'nat'
    if expr1.eval() == True and expr2.eval() == True:
        if flag4 == True:
            if expr4.eval() == True:
                machine_ad_update["LarkNetworkType"] = 'ovs_bridge'
            else:
                machine_ad_update["LarkNetworkType"] = 'bridge'
        else:
            machine_ad_update["LarkNetworkType"] = 'bridge'
        machine_ad_update["LarkBridgeDevice"] = 'eth0'
        machine_ad_update["LarkAddressType"] = 'dhcp'
if flag3 == True:
    if expr3.eval() == True:
        machine_ad_update["LarkNetworkAccounting"] = True

print machine_ad_update.printOld()
