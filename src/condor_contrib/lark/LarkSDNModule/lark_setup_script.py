#!/usr/bin/python

#
# The script takes the job and machine classad from stdin, and 
# sends it to the htcondor module that runs at the remote host 
# (where pox controller also runs) for further processing 
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
assert separator_line == "------\n"

# get machine classad from stdin
machine_ad = sys.stdin.readline()

# use "SEND" as message type to tell htcondor module
# that this is the message from lark setup script which
# sends the full job and machine ClassAds
send_data = "SEND" + "\n" + job_ad + machine_ad


# connect to the htcondor module and send out the classads
HOST = htcondor.param["HTCONDOR_MODULE_HOST"]
PORT = int(htcondor.param["HTCONDOR_MODULE_PORT"])

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
  sock.connect((HOST, PORT))
  sock.sendall(send_data)
finally:
  sock.close()
