#! /usr/bin/env perl

$outfile = "job_dagman_halt-A.nodes.out";

open OUT,">>$outfile" || die "Could not open $outfile\n";
select(OUT);
$|++;
print OUT "Node $ARGV[0] starting\n";

$halt_file = "job_dagman_halt-A.dag.halt";

if ($ARGV[1] eq "-sleep") {
	print OUT "  $ARGV[0] Sleeping\n";
	sleep(1);
	while (! -e $halt_file) {
		sleep(1);
	}
	print OUT "  $ARGV[0] found halt file\n";
	sleep(5);

} elsif ($ARGV[1] eq "-halt") {
	sleep(15); # Allow time for sibling node to start.
	print OUT "  $ARGV[0] halting DAG\n";
	system("touch $halt_file");
	sleep(5);

} elsif ($ARGV[1] eq "-unhalt") {
	print OUT "  $ARGV[0] sleeping and checking for halt file\n";
	sleep(1);
	while (! -e $halt_file) {
		sleep(1);
	}
	print OUT "  $ARGV[0] found halt file\n";
	sleep(40); # Time for next node to start if halt doesn't work...
	print OUT "  $ARGV[0] removing halt file\n";
	system("rm -f $halt_file");
	# Avoid writing anything after removing the halt file, to make
	# the test more deterministic.
	exit(0);

} else {
	sleep(10);
}

print OUT "Node $ARGV[0] succeeded\n";
close(OUT);
