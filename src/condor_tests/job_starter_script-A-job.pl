#! /usr/bin/env perl

$outfile = "job_starter_script-A.out2";

open(OUT, ">>$outfile") or die "Can't open file $outfile";
print OUT "Job: ";
foreach $arg (@ARGV) {
	print OUT "<$arg> ";
}
print OUT "\n";
close(OUT);
