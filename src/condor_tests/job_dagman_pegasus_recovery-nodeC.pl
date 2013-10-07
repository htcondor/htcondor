#! /usr/bin/env perl

open (OUTPUT, "condor_hold $ARGV[0] 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_hold failed: $?";

# Make sure DAGMan can't read the individual node job log files
# when it goes into recovery mode.
unlink "job_dagman_pegasus_recovery-nodeA.log" or die "Unlink failed: $!\n";
unlink "job_dagman_pegasus_recovery-nodeB.log" or die "Unlink failed: $!\n";

sleep 60;

open (OUTPUT, "condor_release $ARGV[0] 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_release failed: $?";

print "Node C succeeded\n";
