#! /usr/bin/env perl

open (OUTPUT, "condor_hold $ARGV[0] 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_hold failed: $?";

# job_dagman_recovery_event_check.log.dummy has events for nodes B1 and
# B2, but not node A, so it violates the DAG semantics.
system("cp -f job_dagman_recovery_event_check.log.dummy job_dagman_recovery_event_check.log");

sleep 60;

open (OUTPUT, "condor_release $ARGV[0] 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_release failed: $?";

print "Node C succeeded\n";
