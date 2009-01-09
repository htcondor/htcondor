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

$cmd = 'job_flocking_to.cmd';
$out = 'job_flocking_to.out';
$testdesc =  'flocking ';
$testname = "job_flocking_to";

$submitted = sub
{
	my %info = @_;
	my $name = $info{"error"};
	CondorTest::debug("Flocking job submitted\n",1);
};

$aborted = sub 
{
	my %info = @_;
	my $done;
	die "Abort event NOT expected\n";
};

$timed = sub
{
	die "Job should have ended by now. Flock To error!\n";
};

$execute = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $name = $info{"error"};
	#CondorTest::RegisterTimed($testname, $timed, 600);
};

$ExitSuccess = sub {
	my %info = @_;
	CondorTest::debug("Flocking job completed without error\n",1);
};



CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
CondorTest::RegisterExecute($testname, $execute);
CondorTest::RegisterSubmit( $testname, $submitted );


if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

