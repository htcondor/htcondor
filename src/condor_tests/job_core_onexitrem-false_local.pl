#!/usr/bin/env perl
##
## ON_EXIT_REMOVE - False
## The parameter will evaluate to false so this test makes sure
## that the job gets requeued
##
use CondorTest;

$cmd = 'job_core_onexitrem-false_local.cmd';
$testname = 'Condor submit policy test for ON_EXIT_REMOVE - local U';

## 
## Status Values
##
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

##
## abnormal
## Not sure how we would end up here, and based on the old
## report message, I'm assuming this is a bad thing
##
$abnormal = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	print "Bad - Job $cluster.$job reported an abnormal event.\n";
	exit(1);
};

##
## aborted
## We should get an aborted callback for the job when we remove
## it in from the queue in the evicted method
##
$aborted = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	print "Good - Job $cluster.$job was removed from the queue\n";
	print "Policy Test Completed\n";
};

##
## held
## Not sure how we would get here
##
$held = sub {
	die "We don't hit Hold state for job restarted for not running long enough!\n";
	exit(1);
};

##
## executed
## Just announce that the job started running
##
$executed = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	print "Good - Job $cluster.$job began execution.\n";
};

##
## evicted
## This is the heart of the test. When the job exits, it will
## come back as being evicted with a requeue attribute set to true
##
$evicted = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	print "Good - Job $cluster.$job was requeued after being evicted.\n";

	##
	## Make sure that we remove the job
	##
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	if ( ! CondorTest::runCondorTool($cmd,\@adarray,2) ) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(0)
	}
};

CondorTest::RegisterEvictedWithRequeue($testname, $evicted);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}
