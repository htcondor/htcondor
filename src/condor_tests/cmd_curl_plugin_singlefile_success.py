#!/usr/bin/env python
import sys
import time

success_file = open("success", "r")
success_contents = success_file.read()
if "<html>Great success!</html>" not in success_contents:
    print("ERROR: Success file contents don't match, going to sleep")
    while True:
        time.sleep(1) # Go to sleep forever to trigger a timeout

print("Success file looks good!")
sys.exit(0)
