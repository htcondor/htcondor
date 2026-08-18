#! /usr/bin/env perl

$outfile = "job_dagman_final-I.nodes.out";

open my $fh, ">>", $outfile or die "Cannot open $outfile: $!";
print $fh "Job for node $ARGV[0]\n";
print $fh "  DAG_STATUS=$ARGV[2]\n";
print $fh "  FAILED_COUNT=$ARGV[3]\n";

# Halt DAG if this is node I_B.
if ($ARGV[0] eq "I_B") {
	print $fh "  Halting DAG\n";
	system("touch job_dagman_final-I.dag.halt");
	sleep(10);
}

if ($ARGV[1]) {
	print $fh "  FAILED done with $ARGV[0]\n";
	close $fh;
	exit($ARGV[1]);
} else {
	print $fh "  OK done with $ARGV[0]\n";
	close $fh;
	exit(0);
}
