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
## Job Deferral Test - Remove
##
## This test will make sure that if a job is being
## deferred and we issue a Remove command, that the job
## will be removed from the starter
#########################################################
use CondorTest;
use POSIX;

##
## Universe
## 
my $universe = $ARGV[0];
my $longuniverse = "";

if($universe eq "van") {
	$longuniverse = "vanilla";
} else {
	$longuniverse = $universe;
}


##
## The timer callback method doesn't provide us with this
## information, so we need to store it from the last event
## callback as a global variable
##
my %info;

##
## When the job should actually run
## We can't use CurrentTime because of how the job's requirements
## are set up
##
$deferralTime = time() + 600;

##
## If this is set to true, then we know that we caused the cooresponding command
##
my $REMOVE = "";

##
## Testing Information
##
$testdesc =  "Job Deferral Testing REMOVE - $universe";
$testname = "job_core_time-deferral-remove" . "_$universe";
$base_name = 'job_core_time-deferral-remove';
$base_cmd = $base_name.".cmd";

##
## submitted
## We need to get the info for the job when it is submitted
##
$submitted = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	CondorTest::debug("Good - Job $cluster.$job was submitted!\n",1);
	##
	## This callback is to put our job on hold
	##
	CondorTest::RegisterTimed($testname, $timed, 10);
};	

##
## executed
## The job has begun to execute over on the starter
## This should never happen because the job should always be
## deferred!
##
$executed = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	CondorTest::debug("Bad - Job $cluster.$job started executing! This should never happen!\n",1);
	exit(1);
};

##
## held
## After the job is deferred, the callback will put the job on
## hold. We keep a flag to make sure that this hold was expected
##
$held = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	##
	## Make sure the hold command was ours
	##
	if ( ! $HOLD ) {
		CondorTest::debug("Bad - Job $cluster.$job was put on hold but not by us!\n",1);
		exit(1);
	}
	
	##
	## Is there a way to check to see if the starter actually 
	## exited??
	##
	CondorTest::debug("Good - Job $cluster.$job was put on hold!\n",1);
	
	##
	## Now we need to remove it
	##

};

##
## aborted
##
$aborted = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	##
	## Make sure the remove command was ours
	##
	if ( ! $REMOVE ) {
		CondorTest::debug("Bad - Job $cluster.$job was removed but not by us!\n",1);
		exit(1);
	}
	
	CondorTest::debug("Good - Job $cluster.$job was aborted and removed from the queue.\n",1);
	CondorTest::debug("Policy Test Completed\n",1);
};

##
## timed
## Call out to remove the job
##
$timed = sub {
	##
	## We have to use info hash from the last event callback, because
	## the timer callback doesn't provide us with it
	## 
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	if ( !defined( $info{"cluster"} ) || !defined( $info{"job"} ) ) {
		my $ulog = $info{"log"};
		CondorTest::debug("Haven't seen submit event yet at timeout!\n",1);
		CondorTest::debug("stat and contents of user log $ulog:\n",1);
		CondorTest::debug(`stat $ulog`,1);
		CondorTest::debug(`cat $ulog`,1);
	}

	##
	## Ignore multiple call backs
	##
	if ( ! $REMOVE ) {
		CondorTest::debug("Removing Job $cluster.$job...\n",1);
		
		##
		## Do the deed!
		##
		$REMOVE = 1;
		my @adarray;
		my $status = 1;
		my $cmd = "condor_rm $cluster.$job";
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if ( !$status ) {
			CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
			exit(1);
		}
	}
};

##
## We need to create a new test file based on the universe
## that we were told to test
##
$cmd = $base_name."_".$universe.".cmd";
open( WRITE_FILE, ">$cmd" ) || die( "Can't open '$cmd' for writing!\n" );

##
## Add the universe information closing with the 'Queue' command
##
print WRITE_FILE "Executable   = ./x_time.pl\n";
print WRITE_FILE "Notification = NEVER\n";
print WRITE_FILE "DeferralPrep = 20\n";
print WRITE_FILE "Universe	   = $longuniverse\n";
print WRITE_FILE "Log		   = $base_name"."_$universe.log\n";
print WRITE_FILE "Output	   = $base_name"."_$universe.out\n";
print WRITE_FILE "Error		   = $base_name"."_$universe.err\n";
print WRITE_FILE "DeferralTime = $deferralTime\n";
print WRITE_FILE "Queue\n";
close( WRITE_FILE );

system("cat $cmd");

$mylogfortest = "$base_name"."_$universe.log";
system("rm -f $mylogfortest");

##
## Setup our testing callbacks
##
#Condor::DebugOn();
CondorTest::RegisterSubmit( $testname, $submitted );
CondorTest::RegisterExecute( $testname, $executed );
CondorTest::RegisterAbort( $testname, $aborted );
	
if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

