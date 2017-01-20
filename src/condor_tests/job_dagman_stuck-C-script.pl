#! /usr/bin/env perl

$outfile = "job_dagman_stuck-C-" . $ARGV[0] . $ARGV[1]. ".out";

# This should be at least several DAGMan submit cycles.
sleep(20);

open(OUT, ">$outfile") or die "Can't open file $outfile";

print OUT "Script $ARGV[0] $ARGV[1] finishing\n";

close(OUT);
