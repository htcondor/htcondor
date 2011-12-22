#! /usr/bin/env perl

$outfile = "job_dagman_final-G.nodes.out";

system("echo 'Job for node $ARGV[0]' >> $outfile");

system("echo '  DAG_STATUS=$ARGV[2]' >> $outfile");
system("echo '  FAILED_COUNT=$ARGV[3]' >> $outfile");

if ($ARGV[1]) {
	system("echo '  FAILED done with $ARGV[0]' >> $outfile");
	exit($ARGV[1]);
} else {
	system("echo '  OK done with $ARGV[0]' >> $outfile");
	exit(0);
}
