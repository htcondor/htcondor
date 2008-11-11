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

$cmd = 'job_core_onexithold-true_java.cmd';
$testname = 'Condor submit with hold for periodic remove test - java U';

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

	print "Good, job - $cluster $job - aborted after Hold state reached\n";
};

$held = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	my $fulljob = "$cluster"."."."$job";
	print "Good, good run of job - $fulljob - should be in queue on hold now\n";
	print "Removing $fulljob\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}
	my @nadarray;
	$status = 1;
	$cmd = "condor_reschedule";
	$status = CondorTest::runCondorTool($cmd,\@nadarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}
};

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	print "Good. for on_exit_hold cluster $cluster must run first\n";
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	die "Bad, job should be on hold and then cpondor_rm and not run to completion!!!\n";
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "submitted: \n";
	{
		print "good job $job expected submitted.\n";
	}
};

CondorTest::RegisterExecute($testname, $executed);
#CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

