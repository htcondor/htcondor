#! /usr/bin/env perl
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
## ON_EXIT_HOLD - True
## The job runs, finishes, and should be put on hold
##
use CondorTest;

$cmd = 'job_core_onexithold-true_local.cmd';
$testdesc =  'Condor submit for ON_EXIT_HOLD test - local U';
$testname = "job_core_onexithold_local";

##
## After our job is held of awhile, we will want to 
## remove it from the queue. When this flag is set to true
## we know that it was our own script that called for the abort,
## and should not be handled as an error
##
my $aborting;

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
	CondorTest::debug("Good - Job $cluster.$job began execution.\n",1);
};

my $on_evictedwithoutcheckpoint = sub {
	CondorTest::debug("Evicted Without Checkpoint from removing jobs.\n",1);
};

##
## aborted
## The job is being aborted, so we need to make sure that
## we are the one doing the abort
##
$aborted = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	##
	## Make sure this was meant to happen
	## 
	if ( $aborting ) {
		CondorTest::debug("Good - Job $cluster.$job is being removed after being held.\n",1);
		CondorTest::debug("Policy Test Completed\n",1);
	##
	## Bad mojo!
	##
	} else {
		CondorTest::debug("Bad - Job $cluster.$job received an unexpected abort event.\n",1);
		exit(1);
	}
};

##
## held
## The job was put on hold after executing, so we just need to
## remove it
##
$held = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	CondorTest::debug("Good - Job $cluster.$job went on hold after executing.\n",1);

	##
	## Remove the job
	## We set the aborting flag so that we know the abort message 
	## isn't a mistake
	##
	$aborting = 1;
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if ( !$status ) {
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		return(1)
	}
};

##
## success 
## The job finished execution but didn't go on hold
##
$success = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	CondorTest::debug("Bad - Job $job.$cluster finished execution but didn't go on hold.\n",1);
	exit(1);
};

CondorTest::RegisterEvictedWithoutCheckpoint($testname, $on_evictedwithoutcheckpoint);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

