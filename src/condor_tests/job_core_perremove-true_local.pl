#!/usr/bin/env perl
##
## PERIODIC_REMOVE - True
## We are checking to see that when PERIODIC_REMOVE evaluates to
## true that the job gets aborted and removed from the queue.
##
use CondorTest;

$cmd = 'job_core_perremove-true_local.cmd';
$testname = 'Condor submit policy test for PERIODIC_REMOVE - local U';

##
## aborted
## The job was aborted just we wanted
##
$aborted = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Good - Job $cluster.$job was aborted and removed from the queue.\n";
	print "Policy Test Completed\n";
};

##
## executed
## Just announce that the job began execution
##
$executed = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Good - Job $cluster.$job began execution.\n";
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterAbort( $testname, $aborted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

