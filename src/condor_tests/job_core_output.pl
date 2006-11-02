#! /usr/bin/env perl
#BEGIN {$^W=1}  #warnings enabled
use CondorTest;
while( <> )
{
	CondorTest::fullchomp($_);
	print "$_\n";
}

