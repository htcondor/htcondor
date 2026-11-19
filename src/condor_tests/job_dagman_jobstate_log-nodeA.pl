#! /usr/bin/env perl

# Proc 0 fails right away, so DAGMan removes proc 1 while it is still
# running.  The test expects proc 1 to have an EXECUTE (and then EVICTED)
# event, so proc 1 drops a sentinel file once it is running, and proc 0
# waits for that file before failing.  Otherwise, on a loaded machine,
# proc 1 can be removed before its starter ever execs it.
$proc = $ARGV[0];
$cluster = $ARGV[1];
$sentinel = "job_dagman_jobstate_log-nodeA.$cluster.started";

if ($proc > 0) {
	open(SENTINEL, ">$sentinel") or die "Can't create $sentinel: $!\n";
	close(SENTINEL);
	sleep(60);
} else {
	my $waited = 0;
	while (! -e $sentinel && $waited < 120) {
		sleep(1);
		$waited++;
	}
	unlink($sentinel);
}

print "Node A job failing...\n";

exit(2);
