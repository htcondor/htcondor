#! /usr/bin/env perl

my $workfile = "job_dagman_pegasus_recovery-nodeB.works";

if (-e $workfile) {
	unlink $workfile or die "Unlink failed: $!\n";
	print "Node B succeeded\n";
} else {
	print "Node B should not get re-run!\n";
	print "Node B failed\n";
	exit 1;
}
