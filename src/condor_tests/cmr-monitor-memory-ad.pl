#!/usr/bin/env perl

use strict;
use warnings;

my $resourceName = "SQUID";
if( defined( $ARGV[1] ) && $ARGV[1] ne "" ) {
	$resourceName = $ARGV[1];
}

my $time = 0;
while( $time < $ARGV[0] ) {
	system( 'condor_status -ads ${_CONDOR_SCRATCH_DIR}/.update.ad -af '
		. "Assigned${resourceName}s ${resourceName}sMemoryUsage" );
	sleep( 10 );
	$time += 10;
}

exit( 0 );
