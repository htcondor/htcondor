#! /usr/bin/env perl

$file = "job_dagman_final-D.scripts.out";
open(OUT, ">>$file") or die "Can't open file $file";
print OUT "Node $ARGV[0] $ARGV[1]\n";
print OUT "  DAG_STATUS=$ARGV[3]\n";
print OUT "  FAILED_COUNT=$ARGV[4]\n";
print OUT "  RETURN=$ARGV[5]\n";
close(OUT);

exit($ARGV[2]);
