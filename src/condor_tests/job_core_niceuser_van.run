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

my $cmd = 'job_core_niceuser_van.cmd';
my $datafile = 'job_core_niceuser_van.data';
my $testdesc =  'condor_submit output works - vanilla U';
my $testname = "job_core_niceuser_van";

my @sdata =
(
"1",
"8",
"3",
"10",
"5",
"2"
);

my @vdata =
(
"1",
"2",
);

my @skiplines = ();


my $doonlyonce = "";
my $count = 2;


$ExitSuccess = sub 
{
	my %info = @_;

	print "Job Completed.....\n";
	$count = $count - 1;

	print "Count now $count\n";
	if( $count != 0 )
	{
		return(0);
	}

	CondorTest::CompareText( $datafile, \@vdata, @skiplines );
};

system("rm -rf job_core_niceuser_van.data");
system("touch job_core_niceuser_van.data");

CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

