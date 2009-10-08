#! /usr/bin/env perl

my $file = "job_dagman_node_prio.order";
if (-e $file) {
	unlink $file;
}

open(OUT, ">>$file") or die "Can't open file $file";
print OUT "$ARGV[0] ";
close(OUT);
