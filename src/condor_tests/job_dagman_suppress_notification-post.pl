#!/usr/bin/env perl
use strict;

my $return = shift;
my $jobid = shift;

die "Rreturn value not 0\n" if($return != 0);

$jobid =~ s/\..+//;

open JOBOUTPUT,"<job_dagman_suppress_notification-node.${jobid}.out" ||
	die "Could not open job_dagman_suppress_notification-node.${jobid}.out\n";

while(<JOBOUTPUT>){
	die "output incorrect: $_" if(! /^Notification = Never/);
}
exit 0;
