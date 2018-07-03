#!/usr/bin/env python
import time
success_file = open("success", "r")
success_contents = success_file.read()
if success_contents != "<html>Great success!</html>\n":
    while True:
        time.sleep(1) # Go to sleep forever to trigger a timeout