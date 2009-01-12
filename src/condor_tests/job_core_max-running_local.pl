#!/usr/bin/env perl
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

#########################################################
## Max Local Jobs
##
## We are testing to make sure that if the user puts
## in a requirement to limit how many local universe jobs will
## be started, the schedd will be able to block them accordingly
##
#########################################################
use CondorTest;

$testdesc =  'Max Running Local Universe Jobs';
$testname = "job_core_max-running_local";
$cmdfile		= 'job_core_max-running_local.cmd';
$basefile		= 'job_core_max-running_local.out.';

##
## Sleep time 
## This needs to match the arguements value that is in the .cmd file
##
my $SLEEP_TIME = 20; # seconds
	
##
## Last Run Time
## When a job runs, we record the last run time
## All the jobs should only run one at a time
##
my $LAST_RUN_TIME = 0;
	
##
## If the runs succeed, but when we analyze the jobs' runtimes
## and see that they are running when they shouldn't, we will store
## the error message in here
##
my $testFailure = "";

##
## Each job will write to its own file, so we need to just check
## to the time that they said they completed is greater than or equal
## to the time of the last job
##
$successCallback = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	##
	## This is the time that they said they ran on the execute machine
	##
	my $outfile = "$basefile$cluster.$job";
	open(FILE, $outfile) || die("Failed to open output file '$outfile'");
	my @output = <FILE>;
	close(FILE);
	my $reportTime = "@output";
	CondorTest::fullchomp($reportTime);
	
	CondorTest::debug("JOB:         $cluster.$job\n".
		  "OUTFILE:     $outfile\n".
		  "REPORT TIME: $reportTime\n".
	      "LAST TIME:   $LAST_RUN_TIME\n".
	      "DIFFERENCE:  ".( $LAST_RUN_TIME ? ($reportTime - $LAST_RUN_TIME) :
	      									 "SKIP" )."\n\n",1);
	
	##
	## If there wasn't a last run time value, then this is the first
	## job submitted. Otherwise we to check to make sure that they 
	## ran far enough apart from each other
	##
	## SLEEP_TIME - 1 to avoid rounding errors

	if ( $LAST_RUN_TIME ) {
		if ( ( $reportTime - $LAST_RUN_TIME ) < ($SLEEP_TIME - 1) ) {
			$testFailure = "More than one local job ran concurrently!";
			return ( 0 );
		}
	}
	$LAST_RUN_TIME = $reportTime;
	return ( 1 );
};
	
	
##
## Run ye olde' test
##
my $success = 1;
CondorTest::RegisterExitedSuccess($testname, $successCallback );
if (CondorTest::RunTest( $testname, $cmdfile, 0)) {	
	##
	## Check if there was a failure
	##
	if ( $testFailure ) {
		CondorTest::debug("$testname: FAILED - $testFailure\n",1);
		$success = 0;
	}
##
## For some reason it failed to run!
##
} else {
	CondorTest::debug("$testname: CondorTest::RunTest() failed to run\n",1);
	$success = 0;
}	

##
## Print success message
##
if ( $success ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
}
	
## 
## Return an exit status based on what happened with all our tests!
##
exit( $success ? 0 : -1 );
	
