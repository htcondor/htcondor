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

$cmd = 'job_core_perhold-true_sched.cmd';
$testdesc =  'Condor submit with for periodic hold test - scheduler U';
$testname = "job_core_perhold_sched";

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

$abnormal = sub {
	my %info = @_;

	die "Want to see only execute, hold and abort events for periodic hold test\n";
};

$aborted = sub {
	my %info = @_;
	my $done;
	CondorTest::debug("Abort event expected from periodic remove after hold event seen\n",1);
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("Held event expected, removing job.....\n",1);
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(1)
	}
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("Periodic Hold should see and execute, followed by a hold and then we remove the job\n",1);
};

$submitted = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("submitted: ok\n",1);
};

my $on_evictedwithoutcheckpoint = sub {
	CondorTest::debug("Evicted Without Checkpoint from removing jobs.\n",1);
};

CondorTest::RegisterEvictedWithoutCheckpoint($testname, $on_evictedwithoutcheckpoint);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

