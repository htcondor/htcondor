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
$testdesc =  'Condor submit policy test for ON_EXIT_REMOVE - local U';
$testname = "job_core_onexitrem_local";

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

##
## success
## The job finished, so we need to make sure that it gets
## removed from the queue
##
$success = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	CondorTest::debug("Good - Job $cluster.$job finished executing and exited.\n",1);
	CondorTest::debug("Policy Test Completed\n",1);
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}
