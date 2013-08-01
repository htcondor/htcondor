#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

##
## ON_EXIT_REMOVE - False
## The parameter will evaluate to false so this test makes sure
## that the job gets requeued
##
use CondorTest;

$cmd = 'job_core_onexitrem-false_local.cmd';
$testdesc =  'Condor submit policy test for ON_EXIT_REMOVE - local U';
$testname = "job_core_onexitrem_local";
$removed = 0;

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
	CondorTest::debug("Bad - Job $cluster.$job reported an abnormal event.\n",1);
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
	CondorTest::debug("Good - Job $cluster.$job was removed from the queue\n",1);
	CondorTest::debug("Policy Test Completed\n",1);
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
	CondorTest::debug("Good - Job $cluster.$job began execution.\n",1);
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
	if($removed != 0) {
		return(0); # been here done this - don't try two job removals
	} else {
		$removed = 1;
	}

	CondorTest::debug("Good - Job $cluster.$job was requeued after being evicted.\n",1);

	##
	## Make sure that we remove the job
	##
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	if ( ! CondorTest::runCondorTool($cmd,\@adarray,2) ) {
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(0)
	}
};

my $on_evictedwithoutcheckpoint = sub {
	CondorTest::debug("Evicted Without Checkpoint from removing jobs.\n",1);
};
CondorTest::RegisterEvictedWithoutCheckpoint($testname, $on_evictedwithoutcheckpoint);
CondorTest::RegisterEvictedWithRequeue($testname, $evicted);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}
