#!/usr/bin/env perl

print "Starting job_dagman_abort-final-A-node-sleep.pl\n";

# This job should sleep until it gets condor_rm'ed.
sleep(9999);

print "After sleep -- we shouldn't get here!\n";

exit 1;
