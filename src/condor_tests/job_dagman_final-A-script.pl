#! /usr/bin/env perl

$file = "job_dagman_final-A-nodes.out";
open(OUT, ">>$file") or die "Can't open file $file";
print OUT "Node $ARGV[0] $ARGV[1]\n";
close(OUT);

