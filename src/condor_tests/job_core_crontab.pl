#!/usr/bin/env perl
#########################################################
## CronTab Tests
## We submit a job that is set to run 4 times every 2
## minutes. We keep track of various information about the
## job so that we know the following:
##	1) The jobs are submitted with the proper preptime
##	2) The jobs are only running every 2 minutes
##	3) The JobRunCount properly reflects how many times we ran
## 	4) The job runs when it was suppose to
##
## When a job finishes but gets rescheduled, we will get
## an eviction notice. If the job ever goes on hold,
## then something went wrong and the test fails
#########################################################
use CondorTest;
use POSIX;

##
## Universe
## 
my $universe = ($ARGV[0] ? $ARGV[0] : "vanilla");

##
## Test Information
##
$testname = "CronTab Testing - $universe";
$base_name = 'job_core_crontab';
$base_cmd = $base_name.".cmd";

##
## Counter for the number of times our job ran
##
my $run_ctr = 0;

##
## The MAX number of times our job should run
##
my $max_run_ctr = 2;

##
## The sleep time of the job
##
my $SLEEP_TIME = 20;

##
## The preptime that the job gets to be sent over to the starter
##
my $PREP_TIME = 90;

##
## When a job begins execution, the schedd should have calculated
## the time that the job should actually start running over on the
## Starter
##
my $DEFERRAL_TIME;
my $LAST_DEFERRAL_TIME;

##
## This is the difference in seconds that one job should be scheduled
## for after the previous. If this fails then we know something
## is possibly with the 
## CronMinute = */3
##
my $DEFERRAL_DIFFERENCE = 180; # seconds

##
## submitted
## A nice message to let us know our job was submitted
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
## We keep a counter to know how many times that it started
##
$executed = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	##
	## When a job with a Cron schedule begins execution it will
	## have a DeferralTime in its job ad. We need to get it out and save
	## it here because by the time we get the eviction notice the schedd
	## might have overwritten it
	##
	my @result;
	my $q_cmd = "condor_q $cluster.$job -format %d DeferralTime";
	if ( ! CondorTest::runCondorTool($q_cmd, \@result, 3) ) {
		print "ERROR: Unable to get DeferralTime due to Condor Tool Failure<$q_cmd>\n";
	    exit(1);
	}
	$DEFERRAL_TIME = "@result";	
	##
	## We're screwed if we can't get out the time
	##
	if ( $DEFERRAL_TIME < 0 || !$DEFERRAL_TIME ) {
		print "Bad - Unable to get DeferralTime for Job $cluster.$job!\n";
		exit(1);
	}
	
	
	##
	## Make sure that we haven't run more then we were suppose to
	##
	if ( ++$run_ctr > $max_run_ctr ) {
		print "Bad - Job $cluster.$job has been executed $run_ctr times!\n";
		exit(1);
	##
	## It's good that we ran, so print a message 
	##
	} else {
		##
		## I'm a stickler for nice messages
		##
		my $suffix = "th";
		if ( $run_ctr == 1 ) {
			$suffix = "st";
		} elsif ( $run_ctr == 2 ) {
			$suffix = "nd";
		} elsif ( $run_ctr == 3 ) {
			$suffix = "rd";
		}
		print "Good - Job $cluster.$job began execution for the ".
			  "$run_ctr$suffix time.\n";
	}
};

##
## evicted
##
$evicted = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	##
	## Get the time that they finished execution
	##
	my $evictionTime = extractEvictionTime( $info{'log'} );
		
	##
	## Make sure that the DeferralTime minus the finished execution time
	## is about the same as the sleep time
	## We can't use Condor's reported execution time because there is 
	## no way for us to differeniate between the deferred time and the
	## actual time spent executing
	##
	if ( $DEFERRAL_TIME <= 0 ) {
		print "Bad - Job $cluster.$job was requeued but we don't have a ".
			  "DeferralTime from when it started executing!\n";
		exit(1);
	}
	
	print "\tEVICTION_TIME: $evictionTime\n";
	print "\tDEFERRAL_TIME: $DEFERRAL_TIME\n";
	
	##
	## If the job didn't execute for long enough, that means something
	## is wrong with the deferral time
	##
	my $sleep = $evictionTime - $DEFERRAL_TIME;
	if ( $sleep < $SLEEP_TIME ) {
		print "Bad - Job $cluster.$job only ran for $sleep seconds. ".
			  "Should have been $SLEEP_TIME seconds!\n";
		exit(1);
	}
	##
	## I am all over these tests like stink on a squirrel!
	## Now we keep track of the last time that we executed to make
	## sure that we are really being scheduled at proper intervals of time
	##
	if ( $LAST_DEFERRAL_TIME > 0 ) {
		print "\tLAST_DEFFERAL_TIME: $LAST_DEFERRAL_TIME\n";
		##
		## The difference between the two times is not exact.
		## This is bad mojo
		## 
		my $diff = $DEFERRAL_TIME - $LAST_DEFERRAL_TIME;
		print "\tDIFFERENCE: $diff\n";
		if ( $diff < $DEFERRAL_DIFFERENCE ) {
			print "Bad - Job $cluster.$job was scheduled to run $diff seconds after ".
				  "the last execution. It should be $DEFERRAL_DIFFERENCE seconds!\n";
			exit(1);
		}	
	}
	$LAST_DEFERRAL_TIME = $DEFERRAL_TIME;
	
	print "Good - Job $cluster.$job was requeued after being evicted.\n";
};

##
## success 
## The job finished execution
##
$success = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	
	##
	## One final check to make sure that the JobRunCount is 
	## being updated properly. We have to look into the history because
	## at this point the job is no longer in the queue
	##
	my @result;
	my $h_cmd = "condor_history -l $cluster.$job";
	if ( ! CondorTest::runCondorTool($h_cmd, \@result, 3) ) {
		print "ERROR: Unable to get JobRunCount due to Condor Tool Failure<$h_cmd>\n";
	    exit(1);
	}
	my $temp = "@result";
	my $h_runcount = -1;
	if ( $temp =~ /JobRunCount = (\d+)/ ) {
		$h_runcount = $1;
	}
	if ( $h_runcount < 0 ) { 
		print "Bad - Unable to get JobRunCount for Job $cluster.$job!\n";
		exit(1);
	}
	##
	## Make sure what we have and what the job's ad has is the same
	##
	if ( $run_ctr != $h_runcount ) {
		print "Bad - Job $cluster.$job finished execution but it's JobRunCount ".
			  "is $h_runcount. It should be $run_ctr!\n";
		exit(1);
	}
			
	print "Good - Job $cluster.$job finished execution.\n";
	print "Policy Test Completed\n";
};

##
## held
## If the job was held then that means we missed our DeferralTime
## that was calculated by the schedd. This is bad.
##
$held = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	print "Bad - Job $cluster.$job went on hold.\n";
	exit(1);
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
print WRITE_FILE "Universe	= $universe\n";
print WRITE_FILE "Log		= $base_name"."_$universe.log\n";
print WRITE_FILE "Output	= $base_name"."_$universe.out\n";
print WRITE_FILE "Error		= $base_name"."_$universe.err\n";
print WRITE_FILE "Queue\n";
close( WRITE_FILE );

##
## Setup our testing callbacks
##
CondorTest::RegisterSubmit( $testname, $submitted );
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterEvictedWithRequeue($testname, $evicted);
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

## -----------------------------------------------------
## HELPER FUNCTIONS
## -----------------------------------------------------

##
## extractEvictionTime()
## For a given log file, get the last eviction time
##
sub extractEvictionTime {
    my ($log) = @_;
    open(FILE, "<$log") || die "Can't open log file '$log'\n";
    while (<FILE>) {
    	##
    	## Pull out the date and time of when the job was actually 
    	## executed by the starter. We always want the last time 
    	## that may be in the logfile
    	##
    	if ( $_ =~ /^004\s+\(.*\)\s+(\d{2})\/(\d{2})\s+(\d{2}):(\d{2}):(\d{2})/ ) {
    		$month  = $1;
    		$day    = $2;
    		$hour   = $3;
    		$minute = $4;
    		$second = $5;
    	}
    } # WHILE
    close(FILE);
    
    ##
    ## Convert what we got back into a timestamp
    ## The current log file does not include the year of when a job
    ## was executed so we will use the current year
	##
	(undef, undef, undef, undef, undef, $year, undef, undef, $isdst) = localtime(time); 
	$timestamp = mktime($second, $minute, $hour, $day, $month - 1, $year, 0, 0, $isdst);
	
    #print "$month/$day/".($_year + 1900)." $hour:$minute:$second\n";
    #print "RUN TIME: $timestamp\n";
    return ($timestamp);
};

