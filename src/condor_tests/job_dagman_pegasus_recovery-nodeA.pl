#! /usr/bin/env perl

my $workfile = "job_dagman_pegasus_recovery-nodeA.works";

# This job should only work once -- so that the DAG will fail if
# recovery mode doesn't work and this job gets re-submitted.
if (-e $workfile) {
	unlink $workfile or die "Unlink failed: $!\n";
	print "Node A succeeded\n";
} else {
	print "Node A should not get re-run!\n";
	print "Node A failed\n";
	exit 1;
}
