#! /usr/bin/env perl

open (OUTPUT, "condor_q -l $ARGV[0] 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_q failed: $?";
