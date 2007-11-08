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

CondorPersonal::DebugOff();
Condor::DebugOff();


my $LogFile = "sdmkdag.log";
print "Build log will be in ---$LogFile---\n";
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!\n";
open(STDERR, ">&STDOUT");
select(STDERR); $| = 1;
select(STDOUT); $| = 1;

my $dir = $ARGV[0];
my $cmd = $ARGV[1];
#$testname = 'Condor submit dag - stork transfer test';
my $testname =  $ARGV[2];
my $timerlength = $ARGV[3];
print "Timer passed in is <<$timerlength>>\n";
foreach my $arg  (@ARGV)
{
	print "$arg ";
}
print "\n";
#$dagman_args = "-v -storklog `condor_config_val LOG`/Stork.user_log";
$dagman_args = "-v ";

chdir("$dir");

my $loc = getcwd();
print "Currently in $loc\n";

$timed = sub
{
	my $left = $submitcount - $donecount;
	system("date");
	print "Expected break-out!!!!!\n";
	exit(1);
};

$executed = sub
{
		if($timerlength ne "0")
		{
			# set a timer on a dag run
			CondorTest::RegisterTimed($testname, $timed, $timerlength);
		}
        print "Good. We need the dag to run\n";
};

$submitted = sub
{
        print "submitted: This test will see submit, executing and successful completion\n";
};

$success = sub
{
        print "executed successfully\n";
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunDagTest($testname, $cmd, 0, $dagman_args) ) {
        print "$testname: SUCCESS\n";
        exit(0);
} else {
        die "$testname: CondorTest::RunTest() failed\n";
}


