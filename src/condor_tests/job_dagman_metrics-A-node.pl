#! /usr/bin/env perl

#sleep(10);#TEMPTEMP?

print "Running node @ARGV\n";

if ($ARGV[0] eq "L2") {
	print "Failing for test\n";
	exit(1);
}
