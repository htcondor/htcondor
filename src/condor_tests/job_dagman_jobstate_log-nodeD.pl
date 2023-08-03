#! /usr/bin/env perl

my $file = "job_dagman_jobstate_log-nodeD.fails";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print "Node D failed\n";
	exit(1);
} else {
	print "Node D succeeded\n";
}
