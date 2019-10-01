#!/usr/bin/env perl

use strict;
use warnings;

# The test doesn't need it, but this code may come in handy later.
# my $jobID = `condor_q -jobads \$_CONDOR_JOB_AD -f '%d' ClusterId -f '.' dummy -f '%d' ProcId`;

my $resourceName = "SQUID";
if( defined( $ARGV[1] ) && $ARGV[1] ne "" ) {
	$resourceName = $ARGV[1];
}

# For simplicity, be sure to sleep through the first full interval.
sleep( 21 );
my $time = 21;
while( $time < $ARGV[0] ) {
	# print( "${jobID} " );
	system( 'condor_status -ads ${_CONDOR_SCRATCH_DIR}/.update.ad -af ' .
		"Assigned${resourceName}s ${resourceName}sUsage ${resourceName}sAverageUsage" );
	sleep( 10 );
	$time += 10;
}

exit( 0 );
