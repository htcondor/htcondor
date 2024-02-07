#! /usr/bin/env perl

print "Node $ARGV[0]\n";

if ($ARGV[1]) {
	$infile = "job_dagman_node_status.status";
	$outfile = $infile . "-" . $ARGV[0];

	# Make sure node status file is regenerated *after* this node starts
	# before we copy it.
	if (-e $infile) {
		system("rm $infile");
	}
	while (! -e $infile) {
		sleep(1);
	}

	print "  Saving $infile to $outfile\n";
	system("cp $infile $outfile");
} else {
	print "  Just sleeping...\n";
	sleep(1);
}
