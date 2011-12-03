#! /usr/bin/env perl

$file = "job_dagman_final-C.scripts.out";
open(OUT, ">>$file") or die "Can't open file $file";
print OUT "Node $ARGV[0] $ARGV[1]\n";
print OUT "  DAG_STATUS=$ARGV[2]\n";
print OUT "  FAILED_COUNT=$ARGV[3]\n";
close(OUT);

exit($ARGV[4]);
