#!/usr/bin/env python
import os
import sys
import time

partial_file = open("partial", "r")
partial_contents = partial_file.read()
if "<html>Content to return on partial requests</html>" not in partial_contents:
    print("Partial file contents don't match, test fail")
    while True:
        time.sleep(1) # Go to sleep forever to trigger a timeout

print("Partial file looks good!")
sys.exit(0)
