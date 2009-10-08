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
use CondorPersonal;
use Cwd;



my $LogFile = "sdmkdag.log";
CondorTest::debug("Build log will be in ---$LogFile---\n",1);
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!\n";
open(STDERR, ">&STDOUT");
select(STDERR); $| = 1;
select(STDOUT); $| = 1;

my $dir = $ARGV[0];
my $cmd = $ARGV[1];
#$testdesc =  'Condor submit dag - stork transfer test';
#$testname = "x_runstorkdag";
my $testdesc =   $ARGV[2];
my $testname = "x_runstorkdag";
my $timerlength = $ARGV[3];
CondorTest::debug("Timer passed in is <<$timerlength>>\n",1);
foreach my $arg  (@ARGV)
{
	CondorTest::debug("$arg ",1);
}
CondorTest::debug("\n",1);
#$dagman_args = "-v -storklog `condor_config_val LOG`/Stork.user_log";
$dagman_args = "-v ";

chdir("$dir");

my $loc = getcwd();
CondorTest::debug("Currently in $loc\n",1);

$timed = sub
{
	my $left = $submitcount - $donecount;
	print scalar localtime() . "\n";
	CondorTest::debug("Expected break-out!!!!!\n",1);
	exit(1);
};

$executed = sub
{
		if($timerlength ne "0")
		{
			# set a timer on a dag run
			CondorTest::RegisterTimed($testname, $timed, $timerlength);
		}
        CondorTest::debug("Good. We need the dag to run\n",1);
};

$submitted = sub
{
        CondorTest::debug("submitted: This test will see submit, executing and successful completion\n",1);
};

$success = sub
{
        CondorTest::debug("executed successfully\n",1);
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunDagTest($testname, $cmd, 0, $dagman_args) ) {
        CondorTest::debug("$testname: SUCCESS\n",1);
        exit(0);
} else {
        die "$testname: CondorTest::RunTest() failed\n";
}


