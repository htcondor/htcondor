#! /usr/bin/env perl

$outfile = "job_dagman_final-E.nodes.out";

system("echo 'Job for node $ARGV[0]' >> $outfile");

system("echo '  DAG_STATUS=$ARGV[2]' >> $outfile");
system("echo '  FAILED_COUNT=$ARGV[3]' >> $outfile");
system("echo '  Removing parent DAGMan' >> $outfile");
system("condor_rm $ARGV[4]");

# Time for condor_rm to take effect before we finish...
sleep(30);

system("echo '  OK done with $ARGV[0]' >> $outfile");
exit(0);
