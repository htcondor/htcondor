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


# We should get condor_rm'ed before we get to here...
runcmd("touch $ARGV[0].finished");
print "$ARGV[0] finished\n";

exit(0);
