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

$cmd = 'job_core_perrelease-false_java.cmd';
$testname = 'Condor submit policy test for periodic_release - java U';

my %info;
my $cluster;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	print "Good. for periodic_release cluster $cluster must run first\n";
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Good, job should complete trivially\n";
};

$timed = sub
{
	my %info = @_;
	# use submit cluster as timed info not assured
	#my $cluster = $info{"cluster"};

	print "Cluster $cluster alarm wakeup\n";
	print "wakey wakey!!!!\n";
	print "good\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}
	sleep 5;
};

$submit = sub
{
	my %info = @_;
	$cluster = $info{"cluster"};
	my $qstat = CondorTest::getJobStatus($cluster);
	while($qstat == -1)
	{
		print "Job status unknown - wait a bit\n";
		sleep 2;
		$qstat = CondorTest::getJobStatus($cluster);
	}

	print "It better be on hold... status is $status(5 is correct)";
	if($qstat != HELD)
	{
		die "Cluster $cluster failed to go on hold\n";
	}


	print "Cluster $cluster submitted\n";
};

$abort = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Cluster $cluster aborted after hold state verified\n";
};

CondorTest::RegisterSubmit($testname, $submit);
CondorTest::RegisterAbort($testname, $abort);
CondorTest::RegisterExecute($testname, $executed);
#CondorTest::RegisterTimed($testname, $timed, 3600);
CondorTest::RegisterExitedSuccess( $testname, $success );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

