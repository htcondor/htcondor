#! /usr/bin/env perl

open (OUT , ">>job_dagman_halt-A.nodes.out") || die "Can't open $!\n";
print OUT "Node $ARGV[1] $ARGV[0] script\n";
close (OUT);
