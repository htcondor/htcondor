#!/usr/bin/env perl

use strict;
use warnings;

my $time = 0;
while( $time < $ARGV[0] ) {
	system( 'condor_status -ads ${_CONDOR_SCRATCH_DIR}/.update.ad -af AssignedSQUIDs SQUIDsMemoryUsage' );
	sleep( 10 );
	$time += 10;
}

exit( 0 );
