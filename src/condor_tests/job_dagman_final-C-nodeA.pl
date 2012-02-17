#! /usr/bin/env perl

$outfile = "job_dagman_final-C.nodes.out";

system("echo 'Job for node C_A' >> $outfile");
system("echo '  DAG_STATUS=$ARGV[0]' >> $outfile");
system("echo '  FAILED_COUNT=$ARGV[1]' >> $outfile");

my $file = "job_dagman_final-C-nodeA.fails";
if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	system("echo '  FAILED done with C_A' >> $outfile");
	exit(1);
} else {
	system("echo '  OK done with C_A' >> $outfile");
	exit(0);
}
