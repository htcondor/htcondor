#! /usr/bin/env perl
use CondorTest;

#BEGIN {$^W=1}  #warnings enabled

while( <> )
{
	CondorTest::fullchomp($_);
	print "$_\n";
}

