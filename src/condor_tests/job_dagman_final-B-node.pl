#! /usr/bin/env perl

$outfile = "job_dagman_final-B.nodes.out";

open my $fh, ">>", $outfile or die "Cannot open $outfile: $!";
print $fh "  DAG_STATUS=$ARGV[2]\n";
print $fh "  FAILED_COUNT=$ARGV[3]\n";
print $fh "$ARGV[0]\n";
close $fh;

exit($ARGV[1]);
