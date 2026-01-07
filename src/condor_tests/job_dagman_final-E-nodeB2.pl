#! /usr/bin/env perl

$outfile = "job_dagman_final-E.nodes.out";

open my $fh, ">>", $outfile or die "Cannot open $outfile: $!";
print $fh "Job for node $ARGV[0]\n";
print $fh "  DAG_STATUS=$ARGV[2]\n";
print $fh "  FAILED_COUNT=$ARGV[3]\n";
print $fh "  Removing parent DAGMan\n";
system("condor_rm $ARGV[4]");

# Time for condor_rm to take effect before we finish...
sleep(30);

print $fh "  OK done with $ARGV[0]\n";
close $fh;
exit(0);
