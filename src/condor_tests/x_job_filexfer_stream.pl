#!/usr/bin/env perl

my $target = $ARGV[0];
my $count = $ARGV[1];
my $sleep = $ARGV[2];

for(;$count >= 0;$count -= 1)
{
	if( $target eq "stderr" )
	{
		print STDERR "1234****$target****4321\n";
	}
	elsif( $target eq "stdout" )
	{
		print STDOUT "123456789abcdef****$target****fedcba987654321\n";
	}
	else
	{
		print STDERR "1234****$target****4321\n";
	}
}
sleep($sleep);
