#! /usr/bin/env perl

print "Node $ARGV[0]\n";

if ($ARGV[1]) {
	$infile = "job_dagman_node_status.status";
	$outfile = $infile . "-" . $ARGV[0];

	# Make sure node status file is regenerated *after* this node starts
	# before we copy it.
	if (-e $infile) {
		unlink $infile;
	}
	while (! -e $infile) {
		sleep(1);
	}

	# DAGMan writes the status file atomically (write temp + rename), so
	# its inode can be replaced at any moment. A read/write copy races
	# that rename -- GNU cp >= 9 detects the inode swap and aborts with
	# "skipping file ..., as it was replaced while being copied", leaving
	# no output. rename(2) is a single atomic syscall, so it cannot race.
	print "  Saving $infile to $outfile\n";
	rename($infile, $outfile) or die "rename $infile -> $outfile failed: $!\n";
} else {
	print "  Just sleeping...\n";
	sleep(1);
}
