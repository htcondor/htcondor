#! /usr/bin/env perl

my $file = "job_dagman_retry_recovery-nodeB.works";

if (-e $file) {
	print "Node B succeeded\n";
} else {
	system("touch $file");
	print "Node B failed\n";
	exit 1;
}
