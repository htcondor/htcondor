#!/usr/bin/env python
# Python analog to foo.c

import sys
import time

min_delay = 10

runtime = int(sys.argv[1])
if runtime < min_delay:
	print 'Delay has a minimum of', min_delay, 'seconds'
	runtime = min_delay

outfile = open(sys.argv[2], 'w')

print 'Dumping stdin:'
outfile.write('Dumping stdin:\n')

for line in sys.stdin:
    print 'stdin: %s' % line,
    outfile.write('stdin: %s' % line)

print 'Doing work for', runtime, 'seconds'
outfile.write('Doing work for %i seconds\n' % runtime)

for t in xrange(runtime):
    print 'i = %i' % t
    outfile.write('i = %i\n' % t)
    time.sleep(1)

print 'Done.'
outfile.write('Done.\n')

sys.exit(0);
