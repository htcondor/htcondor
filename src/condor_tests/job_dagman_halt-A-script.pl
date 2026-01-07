#! /usr/bin/env perl

open my $fh, ">>", "job_dagman_halt-A.nodes.out" or die "Cannot open file: $!";
print $fh "Node $ARGV[1] $ARGV[0] script\n";
close $fh;
sleep(10);
