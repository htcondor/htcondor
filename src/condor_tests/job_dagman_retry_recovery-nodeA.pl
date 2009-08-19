#! /usr/bin/env perl

my @files = ("job_dagman_retry_recovery-nodeB.works",
		"job_dagman_retry_recovery-nodeB2.works");

foreach $file (@files) {
	if (-e $file) {
		unlink $file or die "Unlink failed: $!";
		print "Unlinked $file\n";
	}
}

print "Node A succeeded\n";
