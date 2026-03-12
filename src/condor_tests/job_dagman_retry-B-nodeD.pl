#! /usr/bin/env perl

$failfile = "job_dagman_retry-B-nodeD.fail";
if (-e $failfile) {
	unlink $failfile;
	print "Node job fails\n";
	exit 1;
} else {
	print "Node job succeeds\n";
}
