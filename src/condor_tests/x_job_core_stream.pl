#!/usr/bin/env perl

my $target = $ARGV[0];
my $count = $ARGV[1];

if( $target eq "stderr" )
{
	while( $count > 0 )
	{
		if( ($count % 1000) == 0 )
		{
			sleep 2;
		}
		print STDERR "$target\n";
		$count -= 1;
	}
}
elsif( $target eq "stdout" )
{
	while( $count > 0 )
	{
		if( ($count % 1000) == 0 )
		{
			sleep 2;
		}
		print STDOUT "$target\n";
		$count -= 1;
	}
}
else
{
	while( $count > 0 )
	{
		if( ($count % 1000) == 0 )
		{
			sleep 2;
		}
		print STDERR "$target\n";
		$count -= 1;
	}
}
