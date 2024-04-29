#! /usr/bin/env perl

use lib '.';
use CondorTest;
use CondorUtils;

print "$ARGV[0] started\n";
runcmd("touch $ARGV[0].started");

# Make sure NodeZ jobs are getting submitted.
sleep(20);

print "Condor_rm'ing parent DAGMan ($ARGV[1])\n";

my @array = ();
runCondorTool("condor_rm $ARGV[1]",\@array,2,{emit_output=>1});

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
runcmd("touch $ARGV[0].finished");
print "$ARGV[0] finished\n";

exit(0);
