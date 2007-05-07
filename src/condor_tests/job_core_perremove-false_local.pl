#!/usr/bin/env perl
##
## PERIODIC_REMOVE - False
## We are checking to see that when PERIODIC_REMOVE evaluates to
## false our job will finish running to completion and not evicted
##
use CondorTest;

$cmd = 'job_core_perremove-false_local.cmd';
$testname = 'Condor submit policy test for PERIODIC_REMOVE - local U';

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

##
## success
## Our job was able to finish
##
$success = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	print "Good - Job $cluster.$job finished successfully.\n";
	print "Policy Test Completed\n";
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

