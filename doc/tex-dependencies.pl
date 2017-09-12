#!/usr/bin/env perl

use strict;
use warnings;

my $file = $ARGV[0];
if(! defined( $file )) {
	die( "usage: $0 <file>\n" );
}

exit( writeDependencies( $file ) );

sub writeDependencies {
	my( $file ) = @_;
	(-e $file) || die( "File '$file' does not exist.\n" );

	my $fh;
	open( $fh, "<", $file );

	while( my $line = <$fh> ) {
		if( $line =~ m/^%/ ) { next; }
		if( $line =~ m/\\input{([^}]+)}/ ) {
			print( "$1 " );
			writeDependencies( $1 );
		}
	}

	close( $fh );
}

