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

package CondorTest;

use strict;
use warnings;

use Carp;
use POSIX;
use POSIX qw/strftime/;
use Net::Domain qw(hostfqdn);
use Cwd;
use Time::Local;
use File::Basename;
use IO::Handle;
use FileHandle;

use Condor;
use CondorUtils;
use CondorPersonal;

use base 'Exporter';

our @EXPORT = qw(runCondorTool runToolNTimes RegisterResult is_windows);

my %securityoptions =
(
"NEVER" => "1",
"OPTIONAL" => "1",
"PREFERRED" => "1",
"REQUIRED" => "1",
);

# Tracking Running Tests
my $RunningFile = "RunningTests";
my $teststrt = 0;
my $teststop = 0;
my $DEBUGLEVEL = 1;
my $debuglevel = 2;

my $UseNewRunning = 0;

my $MAX_CHECKPOINTS = 2;
my $MAX_VACATES = 3;
my $MAX_CORES = 10;

my @skipped_output_lines;
my @expected_output;

my $checkpoints;
my $hoststring;
my $submit_file; #RunTest and RunDagTest set.
my $vacates;
my %test;
my %machine_ads;
my $lastconfig;
my $handle; #actually the test name.
my $BaseDir = getcwd();
my $isnightly = IsThisNightly($BaseDir);

#set inital test debug level
DebugLevel(1);

# we want to process and track the collection of cores
my $coredir = "$BaseDir/Cores";
if(!(-d $coredir)) {
	TestDebug("Creating collection directory for cores\n",2);
#	runcmd("mkdir -p $coredir");
#	the above command doesn't work on windows (-p isn't valid for windows' mkdir which always has -p behavior)
#	but since cwd is $BaseDir we can just use perl's mkdir
        mkdir 'Cores' || die "ERROR: could not create $coredir\n";
}

# set up for reading in core/ERROR exemptions
my $errexempts = "ErrorExemptions";
my %exemptions;
my $failed_coreERROR = "";

my %personal_condors = ();
my $CondorTestPid = 0;
my $CleanedUp = 0;

my $test_failure_count = 0;
my $test_success_count = 0;


BEGIN
{
    # disable command buffering so output is flushed immediately
    STDOUT->autoflush();
    STDERR->autoflush();

    # Attempt to clean up personal condors when killed.
    # Unfortunately, this doesn't currently clean up personal condors
    # that are in the middle of being started.
    $SIG{'INT'} = 'CondorTest::Abort';
    $SIG{'TERM'} = 'CondorTest::Abort';

    $MAX_CHECKPOINTS = 2;
    $MAX_VACATES = 3;
	$MAX_CORES = 2;
    $checkpoints = 0;
	$hoststring = "notset:000";
    $vacates = 0;
	$lastconfig = "";
	$DEBUGLEVEL = 1;
	$CondorTestPid = $$;
}

END
{
    # Store the old exit status so we can be sure to return it in the end
    # as calls to `backticks` or system() can change it.
    my $old_exit_status = $?;

    # When a forked child exits, do not do CondorTest cleanup.
    # Only do the cleanup from the main process.
    return if $CondorTestPid != $$;

    if ( !Cleanup() ) {
	# Set exit status to non-zero
	$? = 1;
    } else {
	$? = $old_exit_status;
    }
}

sub Abort()
{
    print "\nReceived kill signal in PID $$ (CondorTestPid = $CondorTestPid).\n";

    exit(1) if $CondorTestPid != $$;

    Cleanup();

    exit(1);
}

sub Cleanup()
{
    if( $CleanedUp ) {
	# Avoid doing this twice (e.g. once in EndTest and once in END).
	return 1;
    }
    $CleanedUp = 1;

    KillAllPersonalCondors();
    if($failed_coreERROR ne "") {

	print "\nTest being marked as FAILED from discovery of core file or ERROR in logs\n";
	print "Results indicate: $failed_coreERROR\n";
	print "Time, Log, message are stored in condor_tests/Cores/core_error_trace\n\n";
	print "************************************\n";
	system("head -15 Cores/core_error_trace");
	print "************************************\n";
	print "\n\n";
	return 0;
    }
    return 1;
}

# This function never returns.
# It should be the last thing that a test program does,
# (but be aware that some older tests do not call it).
# All personal condors are shut down and the final exit status
# of the test is determined.
sub EndTest
{
    my $extra_notes = "";

    my $exit_status = 0;
    if( Cleanup() == 0 ) {
	$exit_status = 1;
    }
    if($failed_coreERROR ne "") {
	$exit_status = 1;
	$extra_notes = "$extra_notes\n  Log Directory Check results: $failed_coreERROR\n";
    }

    if( $test_failure_count > 0 ) {
	$exit_status = 1;
    }

    if( $test_failure_count == 0 && $test_success_count == 0 ) {
	$extra_notes = "$extra_notes\n  CondorTest::RegisterResult() was never called!";
	$exit_status = 1;
    }

    my $result_str = $exit_status == 0 ? "SUCCESS" : "FAILURE";

    my $testname = GetDefaultTestName();

    TestDebug( "\n\nFinal status for $testname: $result_str\n  $test_success_count check(s) passed\n  $test_failure_count check(s) failed$extra_notes\n", 1 );

    exit($exit_status);
}

# This should be called in each check function to register the pass/fail result
sub RegisterResult
{
    my $result = shift;
    my %args = @_;

    my ($caller_package, $caller_file, $caller_line) = caller();
    my $checkname = GetCheckName($caller_file,%args);

    my $testname = $args{test_name} || GetDefaultTestName();
	#print "RegisterResult: test name: $testname\n";

    my $result_str = $result == 1 ? "PASSED" : "FAILED";
    TestDebug( "\n$result_str check $checkname in test $testname\n\n", 1 );
    if( $result != 1 ) {
	$test_failure_count += 1;
    }
    else {
	$test_success_count += 1;
    }
}

sub GetDefaultTestName
{
    return basename( $0, ".run" );
}

sub GetCheckName
{
    my $filename = shift; # module file containing the check
    my %args = @_;        # named arguments to the check function

    if( exists $args{check_name} ) {
	return $args{check_name};
    }

    my $check_name = basename( $filename, ".pm" );

    my $arg_str = "";
    for my $name ( keys %args ) {
        my $value = $args{$name};
		if( $arg_str ne "" ) {
	    	$arg_str = $arg_str . ",";
		}
		$arg_str = $arg_str . "$name=$value";
    }
    if( $arg_str ne "" ) {
	$check_name = $check_name . "($arg_str)";
    }
    return $check_name;
}

# return a file name that did not exist at the time this function was called
sub TempFileName
{
    my $base = shift;

    $base = $base . ".$$";
    my $fname = $base;
    my $num = 0;
    while( -e $fname ) {
	$num = $num + 1;
	$fname = $base . ".$num";
    }
    return $fname;
}

sub Reset
{
    %machine_ads = ();
	Condor::Reset();
	$hoststring = "notset:000";
}

sub SetExpected
{
	my $expected_ref = shift;
	foreach my $line (@{$expected_ref}) {
		TestDebug( "expected: $line\n", 2);
	}
	@expected_output = @{$expected_ref};
}

sub SetSkipped
{
	my $skipped_ref = shift;
	foreach my $line (@{$skipped_ref}) {
		TestDebug( "skip: $line\n", 2);
	}
	@skipped_output_lines = @{$skipped_ref};
}

sub ForceVacate
{
    my %info = @_;

    return 0 if ( $checkpoints >= $MAX_CHECKPOINTS ||
		  $vacates >= $MAX_VACATES );

    # let the job run for a few seconds and then send vacate signal
    sleep 5;
    Condor::Vacate( "\"$info{'sinful'}\"" );
    $vacates++;
    return 1;
}

sub RegisterSubmit
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "submit: missing function reference argument";

    $test{$handle}{"RegisterSubmit"} = $function_ref;
}
sub RegisterExecute
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "execute: missing function reference argument";

    $test{$handle}{"RegisterExecute"} = $function_ref;
}
#	Use a specific type of eviction only, plain one is gone
#
#sub RegisterEvicted
#{
    #my $handle = shift || croak "missing handle argument";
    #my $function_ref = shift || croak "evict: missing function reference argument";
#
    #$test{$handle}{"RegisterEvicted"} = $function_ref;
#}
sub RegisterEvictedWithCheckpoint
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithCheckpoint"} = $function_ref;
}
sub RegisterEvictedWithRequeue
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithRequeue"} = $function_ref;
}
sub RegisterEvictedWithoutCheckpoint
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithoutCheckpoint"} = $function_ref;
}
sub RegisterExited
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExited"} = $function_ref;
}
sub RegisterExitedSuccess
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "exit success: missing function reference argument";

    $test{$handle}{"RegisterExitedSuccess"} = $function_ref;
}
sub RegisterExitedFailure
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExitedFailure"} = $function_ref;
}
sub RegisterExitedAbnormal
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExitedAbnormal"} = $function_ref;
}
sub RegisterAbort
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterAbort"} = $function_ref;
}
sub RegisterShadow
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterShadow"} = $function_ref;
}
sub RegisterImageUpdated
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterImageUpdated"} = $function_ref;
}
sub RegisterWantError
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterWantError"} = $function_ref;
}
sub RegisterHold
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterHold"} = $function_ref;
}
sub RegisterRelease
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterRelease"} = $function_ref;
}
sub RegisterSuspended
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterSuspended"} = $function_ref;
}
sub RegisterUnsuspended
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterUnsuspended"} = $function_ref;
}
sub RegisterDisconnected
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterDisconnected"} = $function_ref;
}
sub RegisterReconnected
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterReconnected"} = $function_ref;
}
sub RegisterReconnectFailed
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterReconnectFailed"} = $function_ref;
}
sub RegisterJobErr
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterJobErr"} = $function_ref;
}
sub RegisterULog
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterULog"} = $function_ref;
}

sub RegisterTimed
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";
	my $alarm = shift || croak "missing wait time argument";

    $test{$handle}{"RegisterTimed"} = $function_ref;
    $test{$handle}{"RegisterTimedWait"} = $alarm;

	# relook at registration and re-register to allow a timer
	# to be set after we are running. 
	# Prior to this change timed callbacks were only regsitered
	# when we call "runTest" and similar calls at the start.

	CheckTimedRegistrations($handle);
}

sub RemoveTimed
{
    my $handle = shift || croak "missing handle argument";

    $test{$handle}{"RegisterTimed"} = undef;
    $test{$handle}{"RegisterTimedWait"} = undef;
    TestDebug( "Remove timer.......\n",4);
    Condor::RemoveTimed( );
}

sub DefaultOutputTest
{
    my %info = @_;

    croak "default_output_test called but no \@expected_output defined"
	unless $#expected_output >= 0;

    TestDebug( "\$info{'output'} = $info{'output'}\n" ,4);

	my $output = "";
	my $error = "";
	my $initialdir = $info{'initialdir'};
	if((defined $initialdir) && ($initialdir ne "")) {
		TestDebug( "Testing with initialdir = $initialdir\n" ,4);
		$output = $initialdir . "/" . $info{'output'};
		$error = $initialdir . "/" . $info{'error'};
		TestDebug( "$output is now being tested.....\n" ,4);
	} else {
		$output = $info{'output'};
		$error = $info{'error'};
	}

    CompareText( $output, \@expected_output, @skipped_output_lines )
	|| die "$handle: FAILURE (STDOUT doesn't match expected output)\n";

    IsFileEmpty( $error ) || 
	die "$handle: FAILURE (STDERR contains data)\n";
}

# You might consider RunTest2 instead.
sub RunTest
{
	DoTest(@_);
}

# Submit a Condor job.
#
# Replacement for RunTest.
#
# Takes a hash of arguments:
#
# name => Name of the test. MANDATORY
# submit_file => submit file to pass to condor_submit
# submit_body => RunTest2 will automatically create a submit file
#                filled with the contents of this and use that
#                as the submit file.
# submit_hash => RunTest2 will automatically create a submit file
#                based on the contents of this hash reference.
#                See below for details
# want_checkpoint => If true (1), you want a checkpoint.
#
# One and only one of submit_file, submit_body, or submit_hash is MANDATORY.
#
# submit_hash builds a submit file from options.  It has a set of defaults
# that can be overwritten.  An empty hash is valid, and just uses the defaults.
#
# The defaults are:
#   universe   => "vanilla",
#   _executable => sleep
#   log => $name.log
#   output => $name.out
#   error => $name.err
#   Notification => NEVER
#   arguments  => 0
#   _queue => 1
#
# Options without an underscore are directly passed into the submit
# file.  You can passed in +Arguments this way ("+test" => '"Value"').
# Options beginning with _ are special.
#
# _executable - An automatically created executable.  If "executable" 
#               is also specified, this is ignored.  Choose from this list:
#               sleep - Sleeps for $ARGV[0] seconds, or forever if 0
# _queue - passed to the queue command as a count.
sub RunTest2
{
	my(%args) = @_;

	my(%TEST_SCRIPTS) = (
		'sleep' => <<END,
#! /usr/bin/env perl
my \$request = \$ARGV[0];
if(\$request eq "0") {
        while(1) { sleep 1 };
} else {
        sleep \$request;
}
exit(0);
END

	);

	my $name = $args{name};

	if(not defined $name) {
		croak "CondorTest::RunTest2 requires a 'name' argument";
	}
	my(@required) = qw(submit_body submit_hash submit_file);
	my $num_submit_options = 0;
	foreach my $r (@required) {
		if(exists $args{$r}) { $num_submit_options++; }
	}
	if($num_submit_options != 1) {
		croak "CondorTest::RunTest2 requires one and only one of the options @required";
	}

	my $submit_file = $args{submit_file};
	my $submit_body = $args{submit_body};
	if(defined $args{submit_hash}) {
		my(%default) = (
			universe   => "vanilla",
			_executable => 'sleep',
			'log' => "$name.log",
			output => "$name.out",
			error => "$name.err",
			Notification => 'NEVER',
			arguments  => '0',
			_queue => '1',
		);
		my %choices = (%default, %{$args{submit_hash}});
		if(exists $choices{'executable'}) {
			delete $choices{'_executable'};
		} else {
			my $exe = $choices{'_executable'};
			if(not exists $TEST_SCRIPTS{$exe}) {
				croak "CondorTest::RunTest2 called with unknown _executable of '$exe'";
			}
			WriteFileOrDie('executable', $TEST_SCRIPTS{$exe});
			$choices{'executable'} = 'executable';
			my $queue = $choices{'_queue'};
			delete $choices{'_queue'};
			$submit_body = '';
			foreach my $k (sort keys %choices) {
				$submit_body .= "$k = $choices{$k}\n";
			}
			$submit_body .= "queue $queue\n";
		}
	}
	if(defined $submit_body) {
		$submit_file = "$name.cmd";
		print STDERR "Writing body to $submit_file\n";
		WriteFileOrDie($submit_file, $submit_body);
	}

	my $want_checkpoint = $args{want_checkpoint};
	if(not defined $want_checkpoint) {
		print STDERR "CondorTest::RunTest2: want_checkpoint not specified. Assuming no checkpoint is wanted.\n";
		$want_checkpoint = 0;
	}

	my $undead = undef;
	return DoTest($name, $submit_file, $want_checkpoint, $undead, $args{dagman_args});
}
 
###################################################################################
##
## We want to pass a call back function to secure the cluster id after DoTest
## submits the job. We were expecting 3 defined values and one possible. We are adding
## another possible which will have stack location alllowing an undefined then
## followed by a defined on for dag args. We want this to work for dag tests too
## so we will expand the 4 dag args to 5 with an undef at poision 3 , the fourth element
##
###################################################################################

sub RunDagTest
{
	my $undead = undef;
	my $count = 0;
	$count = @_;
	if($count == 5) {
		DoTest(@_);
	} else {
		my @newrgs = ($_[0],$_[1],$_[2],$undead,$_[3]);
		DoTest(@newrgs);
	}
}

sub DoTest
{
    $handle              = shift || croak "missing handle argument";
    $submit_file      = shift ;
    my $wants_checkpoint = shift;
	my $clusterIDcallback = shift;
	my $dagman_args = 	shift;

    my $status           = -1;
	my $monitorpid = 0;
	my $waitpid = 0;
	my $monitorret = 0;
	my $retval = 0;

	# Many of our tests want to use RegisterResult and EndTest
	# but don't actually rely on RunTest to do the work. So
	# I am enabling a mode where we can call RunTest just to register
	# the test.
	
	if(!(defined $submit_file)) {
    	Condor::SetHandle($handle);
		print "No submit file passedin. Only registering test\n";
		return($retval);
	}
	

	$failed_coreERROR = "";
	if( !(defined $wants_checkpoint)) {
		die "DoTest must get at least 3 args!!!!!\n";
	}

	TestDebug("RunTest says test: $handle\n",2);;
	# moved the reset to preserve callback registrations which includes
	# an error callback at submit time..... Had to change timing
	CondorTest::Reset();

    croak "too many arguments" if shift;

    # this is kludgey :: needed to happen sooner for an error message callback in runcommand
    Condor::SetHandle($handle);

    # if we want a checkpoint, register a function to force a vacate
    # and register a function to check to make sure it happens
	if( $wants_checkpoint )
	{
		Condor::RegisterExecute( \&ForceVacate );
		#Condor::RegisterEvictedWithCheckpoint( sub { $checkpoints++ } );
	} else {
		if(defined $test{$handle}{"RegisterExecute"}) {
			Condor::RegisterExecute($test{$handle}{"RegisterExecute"});
		}
	}

	CheckRegistrations();

	if($isnightly == 1) {
		print "\nCurrent date and load follow:\n";
		print scalar localtime() . "\n";
		if(CondorUtils::is_windows() == 0) {
			runcmd("uptime");
		}
		print "\n\n";
	}

	my $wrap_test = $ENV{WRAP_TESTS};

	my $config = "";
	if(defined  $wrap_test) {
		print "Config before PersonalCondorTest: $ENV{CONDOR_CONFIG}\n";
		$lastconfig = $ENV{CONDOR_CONFIG};
		$config = PersonalCondorTest($submit_file, $handle);
		if($config ne "") {
			print "PersonalCondorTest returned this config file: $config\n";
			print "Saving last config file: $lastconfig\n";
			$ENV{CONDOR_CONFIG} = $config;
			print "CONDOR_CONFIG now: $ENV{CONDOR_CONFIG}\n";
			runcmd("condor_config_val -config");
		}
	}

	AddRunningTest($handle);

    # submit the job and get the cluster id
	TestDebug( "Now submitting test job\n",4);
	my $cluster = 0;

	$teststrt = time();;
    # submit the job and get the cluster id
	if(!(defined $dagman_args)) {
		#print "Regular Test....\n";
    	$cluster = Condor::TestSubmit( $submit_file );
	} else {
		#print "Dagman Test....\n";
    	$cluster = Condor::TestSubmitDagman( $submit_file, $dagman_args );
	}
    
	if(defined $clusterIDcallback) {
		&$clusterIDcallback($cluster);
	}

    # if condor_submit failed for some reason return an error
    if($cluster == 0){
		print "Why is cluster 0 in RunTest??????\n";
	} else {

    	# monitor the cluster and return its exit status
		# note 1/2/09 bt
		# any exits cause monitor to never return allowing us
		# to kill personal condor wrapping the test :-(
# July 26, 2013
#
# Major architecture change as we used more callbacks for testing
# I stumbled on value in the test which were to be changed by the callbacks were not being
# changed. What was happening in DoTest was that we would fork and the child was
# running monitor and that callbacks were changing the variables in the child's copy.
# The process that is the test calling DoTest was simply waiting for the child to die
# so it could clean up. Now The test switches to be doing the monitoring and the 
# callbacks switch it back up to the test code for a bit. The monitor and the 
# test had always been lock-steped anyways so getting rid of the child
# has had little change except that call backs can be fully functional now.
		
    	$monitorret = Condor::Monitor();
		if(  $monitorret == 1 ) {
			TestDebug( "Monitor happy to exit 0\n",4);
		} else {
			TestDebug( "Monitor not happy to exit 1\n",4);
		}
		$retval = $monitorret;
# 		$monitorpid = fork();
# 		if($monitorpid == 0) {
# 			# child does monitor
#     		$monitorret = Condor::Monitor();

# 			TestDebug( "Monitor did return on its own status<<<$monitorret>>>\n",4);
#     		die "$handle: FAILURE (job never checkpointed)\n"
# 			if $wants_checkpoint && $checkpoints < 1;

# 			if(  $monitorret == 1 ) {
# 				TestDebug( "child happy to exit 0\n",4);
# 				exit(0);
# 			} else {
# 				TestDebug( "child not happy to exit 1\n",4);
# 				exit(1);
# 			}
# 		} else {
# 			# parent cleans up
# 			$waitpid = waitpid($monitorpid, 0);
# 			if($waitpid == -1) {
# 				TestDebug( "No such process <<$monitorpid>>\n",4);
# 			} else {
# 				$retval = $?;
# 				TestDebug( "Child status was <<$retval>>\n",4);
# 				if( WIFEXITED( $retval ) && WEXITSTATUS( $retval ) == 0 )
# 				{
# 					TestDebug( "Monitor done and status good!\n",4);
# 					$retval = 1;
# 				} else {
# 					$status = WEXITSTATUS( $retval );
# 					TestDebug( "Monitor done and status bad<<$status>>!\n",4);
# 					$retval = 0;
# 				}
# 			}
# 		}
	}

	TestDebug( "************** condor_monitor back ************************ \n",4);

	$teststop = time();
	my $timediff = $teststop - $teststrt;

	if($isnightly == 1) {
		print "Test started <$teststrt> ended <$teststop> taking <$timediff> seconds\n";
		print "Current date and load follow:\n";
		print scalar localtime() . "\n";
		if(CondorUtils::is_windows() == 0) {
			runcmd("uptime");
		}
		print "\n\n";
	}

	##############################################################
	#
	# We ASSUME that each version of each personal condor
	# has its own unique log directory whether we are automatically 
	# wrapping a test at this level OR we have a test which
	# sets up N personal condors like a test like job_condorc_abc_van
	# which sets up 3.
	#
	# Our initial check is to see if we are not running in the
	# outer personal condor because checking there requires
	# more of an idea of begin and end times of the test and
	# some sort of understanding about how many tests are running
	# at once. In the case of more then one, one can not assign
	# fault easily.
	#
	# A wrapped test will be found here. A test running with
	# a personal condor outside of src/condor_tests will show
	# up here. A test submitted as part of a multiple number 
	# of personal condors will show up here. 
	#
	# If we catch calls to "CondorPersonal::KillDaemonPids($config)"
	# and it is used consistently we can check both wrapped
	# tests and tests involved with one or more personal condors
	# as we shut the personal condors down.
	#
	# All the personal condors created by CondorPersonal
	# have a testname.saveme in their path which would
	# show a class of tests to be safe to check even if outside
	# of src/condor_tests even if N tests are running concurrently.
	#
	# If we knew N is one we could do an every test check
	# based on start time and end time of the tests running
	# in any running condor.
	#
	##############################################################
	if(ShouldCheck_coreERROR() == 1){
		TestDebug("Want to Check core and ERROR!!!!!!!!!!!!!!!!!!\n\n",2);
		# running in TestingPersonalCondor
		my $logdir = `condor_config_val log`;
		CondorUtils::fullchomp($logdir);
		$failed_coreERROR = CoreCheck($handle, $logdir, $teststrt, $teststop);
	}
	##############################################################
	#
	# When to do core and ERROR checking thoughts 2/5/9
	#
	##############################################################

	if(defined  $wrap_test) {
		my $logdir = `condor_config_val log`;
		CondorUtils::fullchomp($logdir);
		$failed_coreERROR = CoreCheck($handle, $logdir, $teststrt, $teststop);
		if($config ne "") {
			#print "KillDaemonPids called on this config file: $config\n";
			my $condor = GetPersonalCondorWithConfig($config);
			$condor->SetCondorDirection("down");
			if($UseNewRunning == 1) {
				#print "KillPersonal UseNewRunning set to yes. Calling KillDaemons\n";
				CondorPersonal::KillDaemons($config);
			} else {
				#print "KillPersonal UseNewRunning set to no. Calling KillDaemonPids\n";
				CondorPersonal::KillDaemonPids($config);
			}
		} else {
			print "No config setting to call KillDaemonPids with\n";
		}
		print "Restoring this config: $lastconfig\n";
		$ENV{CONDOR_CONFIG} = $lastconfig;
	} else {
		TestDebug( "Not currently wrapping tests\n",4);
	}

	# done with this test
	RemoveRunningTest($handle);

    if($cluster == 0){
		if( exists $test{$handle}{"RegisterWantError"} ) {
			return(1);
		} else {
			return(0);
		}
	} else {
		# ok we think we want to pass it but how did core and ERROR
		# checking go
		if($failed_coreERROR eq "") {
    		return $retval;
		} else {
			# oops found a problem fail test
			print "\nTest being marked as FAILED from discovery of core file or ERROR in logs\n";
			print "Results indicate: $failed_coreERROR\n";
			print "Time, Log, message are stored in condor_tests/Cores/core_error_trace Sample follows:\n\n";
			print "************************************\n";
			system("head -15 Cores/core_error_trace");
			print "************************************\n";
			print "\n\n";
    		return 0;
		}
	}
}

sub CheckTimedRegistrations
{
	my $handle = shift;
	# this one event should be possible from ANY state
	# that the monitor reports to us. In this case which
	# initiated the change I wished to start a timer from
	# when the actual runtime was reached and not from
	# when we started the test and submited it. This is the time 
	# at all other regsitrations have to be registered by....

    if( exists $test{$handle} and defined $test{$handle}{"RegisterTimed"} )
    {
		TestDebug( "Found a timer to register.......\n",4);
		Condor::RegisterTimed( $test{$handle}{"RegisterTimed"} , $test{$handle}{"RegisterTimedWait"});
    }
}

sub CheckRegistrations
{
    # any handle-associated functions with the cluster
    # or else die with an unexpected event
    if( defined $test{$handle}{"RegisterExitedSuccess"} )
    {
        Condor::RegisterExitSuccess( $test{$handle}{"RegisterExitedSuccess"} );
    }
    else
    {
	Condor::RegisterExitSuccess( sub {
	    die "$handle: FAILURE (got unexpected successful termination)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterExitedFailure"} )
    {
	Condor::RegisterExitFailure( $test{$handle}{"RegisterExitedFailure"} );
    }
    else
    {
	Condor::RegisterExitFailure( sub {
	    my %info = @_;
	    die "$handle: FAILURE (returned $info{'retval'})\n";
	} );
    }

    if( defined $test{$handle}{"RegisterExitedAbnormal"} )
    {
	Condor::RegisterExitAbnormal( $test{$handle}{"RegisterExitedAbnormal"} );
    }
    else
    {
	Condor::RegisterExitAbnormal( sub {
	    my %info = @_;
	    die "$handle: FAILURE (got signal $info{'signal'})\n";
	} );
    }

    if( defined $test{$handle}{"RegisterShadow"} )
    {
	Condor::RegisterShadow( $test{$handle}{"RegisterShadow"} );
    }
    else
    {
	Condor::RegisterShadow( sub {
	    my %info = @_;
	    die "$handle: FAILURE (got unexpected shadow exceptions)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterImageUpdated"} )
    {
	Condor::RegisterImageUpdated( $test{$handle}{"RegisterImageUpdated"} );
    }

    if( defined $test{$handle}{"RegisterWantError"} )
    {
	Condor::RegisterWantError( $test{$handle}{"RegisterWantError"} );
    }

    if( defined $test{$handle}{"RegisterAbort"} )
    {
	Condor::RegisterAbort( $test{$handle}{"RegisterAbort"} );
    }
    else
    {
	Condor::RegisterAbort( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job aborted by user)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterHold"} )
    {
	Condor::RegisterHold( $test{$handle}{"RegisterHold"} );
    }
    else
    {
	Condor::RegisterHold( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job held by user)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterSubmit"} )
    {
	Condor::RegisterSubmit( $test{$handle}{"RegisterSubmit"} );
    }

    if( defined $test{$handle}{"RegisterRelease"} )
    {
	Condor::RegisterRelease( $test{$handle}{"RegisterRelease"} );
    }
    #else
    #{
	#Condor::RegisterRelease( sub {
	    #my %info = @_;
	    #die "$handle: FAILURE (job released by user)\n";
	#} );
    #}

    if( defined $test{$handle}{"RegisterJobErr"} )
    {
	Condor::RegisterJobErr( $test{$handle}{"RegisterJobErr"} );
    }
    else
    {
	Condor::RegisterJobErr( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job error -- see $info{'log'})\n";
	} );
    }

    if( defined $test{$handle}{"RegisterULog"} )
    {
	Condor::RegisterULog( $test{$handle}{"RegisterULog"} );
    }
    else
    {
	Condor::RegisterULog( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job ulog)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterSuspended"} )
    {
	Condor::RegisterSuspended( $test{$handle}{"RegisterSuspended"} );
    }
    else
    {
	Condor::RegisterSuspended( sub {
	    my %info = @_;
	    die "$handle: FAILURE (Suspension not expected)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterUnsuspended"} )
    {
	Condor::RegisterUnsuspended( $test{$handle}{"RegisterUnsuspended"} );
    }
    else
    {
	Condor::RegisterUnsuspended( sub {
	    my %info = @_;
	    die "$handle: FAILURE (Unsuspension not expected)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterDisconnected"} )
    {
	Condor::RegisterDisconnected( $test{$handle}{"RegisterDisconnected"} );
    }
    else
    {
	Condor::RegisterDisconnected( sub {
	    my %info = @_;
	    die "$handle: FAILURE (Disconnect not expected)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterReconnected"} )
    {
	Condor::RegisterReconnected( $test{$handle}{"RegisterReconnected"} );
    }
    else
    {
	Condor::RegisterReconnected( sub {
	    my %info = @_;
	    die "$handle: FAILURE (reconnect not expected)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterReconnectFailed"} )
    {
	Condor::RegisterReconnectFailed( $test{$handle}{"RegisterReconnectFailed"} );
    }
    else
    {
	Condor::RegisterReconnectFailed( sub {
	    my %info = @_;
	    die "$handle: FAILURE (reconnect failed)\n";
	} );
    }

    # if we wanted to know about requeues.....
    if( defined $test{$handle}{"RegisterEvictedWithRequeue"} )
    {
        Condor::RegisterEvictedWithRequeue( $test{$handle}{"RegisterEvictedWithRequeue"} );
    } else { 
		Condor::RegisterEvictedWithRequeue( sub {
	    my %info = @_;
	    die "$handle: FAILURE (Unexpected Eviction with requeue)\n";
	} );
	}

    # if we wanted to know about With Checkpoints .....
    if( defined $test{$handle}{"RegisterEvictedWithCheckpoint"} )
    {
        Condor::RegisterEvictedWithCheckpoint( $test{$handle}{"RegisterEvictedWithCheckpoint"} );
    } else { 
		Condor::RegisterEvictedWithCheckpoint( sub {
	    my %info = @_;
	    die "$handle: FAILURE (Unexpected Eviction with checkpoint)\n";
	} );
	}

    # if we wanted to know about Without checkpoints.....
    if( defined $test{$handle}{"RegisterEvictedWithoutCheckpoint"} )
    {
        Condor::RegisterEvictedWithoutCheckpoint( $test{$handle}{"RegisterEvictedWithoutCheckpoint"} );
    } else { 
		Condor::RegisterEvictedWithoutCheckpoint( sub {
	    my %info = @_;
	    die "$handle: FAILURE (Unexpected Eviction without checkpoint)\n";
	} );
	}

	# Use specific eviction bt 6/18/13
	#if(defined $test{$handle}{"RegisterEvicted"} ) {
		#Condor::RegisterEvicted( $test{$handle}{"RegisterEvicted"} );
	#} else {
		#Condor::RegisterEvicted( sub {
	    #my %info = @_;
	    #die "$handle: FAILURE (Unexpected Eviction type)\n";
	#} );
	#}
    # if evicted, call condor_resched so job runs again quickly
    #if( !defined $test{$handle}{"RegisterEvicted"} )
    #{
        #Condor::RegisterEvicted( sub { sleep 5; Condor::Reschedule } );
    #} else {
	#Condor::RegisterEvicted( $test{$handle}{"RegisterEvicted"} );
    #}

    if( defined $test{$handle}{"RegisterTimed"} )
    {
		Condor::RegisterTimed( $test{$handle}{"RegisterTimed"} , $test{$handle}{"RegisterTimedWait"});
    }
}


sub CompareText
{
    my $file = shift || croak "missing file argument";
    my $aref = shift || croak "missing array reference argument";
    my @skiplines = @_;
    my $linenum = 0;
	my $line;
	my $expectline;
	my $debuglevel = 4;

	TestDebug("opening file $file to compare to array of expected results\n",$debuglevel);
    open( FILE, "<$file" ) || die "error opening $file: $!\n";
    
    while( <FILE> )
    {
	CondorUtils::fullchomp($_);
	$line = $_;
	$linenum++;

	TestDebug("linenum $linenum\n",$debuglevel);
	TestDebug("\$line: $line\n",$debuglevel);
	TestDebug("\$\$aref[0] = $$aref[0]\n",$debuglevel);

	TestDebug("skiplines = \"@skiplines\"\n",$debuglevel);
	#print "grep returns ", grep( /^$linenum$/, @skiplines ), "\n";

	next if grep /^$linenum$/, @skiplines;

	$expectline = shift @$aref;
	if( ! defined $expectline )
	{
	    die "$file contains more text than expected\n";
	}
	CondorUtils::fullchomp($expectline);

	TestDebug("\$expectline: $expectline\n",$debuglevel);

	# if they match, go on
	next if $expectline eq $line;

	# otherwise barf
	warn "$file line $linenum doesn't match expected output:\n";
	warn "actual: $line\n";
	warn "expect: $expectline\n";
	return 0;
    }
	close(FILE);

    # barf if we're still expecting text but the file has ended
    ($expectline = shift @$aref ) && 
        die "$file incomplete, expecting:\n$expectline\n";

    # barf if there are skiplines we haven't hit yet
    foreach my $num ( @skiplines )
    {
	if( $num > $linenum )
	{
	    warn "skipline $num > # of lines in $file ($linenum)\n";
	    return 0;
	}
	croak "invalid skipline argument ($num)" if $num < 1;
    }
    
	TestDebug("CompareText successful\n",$debuglevel);
    return 1;
}

sub IsFileEmpty
{
    my $file = shift || croak "missing file argument";
    return -z $file;
}

sub MergeOutputFiles
{
	my $Testhash = shift || croak "Missing Test hash to Merge Output\n";
	my $basename = $Testhash->{corename};

	foreach my $m ( 0 .. $#{$Testhash->{extensions}} )
	{
		my $newlog = $basename . $Testhash->{extensions}[$m];
		#print "Creating core log $newlog\n";
		open(LOG,">$newlog") || return "1";
		print LOG "***************************** Merge Sublogs ***************************\n";
		foreach my $n ( 0 .. $#{$Testhash->{tests}} )
		{
			# get file if it exists
			#print "Add logs for test $Testhash->{tests}[$n]\n";
			my $sublog = $Testhash->{tests}[$n] . $Testhash->{extensions}[$m];
			if( -e "$sublog" )
			{
				print LOG "\n\n***************************** $sublog ***************************\n\n";
				open(INLOG,"<$sublog") || return "1";
				while(<INLOG>)
				{
					print LOG "$_";
				}
				close(INLOG);
			}
			else
			{
				#print "Can not find $sublog\n";
			}
			#print "$n = $Testhash->{tests}[$n]\n";
			#print "$m = $Testhash->{extensions}[$m]\n";
		}
		close(LOG);
	}
}

sub ParseMachineAds
{
    my $machine = shift || croak "missing machine argument";
    my $line = 0;
	my $variable;
	my $value;

	if( ! open(PULL, "condor_status -l $machine 2>&1 |") )
    {
		print "error getting Ads for \"$machine\": $!\n";
		return 0;
    }
    
    TestDebug( "reading machine ads from $machine...\n" ,5);
    while( <PULL> )
    {
	CondorUtils::fullchomp($_);
	TestDebug("Raw AD is $_\n",5);
	$line++;

	# skip comments & blank lines
	next if /^#/ || /^\s*$/;

	# if this line is a variable assignment...
	if( /^(\w+)\s*\=\s*(.*)$/ )
	{
	    $variable = lc $1;
	    $value = $2;

	    # if line ends with a continuation ('\')...
	    while( $value =~ /\\\s*$/ )
	    {
		# remove the continuation
		$value =~ s/\\\s*$//;

		# read the next line and append it
		<PULL> || last;
		$value .= $_;
	    }

	    # compress whitespace and remove trailing newline for readability
	    $value =~ s/\s+/ /g;
	    CondorUtils::fullchomp($value);

	
		# Do proper environment substitution
	    if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ )
	    {
			my $envlookup = $ENV{$2};
	    	TestDebug( "Found $envlookup in environment \n",5);
			$value = $1.$envlookup.$3;
	    }

	    TestDebug( "$variable = $value\n" ,5);
	    
	    # save the variable/value pair
	    $machine_ads{$variable} = $value;
	}
	else
	{
	    TestDebug( "line $line of $submit_file not a variable assignment... " .
		   "skipping\n" ,5);
	}
    }
	close(PULL);
    return 1;
}

sub FetchMachineAds
{
	return %machine_ads;
}

sub FetchMachineAdValue
{
	my $key = shift @_;
	if(exists $machine_ads{$key})
	{
		return $machine_ads{$key};
	}
	else
	{
		return undef;
	}
}

#
# Some tests need to wait to be started and as such we will
# use qedit to change the job add.
#

sub setJobAd
{
	my @status;
	my $qstatcluster = shift;
	my $qattribute = shift; # change which job ad?
	my $qvalue = shift;		# whats the new value?
	my $qtype = shift;		# quote if a string...
	my $cmd = "condor_qedit $qstatcluster $qattribute ";
	if($qtype eq "string") {
		$cmd = $cmd . "\"$qvalue\"";
	} else {
		$cmd = $cmd . "$qvalue";
	}
	print "Running this command: $cmd \n";
	# shhhhhhhh third arg 0 makes it hush its output
	my $qstat = CondorTest::runCondorTool($cmd,\@status,0);
	if(!$qstat)
	{
		print "Test failure due to Condor Tool Failure: $cmd\n";
	    return(1)
	}
	foreach my $line (@status)
	{
		#print "Line: $line\n";
	}
}

#
# Is condor_q able to respond at this time? We'd like to get
# a valid response for whoever is asking.
#

sub getJobStatus
{
	my @status;
	my $qstatcluster = shift;
	my $cmd = "condor_q $qstatcluster -format %d JobStatus";
	# shhhhhhhh third arg 0 makes it hush its output
	my $qstat = CondorTest::runCondorTool($cmd,\@status,0);
	if(!$qstat)
	{
		die "Test failure due to Condor Tool Failure: $cmd\n";
	}

	foreach my $line (@status)
	{
		#print "jobstatus: $line\n";
		if( $line =~ /^(\d).*/)
		{
			return($1);
		}
		else
		{
			print "No job status????\n";
			runcmd("condor_q");
			return("0");
		}
	}
	print "No job status????\n";
	runcmd("condor_q");
	return("0");
}

#
# Run a condor tool and look for exit value. Apply multiplier
# upon failure and return 0 on failure.
#


sub runCondorTool
{
	my $trymultiplier = 1;
	my $start_time = time; #get start time
	my $delta_time = 0;
	my $status = 1;
	my $done = 0;
	my $cmd = shift;
	my $arrayref = shift;
	# use unused third arg to skip the noise like the time
	my $quiet = shift;
	my $options = shift; #hash ref
	my $count = 0;
	my %altoptions = ();
	my $failconcerns = 1;

	if(exists ${$options}{expect_result}) {
		$failconcerns = 0;
	}

	# provide an expect_result=>ANY as a hash reference options
	$altoptions{expect_result} = \&ANY;
	if(defined $options) {
		#simply use altoptions
		${$options}{expect_result} = \&ANY;
	} else {
		$options = \%altoptions;
	}
	#Condor::DebugLevel(4);

	# clean array before filling

	my $attempts = 15;
	$count = 0;
	my $hashref;
	while( $count < $attempts) {
		#print "runCondorTool: Attempt: <$count>\n";

		# Add a message to runcmd output
		${$options}{emit_string} = "runCondorTool: Cmd: $cmd Attempt: $count";
		@{$arrayref} = (); #empty return array...
		my @tmparray;
		TestDebug( "Try command: $cmd\n",4);
		#open(PULL, "_condor_TOOL_TIMEOUT_MULTIPLIER=4 $cmd 2>$catch |");

		$hashref = runcmd("_condor_TOOL_TIMEOUT_MULTIPLIER=10 $cmd", $options);
		my @output =  @{${$hashref}{"stdout"}};
		my @error =  @{${$hashref}{"stderr"}};

		$status = ${$hashref}{"exitcode"};
		#print "runCondorTool: Status was <$status>\n";
		TestDebug("Status is $status after command\n",4);
		if(( $status != 0 ) && ($failconcerns == 1)){
				print "runcmd: $cmd Non-zero return: $status\n";
				#print "************* std out ***************\n";
				#print "************* std err ***************\n";
				#print "************* GetQueue() ***************\n";
				if(exists ${$options}{emit_output}) {
					if(${$options}{emit_output} == 0) {
					} else {
						GetQueue();
					}
				} else {
					GetQueue();
				}
				#print "************* GetQueue() DONE ***************\n";
		} else {

			my $line = "";
			if(defined $output[0]) {
				#have some info here
				foreach my $value (@output)
				{
					#print "adding <$value> to passed array ref(runCondorTool)\n";
					push @{$arrayref}, $value;
				}
			}
			$done = 1;
			# There are times like the security tests when we want
			# to see the stderr even when the command works.
			if(defined $error[0]) {
				#have some info here
				foreach my $value (@error)
				{
					#print "adding <$value> to passed array ref(runCondorTool)(stderr)\n";
					push @{$arrayref}, $value;
				}
			}
			my $current_time = time;
			$delta_time = $current_time - $start_time;
			TestDebug("runCondorTool: its been $delta_time since call\n",4);
			#Condor::DebugLevel(2);
			return(1);
		}
		$count = $count + 1;
		TestDebug("runCondorTool: iteration: $count cmd $cmd failed sleep 10 * $count \n",1);
		my $delaynow = 10*$count;
		if(!defined $quiet) {
			print "runCondorTool: this delay: $delaynow\n";
		}
		sleep((10*$count));
	}
	TestDebug( "runCondorTool: $cmd failed!\n",1);
	#Condor::DebugLevel(2);

	return(0);
}

sub runToolNTimes
{
	my $cmd = shift;
    my $goal = shift;
    my $wantoutput = shift;
	my $haveoptions = shift;

    my $count = 0;
    my $stop = 0;
    my @cmdout = ();
    my @date = ();
	my @outarrray;
	my $cmdstatus = 0;
    $stop = $goal;

    while($count < $stop) {
        @cmdout = ();
		@outarrray = ();
        @date = ();
        @date = `date`;
        CondorUtils::fullchomp $date[0];
        #print "$date[0] $cmd $count\n";
        #@cmdout = `$cmd`;
		if(defined $haveoptions) {
			$cmdstatus = runCondorTool($cmd, \@outarrray, 2, $haveoptions);
        } elsif(defined $wantoutput) {
			if($wantoutput == 0) {
				#print "runToolNTimes quiet mode requested for: $cmd\n";
				$cmdstatus = runCondorTool($cmd, \@outarrray, 2, {emit_output=>0});
			} else {
				# be verbose about it
				$cmdstatus = runCondorTool($cmd, \@outarrray, 2);
			}
		} else {
			# be verbose about it
			$cmdstatus = runCondorTool($cmd, \@outarrray, 2);
		}
		if(!$cmdstatus) {
			print "runCondorTool: $cmd attempt: $count SHOULD NOT fail!\n";
		}

        if(defined $wantoutput) {
			if($wantoutput != 0) {
            	foreach my $line (@outarrray) {
                	print "$line";
            	}
			}
        }
        $count += 1;
    }
	return($cmdstatus);
}

# Lets be able to drop some extra information if runCondorTool
# can not do what it is supposed to do....... short and full
# output from condor_q 11/13

sub GetQueue
{
	my @cmd = ("condor_q", "condor_q -l" );
	foreach my $request (@cmd) {
		print "Queue command: $request\n";
		open(PULL, "$request 2>&1 |");
		while(<PULL>)
		{
			CondorUtils::fullchomp($_);
			print "GetQueue: $_\n";
		}
		close(PULL);
	}
}


sub changeDaemonState
{
	my $timeout = 0;
	my $daemon = shift;
	my $state = shift;
	$timeout = shift; # picks up number of tries... back off on how soon we try.
	my $counter = 0;
	my $cmd = "";
	my $foundTotal = "no";
	my $status;
	my (@cmdarray1, @cmdarray2);

	print "Checking for $daemon being $state\n";
	if($state eq "off") {
		$cmd = "condor_off -fast -$daemon";
	} elsif($state eq "on") {
		$cmd = "condor_on -$daemon";
	} else {
		die "Bad state given in changeScheddState: $state\n";
	}

	$status = runCondorTool($cmd,\@cmdarray1,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure: $cmd\n";
		exit(1);
	}

	my $sleeptime = 0;
	$cmd = "condor_status -$daemon";
	while($counter < $timeout ) {
		$foundTotal = "no";
		@cmdarray2 = {};
		print "about to run $cmd try $counter previous sleep $sleeptime\n";
		$status = CondorTest::runCondorTool($cmd,\@cmdarray2,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure: $cmd\n";
			exit(1)
		}

		foreach my $line (@cmdarray2)
		{
			print "$line\n";
			if($daemon eq "schedd") {
				if( $line =~ /.*Total.*/ ) {
					# hmmmm  scheduler responding
					print "Schedd running\n";
					$foundTotal = "yes";
				}
			} elsif($daemon eq "startd") {
				if( $line =~ /.*Backfill.*/ ) {
					# hmmmm  Startd responding
					print "Startd running\n";
					$foundTotal = "yes";
				}
			}
		}

		if( $state eq "on" ) {
			if($foundTotal eq "yes") {
				# is running again
				return(1);
			} else {
				$counter = $counter + 1;
				$sleeptime = ($counter**2);
				sleep($sleeptime);
			}
		} elsif( $state eq "off" ) {
			if($foundTotal eq "no") {
				#is stopped
				return(1);
			} else {
				$counter = $counter + 1;
				$sleeptime = ($counter**2);
				sleep($sleeptime);
			}
		}

	}
	print "Timeout watching for $daemon state change to: $state\n";
	return(0);
}

#######################
#
# find_pattern_in_array
#
#	Find the array index which contains a particular pattern.
#
# 	First used to strip off variant line in output from
#	condor_q -direct when passed quilld, schedd and rdbms
#	prior to comparing the arrays collected from the output
#	of each command....

sub find_pattern_in_array
{
    my $pattern = shift;
    my $harray = shift;
    my $place = 0;

    TestDebug( "Looking for: $pattern size: $#{$harray}\n",4);
    foreach my $member (@{$harray}) {
        TestDebug( "consider $member\n",5);
        if($member =~ /.*$pattern.*/) {
            TestDebug( "Found: $member line: $place\n",4);
            return($place);
        } else {
            $place = $place + 1;
        }
    }
    print "Got to end without finding it: $pattern\n";
    return(-1);
}

#######################
#
# compare_arrays
#
#	We hash each line from an array and verify that each array has the same contents
# 	by having a value for each key equalling the number of arrays. First used to
#	compare output from condor_q -direct quilld, schedd and rdbms

sub compare_arrays
{
    my $startrow = shift;
    my $endrow = shift;
    my $numargs = shift;
    my %lookup = ();
    my $counter = 0;
    TestDebug( "Check $numargs starting row $startrow end row $endrow\n",4);
    while($counter < $numargs) {
        my $href = shift;
        my $thisrow = 0;
        for my $item (@{$href}) {
            if( $thisrow >= $startrow) {
                if($counter == 0) {
                    #initialize each position
                    $lookup{$item} = 1;
                } else {
                    $lookup{$item} = $lookup{$item} + 1;
                    TestDebug( "Set at: $lookup{$item}:$item\n",4);
                }
                TestDebug( "Store: $item\n",5);
            } else {
                TestDebug( "Skip: $item\n",4);
            }
            $thisrow = $thisrow + 1;
        }
        $counter = $counter + 1;
    }
    #loaded up..... now look!
    foreach my $key (keys %lookup) {
        TestDebug( " $key equals $lookup{$key}\n",4);
		if($lookup{$key} != $numargs) {
			print "Arrays are not the same! key: $key is $lookup{$key} and not $numargs\n";
			return(1);
		}
    }
	return(0);
}

##############################################################################
#
# spawn_cmd
#
#	For a process to start two processes. One will start the passed system
# 	call and the other will wait around for completion where it will stuff 
# 	the results where we can check when we care
#
##############################################################################

sub spawn_cmd
{
	my $cmdtowatch = shift;
	my $resultfile = shift;
	my $result;
	my $toppid = fork();
	my $res;
	my $child;
	my $mylog;
	my $retval;

	if($toppid == 0) {

		my $pid = fork();
		if ($pid == 0) {
			# child 1 code....
			$mylog = $resultfile . ".spawn";
			open(LOG,">$mylog") || die "Can not open log: $mylog: $!\n";
			$res = 0;
			print LOG "Starting this cmd: $cmdtowatch\n";
			$res = verbose_system("$cmdtowatch");
			print LOG "Result from $cmdtowatch is: $res\n";
			print LOG "File to watch is: $resultfile\n";
			if($res != 0) {
				print LOG " failed\n";
				close(LOG);
				exit(1);
			} else {
				print LOG " worked\n";
				close(LOG);
				exit(0);
			}
		} 

		open(RES,">$resultfile") || die "Can't open results file: $resultfile:$!\n";
		$mylog = $resultfile . ".watch";
		open(LOG,">$mylog") || die "Can not open log: $mylog: $!\n";
		print LOG "waiting on pid: $pid\n";
		while(($child = waitpid($pid,0)) != -1) { 
			$retval = $?;
			TestDebug( "Child status: $retval\n",4);
			if( WIFEXITED( $retval ) && WEXITSTATUS( $retval ) == 0 ) {
				TestDebug( "Monitor done and status good!\n",4);
				$retval = 0;
			} else {
				my $status = WEXITSTATUS( $retval );
				TestDebug( "Monitor done and status bad: $status!\n",4);
				$retval = 1;
			}
			print RES "Exit $retval \n";
			print LOG "Pid $child res was $retval\n";
		}
		print LOG "Done waiting on pid: $pid\n";
		close(RES);
		close(LOG);

		exit(0);
	} else {
		# we are done
		return($toppid);
	}
}

##############################################################################
#
# getFqdnHost
#
# hostname sometimes does not return a fully qualified
# domain name and we must ensure we have one. We will call
# this function in the tests wherever we were calling
# hostname.
##############################################################################

sub getFqdnHost {
    my $host = hostfqdn();
    CondorUtils::fullchomp($host);
    return($host);
}

##############################################################################
#
# SearchCondorLog
#
# Search a log for a regexp pattern
#
##############################################################################

sub SearchCondorLog
{
    my $daemon = shift;
    my $regexp = shift;

    my $logloc = `condor_config_val ${daemon}_log`;
    CondorUtils::fullchomp($logloc);

    CondorTest::TestDebug("Search this log: $logloc for: $regexp\n",3);
    open(LOG,"<$logloc") || die "Can not open logfile: $logloc: $!\n";
    while(<LOG>) {
        if( $_ =~ /$regexp/) {
            CondorTest::TestDebug("FOUND IT! $_",2);
            return(1);
        }
    }
    return(0);
}

##############################################################################
#
# SearchCondorLogMultiple`
#
# Search a log for a regexp pattern N times on some interval
# 
# May 2103 extension by bt
#
# Find N new patterns in the file lets you wait for a new say negotiator
# 	cycle to start. ($findnew [true/false]
#
# Find the next pattern after this pattern (next $findafter after regexp) 
#
# Find number of matches between two patterns and use call back for count.
#	Count $findbetween which come after $regexp but before $findafter
#
#	see test job_services_during_neg_cycle.run
#
#
##############################################################################

sub SearchCondorLogMultiple
{
    my $daemon = shift;
    my $regexp = shift;
	my $instances = shift;
	my $timeout = shift;
	my $findnew = shift;
	my $findcallback = shift;
	my $findafter = shift;
	my $findbetween = shift;
	my $currentcount = 0;
	my $found = 0;
	my $tried = 0;
	my $goal = 0;
	my $retryregexp = "";

    my $logloc = `condor_config_val ${daemon}_log`;
    CondorUtils::fullchomp($logloc);
    CondorTest::TestDebug("Search this log: $logloc for: $regexp instances = $instances\n",2);
    CondorTest::TestDebug("Timeout = $timeout\n",2);

	# do we want to see X new events
	if($findnew eq "true") {
		# find current event count
   		open(LOG,"<$logloc") || die "Can not open logfile: $logloc: $!\n";
   		while(<LOG>) {
       		if( $_ =~ /$regexp/) {
           		CondorTest::TestDebug("FOUND IT! $_\n",2);
				$currentcount += 1;
				#print "FOUND IT! $_";
       		} else {
           		#CondorTest::TestDebug(".",2);
				#print "Skipping: $_";
			}
   		}
		close(LOG);
		$goal = $currentcount + $instances;
		CondorTest::TestDebug("Raised request to $goal since current count is $currentcount\n",2);
	} else {
		$goal = $instances;
		#print "SearchCondorLogMultiple: goal: $goal\n";
	}


	my $count = 0;
	my $begin = 0;
	my $foundanything = 0;
	my $showdots = 0;
	#my $tolerance = 5;
	my $done = 0;
	while($found < $goal) {
       	CondorTest::TestDebug("Searching Try $tried\n",2);
		$found = 0;
   		open(LOG,"<$logloc") || die "Can not open logfile: $logloc: $!\n";
   		while(<LOG>) {
			fullchomp($_);
			if(defined $findbetween) {
				# start looking for between string after first pattern
				# and stop when you find after string. call match callback
				# with actual count.
				if( $_ =~ /$regexp/) {
					CondorTest::TestDebug("Found start: $_\n",2);
					$begin = 1;
					$goal = 100000;
				} elsif( $_ =~ /$findafter/) {
					CondorTest::TestDebug("Found done: $_\n",2);
					$done = 1;
					if(defined $findcallback) {
						 &$findcallback($count);
					}
					$found = $goal;
					last;
				} elsif($_ =~ /$findbetween/) {
					if($begin == 1) {
						$count += 1;

						CondorTest::TestDebug("Found Match: $_\n",2);
					}
				} else {
					#print ".";
				}
       		} elsif( $_ =~ /$regexp/) {
           		CondorTest::TestDebug("FOUND IT! $_\n",2);
				$found += 1;
				$foundanything += 1;
				if($showdots == 1) {
					print "instances $instances found $found goal $goal: $_\n";
				}
				if((defined $findcallback) and (!(defined $findafter)) and 
					 ($found == $goal)) {
					 #print "Found after: $_\n";
					&$findcallback($_);
				}
				if((defined $findcallback) and (defined $findafter) and 
					 ($found == $goal) and ($findafter eq $regexp)) {
					&$findcallback($_);
					#print "find calllback and exit\n";
					return(1);
				}
				if((defined $findafter) and ($found == $goal)) {
					# change the pattern we are looking for. really only
					# works well when looking for one particular item.
					# undef the second pattern so we get a crack at the callback
					$found = 0;
					$goal = 1;
					#print "Found $regexp now looking for $findafter\n";
					#$showdots = 1;
					# if we retry we want to start looking for the after
					# pattern when we get through the first search.
					$retryregexp = $regexp;
					$regexp = $findafter;
					#$findafter = undef;
				}
       		} else {
           		#CondorTest::TestDebug(".",2);
					if($showdots == 1) {
							print ".";
				}
			}
   		} # log done
		close(LOG);
		CondorTest::TestDebug("Found: $found want: $goal\n",2);
		if($found < $goal) {
			if($retryregexp ne "") {
				$regexp = $retryregexp;
			}
			sleep 1;
		} else {
			#Done
			last;
		}
		$tried += 1;
		if($tried >= $timeout) {
				CondorTest::TestDebug("SearchCondorLogMultiple: About to fail from timeout!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",1);
				if(defined $findcallback) {
					&$findcallback("HitRetryLimit");
				}
				last;
		}
			#if($tolerance == 0) {
				#CondorTest::TestDebug("SearchCondorLogMultiple: About to fail from timeout!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",1);
				#if(defined $findcallback) {
					#&$findcallback("HitRetryLimit");
				#}
				#last;
			#} else {
				#if($foundanything > 0) {
					#CondorTest::TestDebug("SearchCondorLogMultiple: Using builtin tolerance\n",1);
					#$tolerance -= 1;
					#$tried = 1;
				#} else {
					#$tolerance = 0;
					#$tried -= 2;
				#}
			#}
		#}
	}
	if($found < $goal) {
		return(0);
	} else {
		return(1);
	}
}


##############################################################################
##
## SearchCondorSpecialLog
##
## Serach a log for a regexp pattern
## Log not a daemon log but in log location(NEGOTIATOR_MATCH_LOG)
##
###############################################################################


sub SearchCondorSpecialLog
{
    my $logname = shift;
    my $regexp = shift;
    my $allmatch = shift;

    my $matches = 0;
    my $logloc = `condor_config_val log`;
    CondorUtils::fullchomp($logloc);
    $logloc = "$logloc/$logname";

    #print "SearchCondorSpecialLog: $logname/$regexp/$allmatch\n";
    CondorTest::TestDebug("Search this log: $logloc for: $regexp\n",2);
    open(LOG,"<$logloc") || die "Can not open logfile: $logloc: $!\n";
    while(<LOG>) {
        if( $_ =~ /$regexp/) {
            CondorTest::TestDebug("FOUND IT! $_",2);
            $matches += 1;
        } else {
            if($allmatch != 0) {
                CondorTest::TestDebug("Search this log: $logname for: <$regexp no none matching lines allowed\n",2);
                CondorTest::TestDebug("Failing found $_\n",2);
                return(0);
            }
        }
    }
    if($matches > 0) {
        return(1);
    }
    return(0);
}

##############################################################################
#
# PersonalPolicySearchLog
#
# Serach a log for a security policy
#
##############################################################################


sub PersonalPolicySearchLog
{
    my $pid = shift;
    my $personal = shift;
    my $policyitem = shift;
    my $logname = shift;

	my $logdir = `condor_config_val log`;
	CondorUtils::fullchomp($logdir);

    #my $logloc = $pid . "/" . $pid . $personal . "/log/" . $logname;
    my $logloc = $logdir . "/" . $logname;
    TestDebug("Search this log: $logloc for: $policyitem\n",2);
    open(LOG,"<$logloc") || die "Can not open logfile: $logloc: $!\n";
    while(<LOG>) {
        if( $_ =~ /^.*Security Policy.*$/) {
            while(<LOG>) {
                if( $_ =~ /^\s*$policyitem\s*=\s*\"(\w+)\"\s*$/ ) {
                    #print "FOUND IT! $1\n";
                    if(!defined $securityoptions{$1}){
                        TestDebug("Returning: $1\n",2);
                        return($1);
                    }
                }
            }
        }
    }
    return("bad");
}

sub OuterPoolTest
{
	my $cmd = "condor_config_val log";
	my $locconfig = "";
    TestDebug( "Running this command: $cmd \n",2);
    # shhhhhhhh third arg 0 makes it hush its output
	my $logdir = `condor_config_val log`;
	CondorUtils::fullchomp($logdir);
	TestDebug( "log dir is: $logdir\n",2);
	if($logdir =~ /^.*condor_tests.*$/){
		print "Running within condor_tests\n";
		if($logdir =~ /^.*TestingPersonalCondor.*$/){
			TestDebug( "Running with outer testing personal condor\n",2);
			return(1);
		}
	} else {
		print "Running outside of condor_tests\n";
	}
	return(0);
}

sub PersonalCondorTest
{
	my $submitfile = shift;
	my $testname = shift;
	my $cmd = "condor_config_val log";
	my $locconfig = "";
    print "Running this command: $cmd \n";
    # shhhhhhhh third arg 0 makes it hush its output
	my $logdir = `condor_config_val log`;
	CondorUtils::fullchomp($logdir);
	print "log dir is: $logdir\n";
	if($logdir =~ /^.*condor_tests.*$/){
		print "Running within condor_tests\n";
		if($logdir =~ /^.*TestingPersonalCondor.*$/){
			print "Running with outer testing personal condor\n";
			#my $testname = findOutput($submitfile);
			#print "findOutput saya test is $testname\n";
			my $version = "local";
			
			# get a local scheduler running (side a)
			my $configloc = CondorPersonal::StartCondor( $testname, "x_param.basic_personal" ,$version);
			my @local = split /\+/, $configloc;
			$locconfig = shift @local;
			my $locport = shift @local;
			
			TestDebug("---local config is $locconfig and local port is $locport---\n",3);

			#$ENV{CONDOR_CONFIG} = $locconfig;
		}
	} else {
		print "Running outside of condor_tests\n";
	}
	return($locconfig);
}

sub findOutput
{
	my $submitfile = shift;
	open(SF,"<$submitfile") or die "Failed to open: $submitfile:$!\n";
	my $testname = "UNKNOWN";
	my $line = "";
	while(<SF>) {
		CondorUtils::fullchomp($_);
		$line = $_;
		if($line =~ /^\s*[Ll]og\s+=\s+(.*)(\..*)$/){
			$testname = $1;
			my $previouslog = $1 . $2;
			runcmd("rm -f $previouslog");
		}
	}
	close(SF);
	print "findOutput returning: $testname\n";
	if($testname eq "UNKNOWN") {
		print "failed to find testname in this submit file:$submitfile\n";
		runcmd("cat $submitfile");
	}
	return($testname);
}

# Call down to Condor Perl Module for now

sub TestDebug {
    my $string = shift;
    my $level = shift;
	#my ($package, $filename, $line) = caller();
    Condor::debug("TestGlue: $string", $level);
}

sub debug {
    my ($msg, $level) = @_;
    
    if(!(defined $level)) {
    	print timestamp() . " Test: $msg";
    }
    elsif($level <= $DEBUGLEVEL) {
    	print timestamp() . " Test: $msg";
    }
}

sub SetUseNewRunning {
	CondorPersonal::SetUseNewRunning();
	$UseNewRunning = 1;
	print "Set new running check in CondorPersonal\n";
}

sub timestamp {
    return strftime("%H:%M:%S", localtime);
}

sub DebugLevel {
	my $newlevel = shift;
	my $oldlevel = $DEBUGLEVEL;
    $DEBUGLEVEL = $newlevel;
	if($newlevel != $oldlevel) {
		debug("Test debug level moved from: $oldlevel to: $newlevel\n",2);
	}
	return($oldlevel);
}

sub slurp {
    my ($file) = @_;

    if (not -e $file) {
        print "Warning: trying to slurp non-existent file '$file'";
        return undef;
    }

    my $fh = new FileHandle $file;
    if (not defined $fh) {
        print "Warning: could not open file '$file' to slurp: $!";
        return undef;
    }

    my @contents = <$fh>;
    return wantarray ? @contents : join('', @contents);
}

#################################################################
#
# dual State thoughts which condor_who may detect
#
# turning on, Unknown
# turning on, Not Run Yet
# turning on, Coming up
# turning on, mater alive
# turning on, collector alive
# turning on, collector knows xxxxx
# turning on, all daemons
# going down, all daemons
# going down, collector knows XXXXX
# going down, collector alive
# going down, master alive
# down
#
# We need to more concisely know the state of a personal condor.
#
# There are many tests which simply fail because they believe the personal 
# condor they need never came up when it in fact did AND we spend way too
# long failing when we do fail. We are going to move from address file and
# collector information to things we can detect from the data condor_who
# can provide for us.
#
# As noted above, the state of a personal condor is directional. It matters
# if it is coming alive or shuting down.
#
# Note the vaiance of the fields from condor who.
#
# Daemon       Alive  PID    PPID   Exit   Addr                     Executable
# ------       -----  ---    ----   ----   ----                     ----------
# Collector    yes    9727   9722   no     <128.105.121.64:52589>   /scratch/bt/outoftree/release_dir/sbin/condor_collector
# Master       yes    9722   1      no     <128.105.121.64:58661>   /scratch/bt/outoftree/release_dir/sbin/condor_master
# Negotiator   yes    9775   9722   no     <128.105.121.64:59008>   /scratch/bt/outoftree/release_dir/sbin/condor_negotiator
# Schedd       yes    9777   9722   no     <128.105.121.64:55862>   /scratch/bt/outoftree/release_dir/sbin/condor_schedd
# Startd       yes    9776   9722   no     <128.105.121.64:45231>   /scratch/bt/outoftree/release_dir/sbin/condor_startd
#
#
#Daemon       Alive  PID    PPID   Exit   Addr                     Executable
#------       -----  ---    ----   ----   ----                     ----------
#Collector    no     no     no     0      <128.105.121.64:52589>   
#Master       no     no     no     0      <128.105.121.64:58661>   
#Negotiator   no     no     no     0      <128.105.121.64:59008>   
#Schedd       no     no     no     0      <128.105.121.64:55862>   
#Startd       no     no     no     0      <128.105.121.64:45231>
#
# We are switching from parsing this data with regular expressions to
# collecting a daemons information into a hash with up to 7 entries.
# These must be collected dynamically per daemon. We need a stoage 
# method for the collection of daemons which make up a personal condor
# and a way to store the current state.
#
# As seen below, we have an object barely used within the tests and solely
# used within the CondorTest module and unknown within the PersonalCondor
# module which lets us have an object for every personal condor. The proposal is
# to have another object for the current known fields from condor_who 
# which we will populate per daemon as a personal condor comes up. A hash makes the most sense
# as some items might be missing( as the executable for an off daemon).
#
# Note the is_running field in the PersonalCondorInstance. We might
# maintain that while adding two fields to have current state information
# for the current known collection of daemons. To track the make up of the personal
# condor we will have another field to store information on known daemons
# in this instance of a personal condor. The current thinking is to have 
# a hash of daemon objects. Thus we can handle the dynamics of the data
# with a hash of daemon objects available where ever we expose the
# personal condor object. We will have better state information and
# we will examine and change daemon information after collecting it straight 
# into a daemon object.
#
# Daemon field interpretation will concern itself at the point of interest
# as to wether a field is present, a string or a number. Not at the time 
# of collecting the data.
#
# This will separate collecting the data and determining the current state
# of the personal condor and provide a tidy place to expose and access
# the data from.
#
#How does one do this?
#
#$self = {
#   textfile => shift,
#   placeholders => { }         #  { }, not ( )
#};
#      ...
#
#
#      $self->{placeholders}->{$key} = $value;
#      delete $self->{placeholders}->{$key};
#      @keys = keys %{$self->{placeholders}};
#      foreach my ($k,$v) each %{$self->{placeholders}} { ... }

# who data instance for one daemon
{ package WhoDataInstance;
  sub new 
  {
      my $class = shift;
      my $self = {
	  	  daemon => shift,
		  alive => shift,
		  pid => shift,
		  ppid => shift,
		  pidexit => shift,
		  address => shift,
		  binary => shift, # not always defined initially
	  };
      bless $self, $class;
      return $self;
  }
  sub DisplayWhoDataInstance
  {
      my $self = shift;
	  print "$self->{daemon},$self->{alive},$self->{pid},$self->{ppid},$self->{pidexit},$self->{address},$self->{binary}\n";
  }
  sub GetDaemonName
  {
      my $self = shift;
	  return($self->{daemon});
  }
  sub GetAlive
  {
      my $self = shift;
	  return($self->{alive});
  }
  sub MasterLives
  {
      my $self = shift;
	  if($self->{daemon} ne "Master") {
	  	return(0);
	  }
	  if($self->{alive} eq "yes") {
	  	return(1);
	  }
	  return(0);
  }
}

sub LoadWhoData
{
	my $daemon = shift;
	my $alive = shift;
	my $pid = shift;
	my $ppid = shift;
	my $pidexit = shift;
	my $address = shift;
	my $binary = shift;
	my $condor = GetPersonalCondorWithConfig($ENV{CONDOR_CONFIG});
	if($condor == 0) {
		print "LoadWhoData called before the condor instance exists\n";
		return(0);
	}
	#print "<$condor> condor instance\n";
	#print "<$daemon> daemon\n";
	#print "<$alive> alive\n";
	#print "<$pid> pid\n";
	#print "<$ppid> ppid\n";
	#print "<$binary> binary\n";
	my $daemonwho = new WhoDataInstance($daemon, $alive, $pid, $ppid, $pidexit, $address,$binary);
	#print "<$daemonwho> who instance\n";
	$condor->{personal_who_data}->{$daemon} = $daemonwho;
	return(1);
}

# PersonalCondorInstance is used to keep track of each personal
# condor that is launched.

{ package PersonalCondorInstance;
  sub new
  {
      my $class = shift;
      my $self = {
          name => shift,
          condor_config => shift,
          collector_addr => shift,
          is_running => shift, 
		  # are we coming up or shutting down?
		  personal_direction => 0,
		  personal_state => 0,
		  personal_daemon_list => "",
		  personal_who_data => {},
		  #keys %{$self->{placeholders}};
      };
      bless $self, $class;
      return $self;
  }
  sub SetCondorDirection
  {
      my $self = shift;
	  my $direction = shift;
	  $self->{personal_direction} = $direction; # up or down
  }
  sub SetCondorAlive
  {
      my $self = shift;
	  my $alive = shift;
  	  #print "setting alive state to:$alive\n";
	  $self->{is_running} = $alive;
  }
  sub GetCondorAlive
  {
      my $self = shift;
	  return($self->{is_running});
  }
  sub SetCollectorAddress
  {
      my $self = shift;
	  my $port = shift;
	  #print "Setting collector Address:$port \n";
	  $self->{collector_addr} = $port;
  }
  sub DisplayWhoDataInstances
  {
      my $self = shift;
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	#print "$daemonkey: $self->{personal_who_data}->{$daemonkey}\n";
		$self->{personal_who_data}->{$daemonkey}->DisplayWhoDataInstance();
	  }
  }
  sub CollectorSeesSchedd
  {
      my $self = shift;
	  my %daemon_tracking = ();
	  my @daemons = split /\s/, $self->GetDaemonList();
	  # daemons come out all caps
	  # mark all desired daemons as off
	  foreach my $dmember (@daemons) {
	  	 $daemon_tracking{$dmember} = "no";
		 #print "Setting $dmember to no\n";
	  }
	  if((exists $daemon_tracking{COLLECTOR}) and ($daemon_tracking{SCHEDD})) {
	  		my $currenthost = CondorTest::getFqdnHost();
			my @statarray = ();
			CondorTest::runCondorTool("condor_status -schedd -format \"%s\\n\" name",\@statarray,0,{emit_output=>0});
			foreach my $line (@statarray) {
				if( $line =~ /^.*$currenthost.*/) {
					return("yes");
				}
			}
			return("no");
	  } else {
	  	return("yes");
	  }
  }
  sub HasAllLiveDaemons
  {
      my $self = shift;
	  my %daemon_tracking_all = ();
	  my @daemons = split /\s/, $self->GetDaemonList();
	  #print "DAEMON_LIST:$self->GetDaemonList()\n";
	  # daemons come out all caps
	  # mark all desired daemons as off
	  foreach my $dmember (@daemons) {
	  	 $daemon_tracking_all{$dmember} = "no";
		 #print "Setting $dmember to no\n";
	  }
	  # get state of every current daemon into tracking hash
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	  my $translateddaemon = uc $self->{personal_who_data}->{$daemonkey}->GetDaemonName();
		  $daemon_tracking_all{$translateddaemon} = $self->{personal_who_data}->{$daemonkey}->GetAlive();
		  #print "$translateddaemon is $daemon_tracking_all{$translateddaemon}\n";
	  }
	  # look for any still no
	  my $return = "yes";
	  my $noticethis = "These are still down:";
	  foreach my $key (sort keys %daemon_tracking_all) {
	      next if $key =~ /^JOB_ROUTER$/;
	      next if $key =~ /^SHARED_PORT$/;
	  	  if($daemon_tracking_all{$key} eq "no") {
		  	$noticethis = $noticethis . " $key";
		  	$return = "no";
		  }
	  }
	  $noticethis = $noticethis . "\n";
	  #print "$noticethis";
	  return($return);
  }
  sub HasNoLiveDaemons
  {
      my $self = shift;
	  my %daemon_tracking_all = ();
	  my @daemons = split /\s/, $self->GetDaemonList();
	  #print "DAEMON_LIST:$self->GetDaemonList()\n";
	  # daemons come out all caps
	  # mark all desired daemons as off
	  foreach my $dmember (@daemons) {
	  	 $daemon_tracking_all{$dmember} = "yes";
		 #print "Setting $dmember to no\n";
	  }
	  # get state of every current daemon into tracking hash
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	  my $translateddaemon = uc $self->{personal_who_data}->{$daemonkey}->GetDaemonName();
		  $daemon_tracking_all{$translateddaemon} = $self->{personal_who_data}->{$daemonkey}->GetAlive();
		  #print "$translateddaemon is $daemon_tracking_all{$translateddaemon}\n";
	  }
	  # look for any still yes
	  my $return = "yes";
	  my $noticethis = "These are still up:";
	  foreach my $key (sort keys %daemon_tracking_all) {
	      next if $key =~ /^JOB_ROUTER$/;
	      next if $key =~ /^SHARED_PORT$/;
	  	  if($daemon_tracking_all{$key} eq "yes") {
		  	$noticethis = $noticethis . " $key";
		  	$return = "no";
		  }
	  }
	  $noticethis = $noticethis . "\n";
	  #print "$noticethis";
	  return($return);
  }
  sub HasLiveMaster
  {
      my $self = shift;
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	#print "$daemonkey: $self->{personal_who_data}->{$daemonkey}\n";
		if($daemonkey eq "Master") {
			#print "found master\n";
			#$self->{personal_who_data}->{$daemonkey}->DisplayWhoDataInstance();
			return($self->{personal_who_data}->{$daemonkey}->MasterLives());
		}
	  }
	  return(0);
  }
  sub GetCondorState
  {
      my $self = shift;
	  return $self->{personal_state};
  }
  sub GetCondorDirection
  {
      my $self = shift;
	  return $self->{personal_direction};
  }
  sub GetCondorConfig
  {
      my $self = shift;
      return $self->{condor_config};
  }
  sub GetDaemonList
  {
      my $self = shift;
	  if($self->{personal_daemon_list} ne "") {
	  	  return $self->{personal_daemon_list};
	  } else {
	  	  my $dlist = `condor_config_val daemon_list`;
		  CondorUtils::fullchomp($dlist);
		  # have all daemon lists be space separated
		  $_ = uc $dlist;
		  s/\s*,\s/ /g;
		  s/\s+/ /g;
		  s/,/ /g;
		  $dlist = $_;
		  #my @listelements = split /\s/, $dlist;
		  #my $daemoncount = @listelements;
		  #print "Daemon list has $daemoncount elements\n";
		  $self->{personal_daemon_list} = $dlist;
		  #print "Daemon_List:$dlist\n";
		  return($dlist);
	  }
  }
  sub MakeThisTheDefaultInstance
  {
      my $self = shift;
      $ENV{CONDOR_CONFIG} = $self->{condor_config};
  }
  sub GetCollectorAddress
  {
      my $self = shift;
      return $self->{collector_addr};
  }
}

sub ListAllPersonalCondors
{
    print "Personal Condors:\n";
    for my $name ( keys %personal_condors ) {
        my $condor = $personal_condors{$name};
        print "$name: {\n"
            . "  is_running=$condor->{is_running}\n"
            . "  condor_config=$condor->{condor_config}\n"
            . "}\n";
    }
}

sub KillAllPersonalCondors
{
    for my $name ( keys %personal_condors ) {
        my $condor = $personal_condors{$name};
        if ( $condor->{is_running} == 1 ) {
            KillPersonal($condor->{condor_config});
        }
    }
}

sub GenUniqueCondorName
{
    my $name = "condor";
    my $num = 1;
    while( exists $personal_condors{ $name . $num } ) {
	$num = $num + 1;
    }
    return $name . $num;
}

sub GetPersonalCondorWithConfig
{
    my $condor_config  = shift;
	#print "Looking for condor instance matching:$condor_config\n";
    for my $name ( keys %personal_condors ) {
        my $condor = $personal_condors{$name};
        if ( $condor->{condor_config} eq $condor_config ) {
			#print "Found It!\n";
            return $condor;
        }
    }
    # look after verification that we have a unix type path
    if(CondorUtils::is_windows() == 1) {
	    my $convertedpath = `cygpath $condor_config`;
		CondorUtils::fullchomp($convertedpath);
		print "RELooking for condor instance matching:$convertedpath\n";
    	for my $name ( keys %personal_condors ) {
        	my $condor = $personal_condors{$name};
        	if ( $condor->{condor_config} eq $convertedpath ) {
				#print "Found It!\n";
            	return $condor;
        	} else {
				print "Consider:$condor->{condor_config}\n";
				print "Lookup:$convertedpath\n";
			}
    	}
		#print "Condor Instance Not created yet!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		#print "These exist\n";
		#ListAllPersonalCondors();
		return 0;
    } else {
	#print "Condor Instance Not created yet!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
	return 0;
	}
}

sub StartPersonal {
    my ($testname, $paramfile, $version) = @_;

    $handle = $testname;
    TestDebug("Starting Perosnal($$) for $testname/$paramfile/$version\n",2);

    my $time = strftime("%Y/%m/%d %H:%M:%S", localtime);
    print "$time: About to start a personal Condor in CondorTest::StartPersonal\n";
	#print "Param file is <$paramfile> which contains:\n";
	#system("cat $paramfile");
    my $condor_info = CondorPersonal::StartCondor( $testname, $paramfile ,$version);
    $time = strftime("%Y/%m/%d %H:%M:%S", localtime);
    print "$time: Finished starting personal Condor in CondorTest::StartPersonal\n";

    my @condor_info = split /\+/, $condor_info;
    my $condor_config = shift @condor_info;
    my $collector_port = shift @condor_info;
    my $collector_addr = CondorPersonal::FindCollectorAddress();

	if(CondorUtils::is_windows() == 1) {
		my $windowsconfig = `cygpath -m $condor_config`;
		CondorUtils::fullchomp($windowsconfig);
		print "New windows config <$windowsconfig>\n";
		$condor_config = $windowsconfig;
	}

    $time = strftime("%Y/%m/%d %H:%M:%S", localtime);
    print "$time: Calling PersonalCondorInstance in CondorTest::StartPersonal\n";
    my $new_condor = new PersonalCondorInstance( $version, $condor_config, $collector_addr, 1 );
    $personal_condors{$version} = $new_condor;
	# assume we are creating so we may bring it up, thus direction up or down now up.
	$new_condor->SetCondorDirection("up");
    $time = strftime("%Y/%m/%d %H:%M:%S", localtime);
    #print "$time: Finished calling PersonalCondorInstance in CondorTest::StartPersonal\n";

    return($condor_info);
}

sub CreateAndStoreCondorInstance
{
	my $version = shift;
	my $condorconfig = shift;
	my $collectoraddr = shift;
#print "CreateAndStoreCondorInstance: version: $version config: $condorconfig\n";
	my $amalive = shift;

	if(CondorUtils::is_windows() == 1) {
		my $windowsconfig = `cygpath -m $condorconfig`;
		CondorUtils::fullchomp($windowsconfig);
		print "New windows config <$windowsconfig>\n";
		$condorconfig = $windowsconfig;
	}

	my $new_condor = new PersonalCondorInstance( $version, $condorconfig, $collectoraddr, $amalive );
	$personal_condors{$version} = $new_condor;
#print "Condor instance returned:  $new_condor\n";
	# assume we are creating so we may bring it up, thus direction up or down now up.
	$new_condor->SetCondorDirection("up");
	return($personal_condors{$version});
}

########################
## StartCondorWithParams
##
## Starts up a personal condor that is configured as specified in
## the named arguments to this function.  The personal condor will
## be automatically shut down in this module's END handler or
## when EndTest() is called.
##
## Arguments:
##  condor_name - optional name to be associated with this personal condor
##                (used in naming directories)
##  For other arguments, see CondorPersonal::StartCondorWithParams().
##
## Returns:
##  A PersonalCondorInstance object that may be used to get such things
##  as the location of the config file and the collector address.
##  Sets CONDOR_CONFIG to point to the condor config file.
########################

sub StartCondorWithParams
{
    my %condor_params = @_;
    my $condor_name = $condor_params{condor_name} || "";
    if( $condor_name eq "" ) {
	$condor_name = GenUniqueCondorName();
	$condor_params{condor_name} = $condor_name;
    }

    if( exists $personal_condors{$condor_name} ) {
	die "condor_name=$condor_name already exists!";
    }

    if( ! exists $condor_params{test_name} ) {
	$condor_params{test_name} = GetDefaultTestName();
    }

	# We need to have the condor instance we are bringing up as early as possible
	# for having a place to store condor who data as we come up.

    #my $new_condor = CreateAndStoreCondorInstance( $condor_name, $condor_config, 0, 0 );

    my $condor_info = CondorPersonal::StartCondorWithParams( %condor_params, $condor_name );
    my @condor_info = split /\+/, $condor_info;
    my $condor_config = shift @condor_info;
    my $collector_port = shift @condor_info;

    my $collector_addr = CondorPersonal::FindCollectorAddress();

    #my $new_condor = new PersonalCondorInstance( $condor_name, $condor_config, $collector_addr, 1 );
	my $new_condor = GetPersonalCondorWithConfig($condor_config);
	#$new_condor->{collector_addr} = $collector_addr;
	$new_condor->SetCollectorAddress($collector_addr);
	$new_condor->SetCondorAlive(1);
	#$new_condor->DisplayWhoDataInstances();
	#$new_condor->{is_running} = 1;

    $personal_condors{$condor_name} = $new_condor;

    return $new_condor;
}

sub KillPersonal
{
	my $personal_config = shift;
	my $logdir = "";
	if($personal_config =~ /^(.*[\\\/])(.*)$/) {
		TestDebug("LOG dir is $1/log\n",$debuglevel);
		$logdir = $1 . "/log";
	} else {
		TestDebug("KillPersonal passed this config: $personal_config\n",2);
		e ie "Can not extract log directory\n";
	}
	TestDebug("Doing core ERROR check in  KillPersonal\n",2);
	$failed_coreERROR = CoreCheck($handle, $logdir, $teststrt, $teststop);
	# mark the direction we are going is down/off
	my $condor = GetPersonalCondorWithConfig($personal_config);
	$condor->SetCondorDirection("down");
	#CondorPersonal::KillDaemonPids($personal_config);
	if($UseNewRunning == 1) {
		#print "KillPersonal UseNewRunning set to yes. Calling KillDaemons\n";
		CondorPersonal::KillDaemons($personal_config);
	} else {
		#print "KillPersonal UseNewRunning set to no. Calling KillDaemonPids\n";
		CondorPersonal::KillDaemonPids($personal_config);
	}
	#CondorPersonal::KillDaemons($personal_config);

	if ( $condor ) {
	    $condor->{is_running} = 0;
	}
}

##############################################################################
#
# core and ERROR checking code plus ERROR exemption handling 
#
##############################################################################

sub ShouldCheck_coreERROR
{
	my $logdir = `condor_config_val log`;
	CondorUtils::fullchomp($logdir);
	my $testsrunning = CountRunningTests();
	if(($logdir =~ /TestingPersonalCondor/) &&($testsrunning > 1)) {
		# no because we are doing concurrent testing
		return(0);
	}
	my $saveme = $handle . ".saveme";
	TestDebug("Not /TestingPersonalCondor/ based, saveme is $saveme\n",2);
	TestDebug("Logdir is $logdir\n",2);
	if($logdir =~ /$saveme/) {
		# no because KillPersonal will do it
		return(0);
	}
	TestDebug("Does not look like its in a personal condor\n",2);
	return(1);
}

sub CoreCheck {
	my $test = shift;
	my $logdir = shift;
	my $tstart = shift;
	my $tend = shift;
	my $count = 0;
	my $scancount = 0;
	my $fullpath = "";
	
	if(CondorUtils::is_windows() == 1) {
		#print "CoreCheck for windows\n";
		$logdir =~ s/\\/\//g;
		#print "old log dir <$logdir>\n";
		my $windowslogdir = `cygpath -m $logdir`;
		#print "New windows path <$windowslogdir>\n";
		CondorUtils::fullchomp($windowslogdir);
		$logdir = $windowslogdir;
	}

	if(defined $test) {
		TestDebug("Checking: $logdir for test: $test\n",2);
	}
	my @files = `ls $logdir`;
	my $totalerrors = 0;
	foreach my $perp (@files) {
		CondorUtils::fullchomp($perp);
		$fullpath = $logdir . "/" . $perp;
		if(-f $fullpath) {
			if($fullpath =~ /^.*\/(core.*)$/) {
				# returns printable string
				TestDebug("Checking: $logdir for test: $test Found Core: $fullpath\n",2);
				my $filechange = GetFileTime($fullpath);
				# running sequentially or wrapped core should always
				# belong to the current test. Even if the test has ended
				# assign blame and move file so we can investigate.
				my $newname = MoveCoreFile($fullpath,$coredir);
				FindStackDump($logdir);
				print "\nFound core: $fullpath\n";
				AddFileTrace($fullpath,$filechange,$newname);
				$count += 1;
			} else {
				# do not try to read lock files.
				if($fullpath =~ /^(.*)\.lock$/) { next; }
				if(defined $test) {
					TestDebug("Checking: $fullpath for test: $test for ERROR\n",2);
				}
				$scancount = ScanForERROR($fullpath,$test,$tstart,$tend);
				$totalerrors += $scancount;
				TestDebug("After ScanForERROR error count: $scancount\n",2);
			}
		} else {
			TestDebug( "Not File: $fullpath\n",2);
		}
	}
	
	my $retmsg = ""; 
	if(($count > 0) || ($totalerrors > 0)) {
		$retmsg = "Found $count Core Files and $scancount ERROR statements";
	} else {
		$retmsg = "";
	}
	#print "CoreCheck returning <$retmsg>\n";
	return($retmsg);
}

sub FindStackDump 
{
 	my $logdir = shift;
	my @files = `ls $logdir/*Log*`;
	my $droplines = 0;
	my $line;
	my @dumpstore = ();
	my $leave = 0;
	my $done = 0;
	my $size = 0;

	foreach my $perp (@files) {
		$droplines = 0;
		$done = 0;
		$size = 0;
		fullchomp($perp);
		#print "Looking in $perp for a stack dump\n";
		$droplines = 0;
		open(PERP,"<$perp") or die "Can not open $perp:$!\n";
		while(<PERP>) {
			if($done == 0) {
				#print "looking at $_";
				fullchomp($_);
				$line = $_;
				if($line =~ /^.*?Stack dump.*$/) {
					$droplines = 1;
					print "Stack dump from $perp follows:\n";
				}
				#does it start with a date?
				if($line =~ /^\d+\/\d+\/\d+.*$/) {
					if($droplines == 1) {
						if($size > 0) {
							$droplines = 0;
						}
						#print "Time stamp signaling CPlusPlusfilt\n";
						CPlusPlusfilt(\@dumpstore);
						$done = 1; # one dump per file is fine.
						#last;
					}
				}
				if($droplines == 1) {
					print "$line\n";
					push @dumpstore, $line;
					$size = @dumpstore;
					#$print "dumpstore size now $size\n";
					#print "added $line to dumpstore array\n";
				}
			}
		}
		#print "Done looking at $perp\n";
	}
}

sub CPlusPlusfilt
{
	my $dumpref = shift;
	my $tempfile = "encodeddump" . "$$";
	my $delement = pop @{$dumpref};

	print "------------------------------------------\n";
	open(TF,">$tempfile") or die "Could not create temp file $tempfile:$!\n";

	# preserve ordering by taking last pushes to array first
	while(defined $delement) {
		print TF "$delement\n"; 
		#print "delement: $delement\n";
		$delement = shift @{$dumpref};
	}
	close(TF);
	# am I windows? don't look to see if you have c++filt. Otherwise try before
	# you do system call
	if(is_windows()) {
		# will not use c++cfilt to demangle
	} else {
		my $filtprog = Which("c++filt");
		if($filtprog =~ /c\+\+filt/) {
			system("cat $tempfile | c++filt");
		} else {
			print "Can not demangle. No c++filt program\n";
		}
	}
	
	# clean up
	system("rm -f $tempfile");
	print "------------------------------------------\n";
}

sub ScanForERROR
{
	my $daemonlog = shift;
	my $testname = shift;
	my $tstart = shift;
	my $tend = shift;
	my $count = 0;
	my $ignore = 1;
	open(MDL,"<$daemonlog") or die "Can not open daemon log: $daemonlog:$!\n";
	my $line = "";
	while(<MDL>) {
		CondorUtils::fullchomp();
		$line = $_;
		# ERROR preceeded by white space and trailed by white space, :, ; or -
		if($line =~ /^\s*(\d+\/\d+\s+\d+:\d+:\d+)\s+ERROR[\s;:\-!].*$/){
			TestDebug("$line TStamp $1\n",2);
			print "$line TStamp $1\n";
			$ignore = IgnoreError($testname,$1,$line,$tstart,$tend);
			if($ignore == 0) {
				$count += 1;
				print "\nFound ERROR: $line\n";
				AddFileTrace($daemonlog, $1, $line);
			}
		} elsif($line =~ /^\s*(\d+\/\d+\s+\d+:\d+:\d+)\s+.*\s+ERROR[\s;:\-!].*$/){
			TestDebug("$line TStamp $1\n",2);
			print "$line TStamp $1\n";
			$ignore = IgnoreError($testname,$1,$line,$tstart,$tend);
			if($ignore == 0) {
				$count += 1;
				print "\nFound ERROR: $line\n";
				AddFileTrace($daemonlog, $1, $line);
			}
		} elsif($line =~ /^.*ERROR.*$/){
			TestDebug("Skipping this error: $line\n",2);
		}
	}
	close(MDL);
	return($count);
}

sub CheckTriggerTime
{	
	my $teststartstamp = shift;
	my $timestring = shift;
	my $tsmon = 0;

	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();

	if($timestring =~ /^(\d+)\/(\d+)\s+(\d+):(\d+):(\d+)$/) {
		$tsmon = $1 - 1;
		my $timeloc = timelocal($5,$4,$3,$mday,$tsmon,$year,0,0,$isdst);
		print "timestamp from test start is $teststartstamp\n";
		print "timestamp fromlocaltime is $timeloc \n";
		if($timeloc > $teststartstamp) {
			return(1);
		}
	}
}

sub GetFileChangeTime
{
	my $file = shift;
	my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, $ctime, $blksize, $blocks) = stat($file);

	return($ctime);
}

sub GetFileTime
{
	my $file = shift;
	my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, $ctime, $blksize, $blocks) = stat($file);

	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($ctime);

	$mon = $mon + 1;
	$year = $year + 1900;

	return("$mon/$mday $hour:$min:$sec");
}

sub AddFileTrace
{
	my $file = shift;
	my $time = shift;
	my $entry = shift;

	my $buildentry = "$time	$file	$entry\n";

	my $tracefile = $coredir . "/core_error_trace";
	local *TF;
	open(TF, '>>', $tracefile)
		or die qq(Unable to open "$tracefile" for writing: $!);
	print TF $buildentry;
	close TF;

	TestDebug("\nFile Trace - $buildentry",2);
	ForceCoreLimit();
}

sub ForceCoreLimit
{
	my $cwd = getcwd(); # where was I
	my $count = 0;
	chdir("$coredir");
	
	opendir DS, "." or die "Can not open dataset: $1\n";
	foreach my $subfile (readdir DS)
	{
		next if $subfile =~ /^\.\.?$/;
		if(-f $subfile) {
			if($subfile =~ /^core\.\d+_\d+.*$/) {
				#print "Found core: $subfile\n";
				$count += 1;
			} else {
				#print "File: $subfile, did not match expected pattern\n";
			}

		}
	}
	close(DS);
	if($count > $MAX_CORES) {
		#print "need to trim from $count core files to $MAX_CORES\n";
		opendir DS, "." or die "Can not open dataset:$!\n";
		foreach my $subfile (readdir DS)
		{
			next if $subfile =~ /^\.\.?$/;
			if(-f $subfile) {
				if($subfile =~ /^core\.\d+_(\d+).*$/) {
					if($1 >= $MAX_CORES) {
						system("rm -f $subfile");
					}
				} else {
				}
	
			}
		}
	close(DS);

	} else {
		# go home
	}

	chdir("$cwd");
}

sub MoveCoreFile
{
	my $oldname = shift;
	my $targetdir = shift;
	my $newname = "";
	# get number for core file rename into trace dir
	my $entries = CountFileTrace();
	if($oldname =~ /^.*\/(core.*)\s*.*$/) {
		$newname = $coredir . "/" . $1 . "_$entries";
		runcmd("mv $oldname $newname");
		#system("rm $oldname");
		return($newname);
	} else {
		TestDebug("Only move core files: $oldname\n",2);
		return("badmoverequest");
	}
}

sub CountFileTrace
{
	my $tracefile = $coredir . "/core_error_trace";
	my $count = 0;

	open(CT, "<", $tracefile) or return 0;
	while(<CT>) { $count ++; }
	close(CT);
	return($count);
}

sub LoadExemption
{
	my $line = shift;
	TestDebug("LoadExemption: $line\n",2);
    my ($test, $required, $message) = split /,/, $line;
    my $save = $required . "," . $message;
    if(exists $exemptions{$test}) {
        push @{$exemptions{$test}}, $save;
		TestDebug("LoadExemption: added another for test $test\n",2);
    } else {
        $exemptions{$test} = ();
        push @{$exemptions{$test}}, $save;
		TestDebug("LoadExemption: added new for test $test\n",2);
    }
}

sub IgnoreError
{
	my $testname = shift;
	my $errortime = shift;
	my $errorstring = shift;
	my $tstart = shift;
	my $tend = shift;
	my $timeloc = 0;
	my $tsmon = 0;

	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();

	if($errortime =~ /^(\d+)\/(\d+)\s+(\d+):(\d+):(\d+)$/) {
		$tsmon = $1 - 1;
		$timeloc = timelocal($5,$4,$3,$mday,$tsmon,$year,0,0,$isdst);
	} else {
		die "Time string into IgnoreError: Bad Format: $errortime\n";
	}
	TestDebug("Start: $tstart ERROR: $timeloc End: $tend\n",2);

	# First item we care about is if this ERROR hapened during this test
	if(($tstart == 0) && ($tend == 0)) {
		# this is happening within a personal condor so do not ignore
	} elsif( ($timeloc < $tstart) || ($timeloc > $tend)) {
		TestDebug("IgnoreError: Did not happen during test\n",2);
		return(1); # not on our watch so ignore
	}

	# no no.... must acquire array for test and check all substrings
	# against current large string.... see DropExemptions below
	TestDebug("IgnoreError called for test: $testname and string: $errorstring\n",2);
	#print "IgnoreError called for test <$testname> and string <$errorstring>\n";
	# get list of per/test specs
	if( exists $exemptions{$testname}) {
		my @testarray = @{$exemptions{$testname}};
		foreach my $oneexemption (@testarray) {
			my( $must, $partialstr) = split /,/,  $oneexemption;
			my $quoted = quotemeta($partialstr);
			TestDebug("Looking for: $quoted in this error: $errorstring\n",2);
			if($errorstring =~ m/$quoted/) {
				TestDebug("IgnoreError: Valid exemption\n",2);
				TestDebug("IgnoreError: Ignore ******** $quoted ******** \n",2);
				return(1);
			} 
		}
	}
	# no exemption for this one
	return(0);
}

sub DropExemptions
{
	foreach my $key (sort keys %exemptions) {
    	print "$key\n";
    	my @array = @{$exemptions{$key}};
    	foreach my $p (@array) {
        	print "$p\n";
    	}
	}
}

##############################################################################
#
#	File utilities. We want to keep an up to date record of every
#	test currently running. If we only have one, then the tests are
#	executing sequentially and we can do full core and ERROR detecting
#
##############################################################################
# Tracking Running Tests
# my $RunningFile = "RunningTests";

sub FindControlFile
{
	my $cwd = getcwd();
	my $runningfile = "";
	CondorUtils::fullchomp($cwd);
	TestDebug( "Current working dir is: $cwd\n",$debuglevel);
	if($cwd =~ /^(.*condor_tests)(.*)$/) {
		$runningfile = $1 . "/" . $RunningFile;
		TestDebug( "Running file test is: $runningfile\n",$debuglevel);
		if(!(-d $runningfile)) {
			TestDebug( "Creating control file directory: $runningfile\n",$debuglevel);
			runcmd("mkdir -p $runningfile");
		}
	} else {
		die "Lost relative to where: $RunningFile is :-(\n";
	}
	return($runningfile);
}

sub CleanControlFile
{
	my $controlfile = FindControlFile();
	if( -d $controlfile) {
		TestDebug( "Cleaning old active test running file holding:\n",$debuglevel);
		runcmd("ls $controlfile");
		runcmd("rm -rf $controlfile");
	} else {
		TestDebug( "Creating new active test running file\n",$debuglevel);
	}
	runcmd("mkdir -p $controlfile");
}


sub CountRunningTests
{
	my $runningfile = FindControlFile();
	my $ret;
	my $line = "";
	my $count = 0;
	my $here = getcwd();
	chdir($runningfile);
	my $targetdir = '.';
	opendir DH, $targetdir or die "Can not open $targetdir:$!\n";
	foreach my $file (readdir DH) {
		next if $file =~ /^\.\.?$/;
		next if (-d $file) ;
		$count += 1;
		TestDebug("Counting this test: $file count now: $count\n",2);
	}
	chdir($here);
	return($count);
}

sub AddRunningTest {
    my $test = shift;
    my $runningfile = FindControlFile();
    TestDebug( "Adding: $test to running tests\n",$debuglevel);
    open(OUT, '>', '$runningfile/$test');
    close(OUT);
}

sub RemoveRunningTest {
    my $test = shift;
    my $runningfile = FindControlFile();
    TestDebug( "Removing: $test from running tests\n",$debuglevel);
    unlink("$runningfile/$test");
}


sub IsThisNightly
{
	my $mylocation = shift;

	TestDebug("IsThisNightly passed: $mylocation\n",2);
	if($mylocation =~ /^.*(\/execute\/).*$/) {
		return(1);
	} else {
		return(0);
	}
}

sub is_windows {
	if( $ENV{NMI_PLATFORM} =~ /_win/i ) {
		return 1;
	}
	return 0;
}

# Given a filename and the contents, writes the file to that name.
# it is a fatal error to fail to do so.
sub WriteFileOrDie
{
	my($filename, $body) = @_;
	local *OUT;
	open OUT, '>', $filename
		or croak "CondorTest::WriteFileOrDie: Failed to open(>$filename): $!";
	print OUT $body;
	close OUT;
}

# we want to produce a temporary file to use as a fresh start
# through StartCondorWithParams. 
sub CreateLocalConfig
{
    my $text = shift;
    my $name = shift;
	my $extratext = shift;
    $name = "$name$$";
    open(FI,">$name") or die "Failed to create local config starter file: $name:$!\n";
    print "Created: $name\n";
    print FI "$text";
	if(defined $extratext) {
    	print FI "$extratext";
	}
    runcmd("cat $name");
    close(FI);
    return($name);
}

sub VerifyNoJobsInState
{
	my $state = shift;
	my $number = shift;
    my $maxtries = shift;
    my $done = 0;
    my $count  = 0;
    my @queue = ();
    my $jobsrunning = 0;
	my %jobsstatus = ();;


    while( $done != 1)
    {
        if($count > $maxtries) {
			return($jobsstatus{$state});
        }
        $count += 1;
        @queue = `condor_q`;
        foreach my $line (@queue) {
            fullchomp($line);
            if($line =~ /^(\d+)\s+jobs;\s+(\d+)\s+completed,\s+(\d+)\s+removed,\s+(\d+)\s+idle,\s+(\d+)\s+running,\s+(\d+)\s+held,\s+(\d+)\s+suspended.*$/) {
				#print "$line\n";
				$jobsstatus{jobs} = $1;
				$jobsstatus{completed} = $2;
				$jobsstatus{removed} = $3;
				$jobsstatus{idle} = $4;
				$jobsstatus{running} = $5;
				$jobsstatus{held} = $6;
				$jobsstatus{suspended} = $7;
				if($jobsstatus{$state} == $number){
                    $done = 1;
					print "$number $state\n\n";
					return($number)
				}
            }
        }
        if($done == 0) {
            #print "Waiting for $number $state\n";
            sleep 1;
        }
    }
}

sub AddCheckpoint
{
	$checkpoints++;
}

sub GetCheckpoints
{
	return($checkpoints);
}

sub SetTestName
{
	my $testname = shift;
	Condor::SetHandle($testname);
}

1;
