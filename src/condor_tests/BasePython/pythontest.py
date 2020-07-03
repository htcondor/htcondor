#!/usr/bin/python3

import os
import platform
import sys
import time

print >> sys.stderr, __doc__
print "Version :", platform.python_version()
print "Program :", sys.executable
print 'Script  :', os.path.abspath(__file__)
print

f = open('DONE','w')
print >>f,'all','done'
sys.exit(0)

