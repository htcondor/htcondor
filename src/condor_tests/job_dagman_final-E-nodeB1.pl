#! /usr/bin/env perl

$outfile = "job_dagman_final-E.nodes.out";

system("echo 'Job for node $ARGV[0]' >> $outfile");

system("echo '  DAG_STATUS=$ARGV[2]' >> $outfile");
system("echo '  FAILED_COUNT=$ARGV[3]' >> $outfile");

# Time for sibling node to run...
sleep(600);

system("echo '  OK done with $ARGV[0]' >> $outfile");
exit(0);
