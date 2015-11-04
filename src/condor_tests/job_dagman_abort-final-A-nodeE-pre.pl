#!/usr/bin/env perl

use CondorTest;
use CondorUtils;

$outfile = "job_dagman_abort-final-A-nodeE-pre.out";

if (-e $outfile) {
	runcmd("rm -f $outfile");
}

open(OUT, ">$outfile") or die "Couldn't open output file $outfile: $!";
print OUT "FailedCount: $ARGV[0]\n";
close(OUT);
