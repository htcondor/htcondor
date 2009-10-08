#! /usr/bin/env perl

my $file = "job_dagman_node_prio.order";

open(OUT, ">>$file") or die "Can't open file $file";
print OUT "$ARGV[0] ";
close(OUT);
