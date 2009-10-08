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


$cmd = $ARGV[0];
$jobcount = $ARGV[1];
$neginterval = $ARGV[2];

$neginterval = ($neginterval/2);

CondorTest::debug("Interval to wait is <<$neginterval>>\n",1);
$heldcount = 0;

CondorTest::debug("Submit file for this test is $cmd\n",1);

$testdesc =  'Basic Startd MultiMatch - Grid U';
$testname = "job_startd_multimatch";

my @adarray;
my $status = 1;
my $runcondorcmd = "condor_advertise UPDATE_STARTD_AD job_startd_multimatch.ad";
$status = CondorTest::runCondorTool($runcondorcmd,\@adarray,2);
if(!$status)
{
	CondorTest::debug("Test failure due to Condor Tool Failure<$runcondorcmd>\n",1);
	exit(1)
}

$aborted = sub {
	my %info = @_;
	my $done;
	CondorTest::debug("Abort event not expected!\n",1);
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	$heldcount = $heldcount + 1;
	CondorTest::debug("Held event expected..(count = $heldcount)...\n",1);
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("Grid job executed\n",1);
};

$timed = sub
{
	print scalar localtime() . "\n";
	CondorTest::debug("Held job count test.\n",1);
	if( $jobcount == $heldcount) {
		CondorTest::debug("multimatch working\n",1);
		CondorTest::debug("$testname: SUCCESS\n",1);
		    exit(0);
	} else {
		CondorTest::debug("multimatch not working wanted $jobcount got $heldcount\n",1);
		CondorTest::debug("$testname: FAILURE\n",1);
		exit(1);
	}
};

$success = sub
{
	CondorTest::debug("Success: Grid Test ok\n",1);
};

CondorTest::RegisterHold($testname, $held);
CondorTest::RegisterTimed($testname, $timed, $neginterval);
CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

