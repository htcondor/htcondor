#! /usr/bin/env perl

my $workfile = "job_dagman_pegasus_recovery-nodeA.works";

if (-e $workfile) {
	unlink $workfile or die "Unlink failed: $!\n";
	print "Node A succeeded\n";
} else {
	print "Node A should not get re-run!\n";
	print "Node A failed\n";
	exit 1;
}
