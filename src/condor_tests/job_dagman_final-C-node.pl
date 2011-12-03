#! /usr/bin/env perl

$outfile = "job_dagman_final-C.nodes.out";

system("echo '$ARGV[0]' >> $outfile");

system("echo '  DAG_STATUS=$ARGV[2]' >> $outfile");
system("echo '  FAILED_COUNT=$ARGV[3]' >> $outfile");

exit($ARGV[1]);
