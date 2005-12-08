#!/usr/bin/env perl

##
## Simply prints out the current Unix timestamp
##
print time()."\n";

##
## We can be told to sleep for a bit as well
##
if ( $ARGV[0] ) {
	sleep( $ARGV[0] );
}

exit(0);
