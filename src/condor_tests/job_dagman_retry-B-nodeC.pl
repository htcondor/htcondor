#! /usr/bin/env perl

$failfile = "job_dagman_retry-B-nodeC.fail";
if (-e $failfile) {
	system("rm -f $failfile");
	print "Node job fails\n";
	exit 1;
} else {
	print "Node job succeeds\n";
}
