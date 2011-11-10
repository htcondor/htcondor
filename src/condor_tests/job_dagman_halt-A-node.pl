#! /usr/bin/env perl

open (OUT, ">>job_dagman_halt-A.nodes.out") || die "Can't open $!\n";

print OUT "Node $ARGV[0] starting\n";

$halt_file = "job_dagman_halt-A.dag.halt";

if ($ARGV[1] eq "-sleep") {
	print OUT "  Sleeping\n";
	sleep(5);
	while (! -e $halt_file) {
		print OUT "  Found halt file\n";
		sleep(5);
	}

} elsif ($ARGV[1] eq "-halt") {
	print OUT "  Halting DAG\n";
	system("touch $halt_file");

} elsif ($ARGV[1] eq "-unhalt") {
	print OUT "  Sleeping\n";
	sleep(5);
	while (! -e $halt_file) {
		sleep(5);
	}
	print OUT "  Found halt file\n";
	sleep(30); # Time for next node to start...
	system("rm -f $halt_file");
	print OUT "  Removed halt file\n";
}

print OUT "Node $ARGV[0] succeeded\n";

close(OUT);
