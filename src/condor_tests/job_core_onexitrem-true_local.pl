#!/usr/bin/env perl
##
## ON_EXIT_REMOVE - True
## The parameter will evaluate to true so this test makes sure
## that the job gets removed from the queue after it finishes executing
## It would be nice if we could incorporate the true/false tests into
## a single test, that is have ON_EXIT_REMOVE evaluate to false a couple 
## times, make sure the job gets requeued, then have it evaluate to true
## and make sure it cleans itself up. But alas, CondorTest is unable
## to handle a job getting executed twice (from what I can tell)
##
use CondorTest;

$cmd = 'job_core_onexitrem-true_local.cmd';
$testname = 'Condor submit policy test for ON_EXIT_REMOVE - local U';

##
## Status Values
##
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

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
## The job finished, so we need to make sure that it gets
## removed from the queue
##
$success = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	print "Good - Job $cluster.$job finished executing and exited.\n";
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
