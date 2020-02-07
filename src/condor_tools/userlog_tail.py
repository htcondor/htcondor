#!/usr/bin/env python

import sys;
import htcondor;

# Still needs a configurable timeout to match userlog_tail.cpp.
# Also would need to print the whole classad, but that should be
# a simple accessor to add.

def main( argv ):
	try:
		jel = htcondor.JobEventLog( argv[1] )
	except IOError ioe:
		print "Failed to find job event log {0}".format( argv[1] )
		exit( 1 )

	if( str(jel) != str(iter(jel))):
		print( "jel != iter(jel)" );
		exit( 2 )

	if( str(jel) != str(jel.follow())):
		print( "jel != jel.follow()" );
		exit( 3 )

	if( str(jel) != str(jel.follow( 100 ))):
		print( "jel != jel.follow( 100 )" );
		exit( 3 )

	for event in jel.follow( 1000 ):
		print "Found event of type {0}".format( event.type )
		if event.type != htcondor.JobEventType.NONE:
			print "... for job {0}".format( event.Cluster )

if __name__ == "__main__":
	main( sys.argv )
