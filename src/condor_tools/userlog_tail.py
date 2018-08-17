#!/usr/bin/env python

import sys;
import htcondor;

def main( argv ):
	jel = htcondor.JobEventLog( argv[1] )
	if not jel.isInitialized():
		print "Failed to find job event log {0}".format( argv[1] )
		exit( 1 )
	jel.setFollowing()
	jel.setFollowTimeout( 100 )
	for event in jel:
		print "Found event of type {0}".format( event.type() )
		if event.type() != htcondor.JobEventType.ULOG_NONE:
			print "... for job {0}".format( event.Cluster )

if __name__ == "__main__":
	main( sys.argv )
