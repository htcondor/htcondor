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

Condor::DebugOff();

$cmd = $ARGV[0];
$jobcount = $ARGV[1];
$neginterval = $ARGV[2];

$neginterval = ($neginterval/2);

print "Interval to wait is <<$neginterval>>\n";
$heldcount = 0;

print "Submit file for this test is $cmd\n";
#print "looking at env for condor config\n";
#system("printenv | grep CONDOR_CONFIG");

$testname = 'Basic Startd MultiMatch - Grid U';

my @adarray;
my $status = 1;
my $runcondorcmd = "condor_advertise UPDATE_STARTD_AD job_startd_multimatch.ad";
$status = CondorTest::runCondorTool($runcondorcmd,\@adarray,2);
if(!$status)
{
	print "Test failure due to Condor Tool Failure<$runcondorcmd>\n";
	exit(1)
}

$aborted = sub {
	my %info = @_;
	my $done;
	print "Abort event not expected!\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	$heldcount = $heldcount + 1;
	print "Held event expected..(count = $heldcount)...\n";
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	print "Grid job executed\n";
};

$timed = sub
{
	system("date");
	print "Held job count test.\n";
	if( $jobcount == $heldcount) {
		print "multimatch working\n";
		print "$testname: SUCCESS\n";
		    exit(0);
	} else {
		print "multimatch not working wanted $jobcount got $heldcount\n";
		print "$testname: FAILURE\n";
		exit(1);
	}
};

$success = sub
{
	print "Success: Grid Test ok\n";
};

CondorTest::RegisterHold($testname, $held);
CondorTest::RegisterTimed($testname, $timed, $neginterval);
CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

