#!/usr/bin/env perl

my $target = $ARGV[0];
my $count = $ARGV[1];


if( $target eq "stderr" )
{
	while( $count > 0 )
	{
		print STDERR "$target\n";
		$count -= 1;
	}
}
elsif( $target eq "stdout" )
{
	while( $count > 0 )
	{
		print STDOUT "$target\n";
		$count -= 1;
	}
}
else
{
	while( $count > 0 )
	{
		print STDERR "$target\n";
		$count -= 1;
	}
}
