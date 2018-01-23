#! /usr/bin/env perl

use lib '.';
use CondorTest;
use CondorUtils;

print "Job for node $ARGV[0]\n";

@tmpfiles = ("job_dagman_rm-NodeB-pre.started",
	"job_dagman_rm-NodeB-pre.finished",
	"job_dagman_rm-NodeC-post.started",
	"job_dagman_rm-NodeC-post.finished",
	"job_dagman_rm-NodeD-job.started",
	"job_dagman_rm-NodeD-job.finished",
	"job_dagman_rm-NodeE-job.started",
	"job_dagman_rm-NodeE-job.finished",
	"job_dagman_rm-NodeF-job.started",
	"job_dagman_rm-NodeF-job.finished",
	"job_dagman_rm-NodeZ-job.started",
	"job_dagman_rm-NodeZ-job.finished");
push(@tmpfiles, glob("job_dagman_rm-NodeZ-job.*.started"));
push(@tmpfiles, glob("job_dagman_rm-NodeZ-job.*.finished"));

foreach $fname (@tmpfiles) {
	if (-e $fname) {
		print "Removing $fname\n";
		runcmd("rm -f $fname");
	}
}

print "OK done with $ARGV[0]\n";
exit(0);
