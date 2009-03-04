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

my $cmd = 'job_core_leaveinqueue-false_van.cmd';
my $testdesc =  'Condor submit with policy set to not trigger for leave_in_queue - vanilla U';
my $testname = "job_core_leaveinqueue_van";

my $killedchosen = 0;
my $debuglevel = 2;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub COMPLETED{4};
sub RUNNING{2};

my %testerrors;
my %info;
my $cluster;

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	CondorTest::debug("Good. for leave_in_queue cluster $cluster must run first\n",$debuglevel);
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("Good, job should be done but NOT left in the queue!!!\n",$debuglevel);
	my $status = 1;
	my $delay = 1;
	my $backoffmax = 17; #(1 + 2 + 4 + 8 + 16 = 31 seconds max)
	my $foundit = 0;
	my $cmd = "condor_q $cluster";
	while( $delay < $backoffmax) {
		my @adarray;
		$foundit = 0;
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if(!$status)
		{
			CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",$debuglevel);
			exit(1)
		}
		foreach my $line (@adarray)
		{
			CondorTest::debug("$line\n",$debuglevel);
			if($line =~ /^\s*$cluster\..*$/) {
				CondorTest::debug("$line\n",$debuglevel);
				CondorTest::debug("job should be done but NOT left in the queue!!\n",$debuglevel);
				$foundit = 1;
			}
		}
		if($foundit == 1) {
			sleep $delay;
			$delay = 2 * $delay;
			next;
		} else {
			CondorTest::debug("Job not in queue as expected\n",$debuglevel);
			last;
		}
	}
	if($foundit == 1) {
		print "bad\n";
		die "Job still in the queue after multiple looks\n";
	}
	print "ok\n";
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	CondorTest::debug("submitted: \n",$debuglevel);
	{
		CondorTest::debug("good job $job expected submitted.\n",$debuglevel);
	}
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterSubmit( $testname, $submitted );

print "Leave in queue False - job should not still be in the queue - ";

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",$debuglevel);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

