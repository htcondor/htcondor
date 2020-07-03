#!/usr/bin/python3

import os
import platform
import sys
import time
import socket

print >> sys.stderr, __doc__
print "Version :", platform.python_version()
print "Program :", sys.executable
print 'Script  :', os.path.abspath(__file__)
print

s = socket.socket()
s.bind(("",0))
socketname =  s.getsockname()
print socketname
p = open('PORT','w')
print >>p,socketname
f = open('DONE','w')
print >>f,'all','done'
sys.exit(0)

