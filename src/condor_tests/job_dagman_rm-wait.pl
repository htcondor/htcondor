#! /usr/bin/env perl

use CondorTest;
use CondorUtils;

print "$ARGV[0] started\n";
runcmd("touch $ARGV[0].started");

print "$ARGV[0] waiting for DAGMan to exit\n";
$lockfile = "job_dagman_rm.dag.lock";
while (-e $lockfile) {
	sleep(1);
}

runcmd("touch $ARGV[0].finished");
print "$ARGV[0] finished\n";

exit(0);
