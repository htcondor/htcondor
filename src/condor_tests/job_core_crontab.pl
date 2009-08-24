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
## CronTab Tests
## We submit a job that is set to run 4 times every 2
## minutes. We keep track of various information about the
## job so that we know the following:
##	1) The jobs are submitted with the proper preptime
##	2) The jobs are only running every 2 minutes
##	3) The NumJobStarts properly reflects how many times we ran
## 	4) The job runs when it was suppose to
##
## When a job finishes but gets rescheduled, we will get
## an eviction notice. If the job ever goes on hold,
## then something went wrong and the test fails
#########################################################
use CondorTest;
use POSIX;
use strict;
use warnings;

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
## Test Information
##
my $testdesc =  "CronTab Testing - $universe";
my $base_name = 'job_core_crontab';
my $base_cmd = $base_name.".cmd";
my $testname = "";
my $debuglevel = 2;

if( defined $ARGV[0] ) {
	if($ARGV[0] =~ /.*van.*/ ) {
		$testname = "job_core_crontab_van";
	} elsif($ARGV[0] =~ /.*sched.*/ ) {
		$testname = "job_core_crontab_sched";
	} elsif($ARGV[0] =~ /.*loc.*/ ) {
		$testname = "job_core_crontab_local";
	} else {
		die "Unexpected universe passed to job_core_crontab.pl($ARGV[0])\n";
	}
} else {
	$testname = "job_core_crontab_van";
}

# log file is related to the universe...
my $logname = $base_name . "_" .  $universe . ".log";
system("rm -f $logname");

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
my $DEFERRAL_TIME = 0;
my $LAST_DEFERRAL_TIME = 0;

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
my $submitted = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	CondorTest::debug("Good - Job $cluster.$job was submitted!\n",$debuglevel);
};	

##
## executed
## The job has begun to execute over on the starter
## We keep a counter to know how many times that it started
##
my $executed = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	##
	## When a job with a Cron schedule begins execution it will
	## have a DeferralTime in its job ad. We need to get it out and save
	## it here because by the time we get the eviction notice the schedd
	## might have overwritten it
	##
	my @result;
	#my $q_cmd = "_CONDOR_TOOL_DEBUG=D_ALL condor_q $cluster.$job -debug -format %d DeferralTime\n";
	my $q_cmd = "_CONDOR_TOOL_DEBUG=D_ALL condor_q $cluster.$job -format %d DeferralTime\n";
	if ( ! CondorTest::runCondorTool($q_cmd, \@result, 3) ) {
		CondorTest::debug("ERROR: Unable to get DeferralTime due to Condor Tool Failure<$q_cmd>\n",$debuglevel);
	    exit(1);
	}

	$DEFERRAL_TIME = "@result";	
	print "DEFERRAL_TIME = <$DEFERRAL_TIME>\n";
	##
	## We're screwed if we can't get out the time
	##
	if ( $DEFERRAL_TIME < 0 || !$DEFERRAL_TIME ) {
		CondorTest::debug("Bad - Unable to get DeferralTime for Job $cluster.$job!\n",$debuglevel);
		exit(1);
	}
	
	
	##
	## Make sure that we haven't run more then we were suppose to
	##
	if ( ++$run_ctr > $max_run_ctr ) {
		CondorTest::debug("Bad - Job $cluster.$job has been executed $run_ctr times!\n",$debuglevel);
		my @result;
		CondorTest::runCondorTool("condor_rm $cluster.$job", \@result, 3);
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
		CondorTest::debug("Good - Job $cluster.$job began execution for the ".
			  "$run_ctr$suffix time.\n",$debuglevel);
	}
};

##
## evicted
##
my $evicted = sub {
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
		CondorTest::debug("Bad - Job $cluster.$job was requeued but we don't have a ".
			  "DeferralTime from when it started executing!\n",$debuglevel);
		exit(1);
	}
	
	CondorTest::debug("\tEVICTION_TIME: $evictionTime\n",$debuglevel);
	CondorTest::debug("\tDEFERRAL_TIME: $DEFERRAL_TIME\n",$debuglevel);
	
	##
	## If the job didn't execute for long enough, that means something
	## is wrong with the deferral time
	##
	my $sleep = $evictionTime - $DEFERRAL_TIME;
	if ( $sleep < ($SLEEP_TIME - 2)) {
		CondorTest::debug("Bad - Job $cluster.$job only ran for $sleep seconds. ".
			  "Should have been $SLEEP_TIME seconds!\n",$debuglevel);
		exit(1);
	}
	##
	## I am all over these tests like stink on a squirrel!
	## Now we keep track of the last time that we executed to make
	## sure that we are really being scheduled at proper intervals of time
	##
	if ( $LAST_DEFERRAL_TIME > 0 ) {
		CondorTest::debug("\tLAST_DEFFERAL_TIME: $LAST_DEFERRAL_TIME\n",$debuglevel);
		##
		## The difference between the two times is not exact.
		## This is bad mojo
		## 
		my $diff = $DEFERRAL_TIME - $LAST_DEFERRAL_TIME;
		CondorTest::debug("\tDIFFERENCE: $diff\n",$debuglevel);
		if ( $diff < $DEFERRAL_DIFFERENCE ) {
			CondorTest::debug("Bad - Job $cluster.$job was scheduled to run $diff seconds after ".
				  "the last execution. It should be $DEFERRAL_DIFFERENCE seconds!\n",$debuglevel);
			exit(1);
		}	
	}
	$LAST_DEFERRAL_TIME = $DEFERRAL_TIME;
	
	CondorTest::debug("Good - Job $cluster.$job was requeued after being evicted.\n",$debuglevel);
};

##
## success 
## The job finished execution
##
my $success = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	##
	## One final check to make sure that the NumJobStarts is 
	## being updated properly. We have to look into the history because
	## at this point the job is no longer in the queue
	##
	## 06/12/2007
	## Getting the JobRunCount has proven to be unreliable for some reason
	## So we're going to 
	##
	my $try = 3;
	my $h_runcount = -1;
	while ($try-- > 0 && $h_runcount == -1) {
		my @result;
		my $historyfile = `condor_config_val history`;
		CondorTest::fullchomp($historyfile);
		print "WARNING: workaround for bug condor wiki #683 \n";
		# There should never be a job finished and out of the queue
		# and not already in the EXISTING history file.
		while(!(-f $historyfile)) {
			# wait for history file to exist
			sleep(1);
		}
		my $h_cmd = "condor_history -l $cluster.$job";
		if ( ! CondorTest::runCondorTool($h_cmd, \@result, 3) ) {
			CondorTest::debug("ERROR: Unable to get JobRunCount due to Condor Tool Failure<$h_cmd>\n",$debuglevel);
			exit(1);
		}
		my $temp = "@result";
		if ( $temp =~ /NumJobStarts = (\d+)/ ) {
			$h_runcount = $1;
		}
		if ( $h_runcount < 0 ) { 
			CondorTest::debug("$h_cmd\n",$debuglevel);
			CondorTest::debug("+-----------------------------------------------+\n",$debuglevel);
			CondorTest::debug("$temp\n",$debuglevel);
			CondorTest::debug("+-----------------------------------------------+\n\n",$debuglevel);
			
			##
			## Check if the job is still in the queue?
			##
			$h_cmd = "condor_q";
			if ( ! CondorTest::runCondorTool($h_cmd, \@result, 3) ) {
				CondorTest::debug("ERROR: Unable get job queue information<$h_cmd>\n",$debuglevel);
				exit(1);
			}
			CondorTest::debug("$h_cmd\n",$debuglevel);
			CondorTest::debug("+-----------------------------------------------+\n",$debuglevel);
			CondorTest::debug("@result\n",$debuglevel);
			CondorTest::debug("+-----------------------------------------------+\n",$debuglevel);
			sleep(10);
		}
	} # WHILE
	if ($h_runcount == -1) { 
		CondorTest::debug("Bad - Unable to get JobRunCount for Job $cluster.$job!\n",$debuglevel);
		exit(1);
	}
	
	##
	## Make sure what we have and what the job's ad has is the same
	##
	if ( $run_ctr != $h_runcount ) {
		CondorTest::debug("Bad - Job $cluster.$job finished execution but its NumJobStarts ".
			  "is $h_runcount. It should be $run_ctr!\n",$debuglevel);
		exit(1);
	}
			
	CondorTest::debug("Good - Job $cluster.$job finished execution.\n",$debuglevel);
	CondorTest::debug("Policy Test Completed\n",$debuglevel);
};

##
## held
## If the job was held then that means we missed our DeferralTime
## that was calculated by the schedd. This is bad.
##
my $held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	CondorTest::debug("Bad - Job $cluster.$job went on hold.\n",$debuglevel);
	exit(1);
};

##
## We need to create a new test file based on the universe
## that we were told to test
##
my $cmd = $base_name."_".$universe.".cmd";
open( READ_FILE, "<$base_cmd" ) || die( "Can't open '$base_cmd' for reading!\n" );
open( WRITE_FILE, ">$cmd" ) || die( "Can't open '$cmd' for writing!\n" );
while( <READ_FILE> ) {
	print WRITE_FILE $_;
} # WHILE
close( READ_FILE );

##
## Add the universe information closing with the 'Queue' command
##
print WRITE_FILE "Universe	= $longuniverse\n";
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
	CondorTest::debug("$testname: SUCCESS\n",$debuglevel);
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
    my $month;
    my $day;
    my $hour;
    my $minute;
    my $second;
	my $timestamp;
    CondorTest::debug("Searching $log for latest eviction event\n", $debuglevel);
    open(FILE, "<$log") || die "Can't open log file '$log'\n";
    while (<FILE>) {
    	##
    	## Pull out the date and time of when the job was evicted
    	## from the execute machine. We always want the last time 
    	## that may be in the logfile
    	##
    	if ( $_ =~ /^004\s+\(.*\)\s+(\d{2})\/(\d{2})\s+(\d{2}):(\d{2}):(\d{2})/ ) {
            CondorTest::debug("Found eviction event: $_", $debuglevel);
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
	(undef, undef, undef, undef, undef, my $year, undef, undef, my $isdst) = localtime(time); 
	$timestamp = mktime($second, $minute, $hour, $day, $month - 1, $year, 0, 0, $isdst);
	
    CondorTest::debug("Eviction date: $month/$day/".($year + 1900)." $hour:$minute:$second\n", $debuglevel);
    CondorTest::debug("Eviction timestamp: $timestamp\n", $debuglevel);
    return ($timestamp);
};

