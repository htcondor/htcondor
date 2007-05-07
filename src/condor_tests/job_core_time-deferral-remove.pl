#!/usr/bin/env perl
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
my $universe = ($ARGV[0] ? $ARGV[0] : "vanilla");

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
$testname = "Job Deferral Testing REMOVE - $universe";
$base_name = 'job_core_time-deferral-tests';
$base_cmd = $base_name.".cmd";

##
## submitted
## We need to get the info for the job when it is submitted
##
$submitted = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	print "Good - Job $cluster.$job was submitted!\n";
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
	
	print "Bad - Job $cluster.$job started executing! This should never happen!\n";
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
		print "Bad - Job $cluster.$job was put on hold but not by us!\n";
		exit(1);
	}
	
	##
	## Is there a way to check to see if the starter actually 
	## exited??
	##
	print "Good - Job $cluster.$job was put on hold!\n";
	
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
		print "Bad - Job $cluster.$job was removed but not by us!\n";
		exit(1);
	}
	
	print "Good - Job $cluster.$job was aborted and removed from the queue.\n";
	print "Policy Test Completed\n";
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

	##
	## Ignore multiple call backs
	##
	if ( ! $REMOVE ) {
		print "Removing Job $cluster.$job...\n";
		
		##
		## Do the deed!
		##
		$REMOVE = 1;
		my @adarray;
		my $status = 1;
		my $cmd = "condor_rm $cluster.$job";
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if ( !$status ) {
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1);
		}
	}
};

##
## We need to create a new test file based on the universe
## that we were told to test
##
$cmd = $base_name."_".$universe.".cmd";
open( READ_FILE, "<$base_cmd" ) || die( "Can't open '$base_cmd' for reading!\n" );
open( WRITE_FILE, ">$cmd" ) || die( "Can't open '$cmd' for writing!\n" );
while( <READ_FILE> ) {
	print WRITE_FILE $_;
} # WHILE
close( READ_FILE );

##
## Add the universe information closing with the 'Queue' command
##
print WRITE_FILE "Universe	   = $universe\n";
print WRITE_FILE "Log		   = $base_name"."_$universe.log\n";
print WRITE_FILE "Output	   = $base_name"."_$universe.out\n";
print WRITE_FILE "Error		   = $base_name"."_$universe.err\n";
print WRITE_FILE "DeferralTime = $deferralTime\n";
print WRITE_FILE "Queue\n";
close( WRITE_FILE );

##
## Setup our testing callbacks
##
#Condor::DebugOn();
CondorTest::RegisterSubmit( $testname, $submitted );
CondorTest::RegisterExecute( $testname, $executed );
CondorTest::RegisterAbort( $testname, $aborted );
##
## This callback is to put our job on hold
##
CondorTest::RegisterTimed($testname, $timed, 30);
	
if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

