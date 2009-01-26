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

$cmd = 'job_core_onexithold-false_van.cmd';
$testdesc =  'Condor submit with hold for periodic remove test - vanilla U';
$testname = "job_core_onexithold_van";

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my %testerrors;
my %info;
my $cluster;

$abnormal = sub {
	my %info = @_;

	die "Want to see only submit and abort events for periodic remove test\n";
};

$aborted = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	if( $job eq "000" )
	{
		CondorTest::debug("Good, job - $job - aborted after Hold state reached\n",1);
	}
	elsif( $job eq "001" )
	{
		CondorTest::debug("Bad, job $job should not be aborted EVER!\n",1);
		$testerrors{$job} = "job $job should not be aborted EVER!";
	}
	else
	{
		die "ABORT: ONLY 2 jobs expected - job $job - !!!!!!!!!!!!\n";
	}
};

$held = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	if( $job eq 0 )
	{
		my $fulljob = "$cluster"."."."$job";
		CondorTest::debug("Good, good run of job - $fulljob - should be in queue on hold now\n",1);
		CondorTest::debug("Removing $fulljob\n",1);
		my @adarray;
		my $status = 1;
		my $cmd = "condor_rm $cluster";
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if(!$status)
		{
			CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
			exit(1)
		}
		my @nadarray;
		$status = 1;
		$cmd = "condor_reschedule";
		$status = CondorTest::runCondorTool($cmd,\@nadarray,2);
		if(!$status)
		{
			CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
			exit(1)
		}
	}
	elsif( $job eq 1 )
	{
		CondorTest::debug("Bad, job $job should NOT be on hold!!!\n",1);
	}
	else
	{
		die "HOLD: ONLY 2 jobs expected - job $job - !!!!!!!!!!!!\n";
	}
};

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	CondorTest::debug("Good. for on_exit_hold cluster $cluster must run first\n",1);
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	CondorTest::debug("Good, good job - $job - should complete trivially\n",1);
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	CondorTest::debug("submitted: \n",1);
	{
		CondorTest::debug("good job $job expected submitted.\n",1);
	}
};

CondorTest::RegisterExecute($testname, $executed);
#CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
#CondorTest::RegisterAbort( $testname, $aborted );
#CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

