#! /usr/bin/env perl

$outfile = "job_dagman_final-I.nodes.out";

system("echo 'Job for node $ARGV[0]' >> $outfile");

system("echo '  DAG_STATUS=$ARGV[2]' >> $outfile");
system("echo '  FAILED_COUNT=$ARGV[3]' >> $outfile");

# Halt DAG if this is node I_B.
if ($ARGV[0] eq "I_B") {
	system("echo '  Halting DAG' >> $outfile");
	system("touch job_dagman_final-I.dag.halt");
	sleep(10);
}

if ($ARGV[1]) {
	system("echo '  FAILED done with $ARGV[0]' >> $outfile");
	exit($ARGV[1]);
} else {
	system("echo '  OK done with $ARGV[0]' >> $outfile");
	exit(0);
}
