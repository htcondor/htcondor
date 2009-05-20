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

CondorTest::debug("Submit file for this test is $cmd\n",1);
#print "looking at env for condor config\n";

$testdesc =  'Basic Parallel - Parallel U';
$testname = "cmd_chirp_get-set_par";

$aborted = sub {
	my %info = @_;
	my $done;
	CondorTest::debug("Abort event not expected!\n",1);
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("Held event not expected.....\n",1);
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::RegisterTimed($testname, $timed, 600);
	CondorTest::debug("Parallel job executed\n",1);
};

$timed = sub
{
	die "Test took too long!!!!!!!!!!!!!!!\n";
};

$success = sub
{
	CondorTest::debug("Success: Parallel Test ok\n",1);
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

