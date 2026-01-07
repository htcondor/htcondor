#! /usr/bin/env perl

$outfile = "job_dagman_final-C.nodes.out";

open my $fh, ">>", $outfile or die "Cannot open $outfile: $!";
print $fh "Job for node C_A\n";
print $fh "  DAG_STATUS=$ARGV[0]\n";
print $fh "  FAILED_COUNT=$ARGV[1]\n";

my $file = "job_dagman_final-C-nodeA.fails";
if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print $fh "  FAILED done with C_A\n";
	close $fh;
	exit(1);
} else {
	print $fh "  OK done with C_A\n";
	close $fh;
	exit(0);
}
