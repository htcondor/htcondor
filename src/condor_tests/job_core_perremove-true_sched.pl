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

use CondorTest;

$cmd = 'job_core_perremove-true_sched.cmd';
$testdesc =  'Condor submit policy test for PERIODIC_REMOVE - scheduler U';
$testname = "job_core_perremove_sched";

my $killedchosen = 0;

my %args;
my $cluster;

$aborted = sub {
	CondorTest::debug("Abort event expected from periodic_remove policy evaluating to true\n",1);
	CondorTest::debug("Policy test worked.\n",1);
};

$executed = sub
{
	%args = @_;
	$cluster = $args{"cluster"};
	CondorTest::debug("Good. for on_exit_remove cluster $cluster must run first\n",1);
};

my $on_evictedwithoutcheckpoint = sub {
	CondorTest::debug("Evicted Without Checkpoint from removing jobs.\n",1);
};

CondorTest::RegisterEvictedWithoutCheckpoint($testname, $on_evictedwithoutcheckpoint);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterAbort( $testname, $aborted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

