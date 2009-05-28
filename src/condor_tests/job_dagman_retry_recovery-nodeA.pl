#! /usr/bin/env perl

my $file = "job_dagman_retry_recovery-nodeB.works";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print "Unlinked $file\n";
}

print "Node A succeeded\n";
