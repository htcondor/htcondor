#! /usr/bin/env perl

$retry = 0;
$file = "job_dagman_retry-B-nodeB.retry";
open (IN, "<$file") or die "Can't open $file\n";
while (<IN>) {
	chomp();
	$retry = $_;
}
close(IN);

if ($retry gt 1) {
	print "Node job succeeds\n";
} else {
	print "Node job fails\n";
	exit 1;
}
