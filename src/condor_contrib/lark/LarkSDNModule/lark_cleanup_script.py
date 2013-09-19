#!/usr/bin/python

#
# This script is called when the job finishes executing to do clenup.
# Basically it sends out the the IP address of the inner network 
# interface for the job, and ask htcondor_module.py to delete the 
# network classad it stores for that specific IP address.
#

import sys
import classad
import re
import socket
import htcondor

# get job classad from stdin
job_ad = sys.stdin.readline()

# get seperator line
separator_line = sys.stdin.readline()
assert separator_line == '------\n'

# get machine classad from stdin
machine_ad = sys.stdin.readline()
machine_ad = classad.ClassAd(machine_ad)

# parse out the IP address for inner network interface
ip_src = machine_ad.eval("LarkInnerAddressIPv4")

# use "CLEAN" as message type to tell htcondor module
# that this is the message from lark cleanup script
send_data = "CLEAN" + "\n" + ip_src

# connect to the htcondor module and send out the info
HOST = htcondor.param["HTCONDOR_MODULE_HOST"]
PORT = int(htcondor.param["HTCONDOR_MODULE_PORT"])

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
    sock.connect((HOST, PORT))
    sock.sendall(send_data)
finally:
    sock.close()


