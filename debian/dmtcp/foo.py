#!/usr/bin/env python
# Python analog to foo.c

import sys
import time

runtime = int(sys.argv[1])
outfile = open(sys.argv[2], 'w')

outfile.write('Dumping stdin:\n')
for line in sys.stdin:
    print 'stdin: %s' % line
    outfile.write('stdin: %s' % line)
outfile.write('Doing work for %i seconds\n' % runtime)
for t in xrange(runtime):
    print 'i = %i' % t
    outfile.write('i = %i\n' % t)
    time.sleep(1)
outfile.write('Done.\n')
