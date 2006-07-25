#! /usr/bin/env perl

my $request = $ARGV[0];
if($request eq "0")
{
	while(1) { sleep 1 };
}
else
{
	sleep $request;
}
exit(0);
