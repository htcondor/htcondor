#! /usr/bin/env perl

print "$ARGV[0] started\n";

# Touch file, portably, without external command
open(F, ">>$ARGV[0].started");
close(F);

print ">>$ARGV[0].started\n";

print "$ARGV[0] waiting for DAGMan to exit\n";
$lockfile = "job_dagman_rm.dag.lock";
while (-e $lockfile) {
	sleep(1);
}

# Um, actually...
# Dagman doesn't remove us, a previous job in this DAG
# has removed dagman, and the schedd some time later 
# will remove us.  To fix common race conditions, create
# a less common race condition, and sleep here, hoping
# the schedd will actually remove us
#

sleep(1000);


# We should get condor_rm'ed before we get to here...
open(G, ">>$ARGV[0].finished");
close(G);
print "$ARGV[0] finished\n";

exit(0);
