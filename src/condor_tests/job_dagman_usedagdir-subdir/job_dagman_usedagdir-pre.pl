#! /usr/bin/env perl

$file = "job_dagman_usedagdir-" . $ARGV[0] . "-pre.out";
open(OUT, ">$file") or die "Can't open $file\n";

print OUT "$ARGV[0] pre script\n";

close(OUT);
