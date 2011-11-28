#! /usr/bin/env perl

$file = "job_dagman_final-B-scripts.out";
open(OUT, ">>$file") or die "Can't open file $file";
print OUT "DAG_STATUS=$ARGV[2]\n";
print OUT "Node $ARGV[0] $ARGV[1]\n";
close(OUT);

exit($ARGV[3]);
