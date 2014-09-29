#! /usr/bin/env perl

use CondorTest;
use CondorUtils;

print "$ARGV[0] started\n";
runcmd("touch $ARGV[0].started");


# Make sure the other jobs/scripts are all started.
@tmpfiles = ("job_dagman_rm-NodeB-pre.started",
	"job_dagman_rm-NodeC-post.started",
	"job_dagman_rm-NodeD-job.started");

foreach $fname (@tmpfiles) {
	while (!-e $fname) {
		sleep(5);
	}
	print "Saw $fname\n";
}


print "Condor_rm'ing parent DAGMan ($ARGV[1])\n";

my @array = ();
runCondorTool("condor_rm $ARGV[1]",\@array,2,{emit_output=>1});

print "$ARGV[0] waiting for DAGMan to exit\n";
$lockfile = "job_dagman_rm.dag.lock";
while (-e $lockfile) {
	sleep(1);
}


runcmd("touch $ARGV[0].finished");
print "$ARGV[0] finished\n";

exit(0);
