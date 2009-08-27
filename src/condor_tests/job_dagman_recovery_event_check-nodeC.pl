#! /usr/bin/env perl

open (OUTPUT, "condor_hold $ARGV[0] 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_hold failed: $?";

# Remove the log of the first node, so that when we go into recovery
# mode, we'll get events that violate the DAG semantics.
system("rm -f job_dagman_recovery_event_check-nodeA.log");

sleep 60;

open (OUTPUT, "condor_release $ARGV[0] 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_release failed: $?";

print "Node C succeeded\n";
