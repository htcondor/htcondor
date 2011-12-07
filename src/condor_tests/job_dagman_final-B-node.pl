#! /usr/bin/env perl

$outfile = "job_dagman_final-B.nodes.out";

system("echo '  DAG_STATUS=$ARGV[2]' >> $outfile");
system("echo '  FAILED_COUNT=$ARGV[3]' >> $outfile");

system("echo '$ARGV[0]' >> $outfile");

exit($ARGV[1]);
