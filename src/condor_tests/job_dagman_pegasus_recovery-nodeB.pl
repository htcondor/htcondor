#! /usr/bin/env perl

my $workfile = "job_dagman_pegasus_recovery-nodeB.works";

# This job should only work once -- so that the DAG will fail if
# recovery mode doesn't work and this job gets re-submitted.
if (-e $workfile) {
	unlink $workfile or die "Unlink failed: $!\n";
	print "Node B succeeded\n";
} else {
	print "Node B should not get re-run!\n";
	print "Node B failed\n";
	exit 1;
}
