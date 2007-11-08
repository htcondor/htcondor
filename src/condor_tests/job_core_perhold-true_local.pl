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
## PERIODIC_HOLD - True
## We submit a job and it should be put on hold
## When the job comes back and is on hold, we will remove it and
## make sure that the job is removed properly. I'm not sure if I
## should even bother doing this, but I was following what was 
## in the original code for the vanilla universe
##
use CondorTest;

$cmd = 'job_core_perhold-true_local.cmd';
$testname = 'Condor submit with for PERIODIC_HOLD test - local U';

## 
## Status Values
##
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

##
## When this is set to true, that means our job was 
## put on hold first
##
my $gotHold;

##
## abnormal
## Not sure how we would end up here, and based on the old
## report message, I'm assuming this is a bad thing
##
$abnormal = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	print "Bad - Job $cluster.$job reported an abnormal event.\n";
	exit(1);
};

##
## aborted
## We should see this event when we remove our job after
## it gets put on hold. The held method below will set a flag
## to let us know hat the abort was expected. Otherwise, we 
## failed for some other reason
##
$aborted = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	if ( $gotHold ) {
		print "Good - Job $cluster.$job was aborted after being put on hold.\n";
		print "Policy Test Completed\n";
	} else {
		print "Bad - Job $cluster.$job was aborted before it was put on hold.\n";
		exit(1);
	}
};

##
## held
## The job was put on hold, which is what we wanted
## After the job is put on hold we will want to remove it from
## the queue. We will set the gotHold flag to let us know that
## the hold event was caught. Thus, if we get two hold events
## that's bad mojo
##
$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	##
	## Make sure we weren't here earlier
	##
	if ( $gotHold ) {
		print "Bad - Job $cluster.$job was put on hold twice.\n";
		exit(1);
	}

	print "Good - Job $cluster.$job was put on hold.\n";

	##
	## Remove the job from the queue
	##
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if ( !$status ) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		return(1)
	}

	$gotHold = 1;
};

##
## executed
## Just announce that the job began execution
##
$executed = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Good - Job $cluster.$job began execution.\n";
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

