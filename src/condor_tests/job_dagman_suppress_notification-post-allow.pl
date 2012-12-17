#!/usr/bin/env perl
use strict;

my $return = shift;
my $jobid = shift;

die "Return value not 0\n" if($return != 0);

$jobid =~ s/\..+//;

open JOBOUTPUT,"<job_dagman_suppress_notification-cmd_line-node.${jobid}.out" ||
	die "Could not open job_dagman_suppress_notification-cmd_line-node.${jobid}.out\n";

while(<JOBOUTPUT>){
	die "output incorrect: $_" if(! /^Notification = Always/);
}

close JOBOUTPUT;
exit 0;
