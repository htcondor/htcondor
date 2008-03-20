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

##
## PERIODIC_RELEASE - True
## We submit a job that goes on hold right away, then check to
## see that it gets released before beginning execution. To make
## sure that job actually gets released (because otherwise we would
## be waiting forever), we have a timer to catch ourselves if
## the job is still on hold
##
use CondorTest;
$mypid = $ARGV[0];

# make this run pid unique so a second run does not tumple on prior log
$corename = 'job_core_perrelease-true_local';
$corenamewithpid = 'job_core_perrelease-true_local' . $mypid;
$template = $corename . '.template';
$cmd = $corenamewithpid . '.cmd';
$testname = 'Condor submit policy test for PERIODIC_RELEASE - local U';

#create command file

my $line = "";
open(TEMPLATE,"<$template") || die "Can not open template<$template>:$!";
open(CMD,">$cmd") || die "Can not create submit file<$cmd>:$!\n";
while(<TEMPLATE>) {
	$line = $_;
	if($line =~ /^log\s+=\s+.*$/) {
		print CMD "log = $corenamewithpid.log\n";
	} elsif($line =~ /^output\s+=\s+.*$/) {
		print CMD "output = $corenamewithpid.out\n";
	} elsif($line =~ /^error\s+=\s+.*$/) {
		print CMD "error = $corenamewithpid.err\n";
	} else {
		print CMD "$line";
	}
}
close(TEMPLATE);
close(CMD);

##
## The timer callback method doesn't provide us with this
## information, so we need to store it from the last event
## callback as a global variable
##
my %info;

##
## We need to make sure that our job went on hold first 
## before it was allowed to begin executing
##
my $gotHold;

##
## Status Values
##
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

##
## executed
## The job has begun execution, make sure that it went on hold first
##
$executed = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	if ( $gotHold ) {
		print "Good - Job $cluster.$job and began execution ".
			  "after being held.\n";
	##
	## We never were on hold!
	##
	} else {
		print "Bad - Job $cluster.$job began execution without ever ".
			  "being put on hold.\n";
		exit(1);
	}
};

##
## success
## The job finished execution
##
$success = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Good - Job $cluster.$job finished execution.\n";
	print "Policy Test Completed\n";
};

##
## submit
## After the job is submitted we need to make sure it went on hold
##
$submit = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	##
	## Get the job status
	##
	my $qstat = CondorTest::getJobStatus($cluster);
	my $sleepTime = 2;
	while ( $qstat == -1 ) {
		print "Job status unknown - checking in $sleepTime seconds...\n";
		sleep($sleepTime);
		$qstat = CondorTest::getJobStatus($cluster);
	} # WHILE

	##
	## Check to make sure that it's on hold
	## 
	if ( $qstat == HELD) {
		print "Good - Job $cluster.$job went on hold as soon as it ".
			  "was submitted.\n";
		$gotHold = 1;
	##
	## The job didn't go on hold, so we need to abort
	##
	} else {
		print "Bad - Job $cluster.$job failed to go on hold.\n";
		exit(1);
	}

	CondorTest::setJobAd($cluster, foo, true, bool);
};

##
## timed
## If this ever gets called than our job never got 
## released, which is a bad thing!
##
$timed = sub {
	##
	## We have to use info hash from the last event callback, because
	## the timer callback doesn't provide us with it
	## 
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	print "Bad - Job $cluster.$job hit our timer!.\n";

	##
	## Go ahead and remove the job from the queue now
	##
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if (!$status) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		return(1)
	}
	exit(1);
};

##
## release
## Our job got released, which is what we wanted. So we'll 
## let it run and make sure it doesn't go on hold or anything
## incorrect
##
$release = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	##
	## This is probably not necessary, but just make sure that our 
	## job was put on hold before it was released
	##
	if ( $gotHold ){
		print "Good - Job $cluster.$job was released after being put on hold.\n";
	} else {
		print "Bad - Job $cluster.$job received a release event without ever ".
			  "being put on hold.\n";
		exit(1);
	}
	
	##
	## Now set the job to run
	##
	my @adarray;
	my $status = 1;
	#$ENV{_CONDOR_TOOL_DEBUG}="D_ALL";
	my $cmd = "_CONDOR_TOOL_DEBUG=D_ALL condor_reschedule -d";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2, 1, 1);
	if (!$status) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1);
	}

	print "Stderr with debug info for condor_reschedule follows\n";
	system("cat runCTool$$");
};

CondorTest::RegisterSubmit($testname, $submit);
CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterRelease( $testname, $release );

##
## This callback is to see if our job never got released
## The callback time should be greater than the sleep time
## of the job and the periodic interval. 60 seconds should
## suffice for now
##
CondorTest::RegisterTimed($testname, $timed, 90);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

