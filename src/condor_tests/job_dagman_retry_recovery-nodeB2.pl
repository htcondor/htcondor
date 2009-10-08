#! /usr/bin/env perl

my $file = "job_dagman_retry_recovery-nodeB2.works";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print "Node B2 succeeded\n";
} else {
	system("touch $file");
	print "Node B2 failed\n";
	exit 1;
}
