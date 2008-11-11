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
## PERIODIC_RELEASE - False
## This test makes sure that if we put a job on hold that
## the PERIODIC_RELEASE expression will never evaluate to true
## and let the job be released. We do this by setting the job
## to be initially on hold, then having a timer method check
## after a minute to make sure the job is still on hold
##
use CondorTest;
$mypid = $ARGV[0];

# make this run pid unique so a second run does not tumple on prior log
$corename = 'job_core_perrelease-false_local';
$corenamewithpid = 'job_core_perrelease-false_local' . $mypid;
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
## Status Values
##
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

##
## After our job is held of awhile, we will want to 
## remove it from the queue. When this flag is set to true
## we know that it was our own script that called for the abort,
## and should not be handled as an error
##
my $aborting;

##
## executed
## Our job should never be allowed to begin execution
##
$executed = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Bad - Job $cluster.$job began execution when it should ".
		  "have been on hold.\n";
	exit(1);
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
};

##
## success
## Our job should never run!
##
$success = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Bad - Job $cluster.$job completed its execution when it should ".
		  "have been on hold.\n";
	exit(1);
};

##
## timed
## This is a callback we register to make sure that 
## the job never ran and we can clean ourselves up
## We set the aborting flag to true so that when we catch
## the abort event, we know it was us and not a mistake from
## something else
##
$timed = sub {
	##
	## We have to use info hash from the last event callback, because
	## the timer callback doesn't provide us with it
	## 
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	print "Good - Job $cluster.$job was never released from being held.\n";
	$aborting = 1;

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
};

##
## abort
## The job is being aborted, so we need to make sure that
## we are the one doing the abort
##
$abort = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	##
	## Make sure this was meant to happen
	## 
	if ( $aborting ) {
		print "Good - Job $cluster.$job is being removed after being held.\n";
		print "Policy Test Completed\n";
	##
	## Bad mojo!
	##
	} else {
		print "Bad - Job $cluster.$job received an unexpected abort event.\n";
		exit(1);
	}
};

CondorTest::RegisterSubmit($testname, $submit);
CondorTest::RegisterAbort($testname, $abort);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );

##
## The job is never going to be released, so we have a 
## call back method that should be executed after several
## policy evaluations. If our callback method is executed
## than we know that the expression works as expected
## We will sleep for 1 minute because the interval is 15 seconds
##
CondorTest::RegisterTimed($testname, $timed, 60);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

