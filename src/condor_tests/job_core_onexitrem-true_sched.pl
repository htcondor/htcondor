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

$cmd = 'job_core_onexitrem-true_sched.cmd';
$testdesc =  'Condor submit policy test for ON_EXIT_REMOVE - scheduler U';
$testname = "job_core_onexitrem_sched";

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my %args;
my $cluster;

$abnormal = sub {

	CondorTest::debug("Want to see only submit and abort events for periodic remove test\n",1);
	exit(1);
};

$aborted = sub {
	CondorTest::debug("Abort event expected from expected condor_rm used to remove the requeueing job\n",1);
	CondorTest::debug("Policy test worked.\n",1);
};

$held = sub {
	CondorTest::debug("We don't hit Hold state for job restarted for not running long enough!\n",1);
	exit(1);
};

$executed = sub
{
	%args = @_;
	$cluster = $args{"cluster"};
	CondorTest::debug("Good. for on_exit_remove cluster $cluster must run first\n",1);
};

$evicted = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("Good a requeue.....after eviction....\n",1);
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(0)
	}
};

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

