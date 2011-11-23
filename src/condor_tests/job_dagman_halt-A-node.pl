#! /usr/bin/env perl

$outfile = "job_dagman_halt-A.nodes.out";

system("echo 'Node $ARGV[0] starting' >> $outfile");

$halt_file = "job_dagman_halt-A.dag.halt";

if ($ARGV[1] eq "-sleep") {
	system("echo '  $ARGV[0] Sleeping' >> $outfile");
	sleep(1);
	while (! -e $halt_file) {
		sleep(1);
	}
	system("echo '  $ARGV[0] found halt file' >> $outfile");
	sleep(5);

} elsif ($ARGV[1] eq "-halt") {
	sleep(15); # Allow time for sibling node to start.
	system("echo '  $ARGV[0] halting DAG' >> $outfile");
	system("touch $halt_file");
	sleep(5);

} elsif ($ARGV[1] eq "-unhalt") {
	system("echo '  $ARGV[0] sleeping and checking for halt file' >> $outfile");
	sleep(1);
	while (! -e $halt_file) {
		sleep(1);
	}
	system("echo '  $ARGV[0] found halt file' >> $outfile");
	sleep(40); # Time for next node to start...
	system("rm -f $halt_file");
	system("echo '  $ARGV[0] removed halt file' >> $outfile");

} else {
	sleep(10);
}

system("echo 'Node $ARGV[0] succeeded' >> $outfile");
