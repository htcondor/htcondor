#!/usr/bin/env python

import os
import sys
import time

os.system("echo PWD")
os.system("pwd")
os.system("echo --")

os.system("echo ls -alp")
os.system("ls -alp")
os.system("echo --")

os.system("echo cat .multifile_curl_plugin.in")
os.system("cat .multifile_curl_plugin.in")
os.system("echo --")

os.system("echo cat .multifile_curl_plugin.out")
os.system("cat .multifile_curl_plugin.out")

# Check for the existence of all three files: file1, file2, file3
# If they're not all here, then go to sleep and let the test timeout.




