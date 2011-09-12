#! /usr/bin/env perl

$retry = $ARGV[0];
if ($retry gt 0) {
	print "PRE script succeeds\n";
	$file = "job_dagman_retry-B-nodeA.retry";
	open(OUT, ">$file") or die "Can't open $file\n";
	print OUT "$retry\n";
	close(OUT);
} else {
	print "PRE script fails\n";
	exit 1;
}
