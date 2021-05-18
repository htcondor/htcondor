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

my %AllowedEvents = ();
my $WindowsWebServerPid = "";
my $WindowsProcessObj = 0;
my $porttool = "";

use base 'Exporter';

our @EXPORT = qw(PrintTimeStamp SetTLOGLevel TLOG timestamp runCondorTool runCondorToolCarefully runToolNTimes RegisterResult EndTest GetLogDir FindHttpPort CleanUpChildren StartWebServer VerifyNumJobsInState);

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
my $DEBUGLEVEL = 2;
my $debuglevel = 3;
my $TLOGLEVEL = 1;

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
my $handle = "CURRENT"; # actually the test name, "CURRENT" for automatic naming based on $0
my $BaseDir = getcwd();

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
	if ($^O =~ /MSWin32/) {
		require Win32::Process;
		require Win32;
	}

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
	$DEBUGLEVEL = 2;
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

	print STDERR "\nTest being marked as FAILED from discovery of core file or ERROR in logs\n\t$failed_coreERROR\n";

	print "\nTest being marked as FAILED from discovery of core file or ERROR in logs\n";
	print "Results indicate: $failed_coreERROR\n";
	print "Time, Log, message are stored in condor_tests/Cores/core_error_trace\n\n";
	print "************************************\n";
	MyHead("-15", "Cores/core_error_trace");
	print "************************************\n";
	print "\n\n";
	RegisterResult(0,test_name=>$handle);
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
	my $no_exit = shift;
    my $extra_notes = "";

    my $exit_status = 0;
    if( Cleanup() == 0 ) {
		print "0 return from cleanup means $failed_coreERROR was not empty\n";
		$exit_status = 1;
    }

	# at this point all the personals started should be stopped
	# so we will validate this and if we can not, this adds a negative result.
	
	## TODO: print this message only if there were personal condor's started..
	debug("EndTest: Checking shutdown state of personal HTCondor(s) : \n");
	my $amidown = "";
	foreach my $name (sort keys %personal_condors) {
		my $condor = $personal_condors{$name};
		my $down = CondorPersonal::ProcessStateWanted($condor->{condor_config});
		$amidown .= "<$name>:$down ";
		if ($down ne "down") {
			RegisterResult(0, test_name=>$handle, check_name=>"<$name> is_down");
		} else {
			RegisterResult(1, test_name=>$handle, check_name=>"<$name> is_down");
		}
	}
	
	# Cleanup stops all personals in test which triggers a CoreCheck per personal
	# not all tests call RegisterResult yet and I changed the ordering in remote_task
	# so to insure the presence of at least one call to RegisterResult passing the
	# CoreCheck result
	if($failed_coreERROR ne "") {
		$exit_status = 1;
		$extra_notes = "$extra_notes\n  $failed_coreERROR\n";
		RegisterResult(0,test_name=>$handle, check_name=>"no_cores_or_errors");
	} else {
		RegisterResult(1,test_name=>$handle, check_name=>"no_cores_or_errors");
	}

	if( $test_failure_count > 0 ) {
		$exit_status = 1;
	}

	if( $test_failure_count == 0 && $test_success_count == 0 ) {
		$extra_notes = "$extra_notes\n  CondorTest::RegisterResult() was never called!";
		$exit_status = 1;
	} elsif ( $test_failure_count != 0) {
		$extra_notes = ", $test_failure_count check(s) FAILED$extra_notes";
	}

	my $result_str = $exit_status == 0 ? "PASSED" : "FAILED";
	my $testname = GetDefaultTestName();

	TestDebug( "$test_success_count check(s) passed$extra_notes\n", 1 );
	TestDebug( "Final status for $testname: $result_str\n", 1);

	if(defined $no_exit) {
		return($exit_status);
	} else {
    	exit($exit_status);
	}
	#return($exit_status);
}

# This should be called in each check function to register the pass/fail result
sub RegisterResult
{
    my $result = shift;
    my %args = @_;

    my $checkname = $args{check_name};
    my $testname = $args{test_name};

    if ( ! defined $checkname) {
        my ($caller_package, $caller_file, $caller_line) = caller();
        $checkname = GetCheckName($caller_file,%args);
    }

    if ( ! defined $testname) {
        $testname = GetDefaultTestName();
    }

    my $result_str = $result == 1 ? "PASSED" : "FAILED";
    print "\n$result_str check $checkname in test $testname\n\n";
    if ($result != 1) { $test_failure_count += 1; }
    else { $test_success_count += 1; }
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
    	# when we add bulk submit lines in RunCheck we really dont
		# want to see them
		if($name ne "append_submit_commands") {
        	my $value = $args{$name};
			if( $arg_str ne "" ) {
	    		$arg_str = $arg_str . ",";
			}
			$arg_str = $arg_str . "$name=$value";
		}
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
	Condor::NonEventReset();
	$hoststring = "notset:000";
	$failed_coreERROR = "";
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

sub IdentifyTimer {
	my $timercalback = shift;
	foreach my $key (keys %test) {
		print "test:$key\n";
		if($test{$key}{"RegisterTimed"} == $timercalback) {
			print "timer identified as $key\n";
			return($key);
		}
	}
	return("");
}

# remove CondorLog::RunCheck timer
#
sub RegisterCLTimed
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";
	my $alarm = shift || croak "missing wait time argument";

    $test{$handle}{"RegisterCLTimed"} = $function_ref;
    $test{$handle}{"RegisterCLTimedWait"} = $alarm;

	# relook at registration and re-register to allow a timer
	# to be set after we are running. 
	# Prior to this change timed callbacks were only regsitered
	# when we call "runTest" and similar calls at the start.

	CheckCLTimedRegistrations($handle);
}

# remove SimpleJob::RunCheck timer
#
sub RemoveCLTimed
{
    my $handle = shift || croak "missing handle argument";

    $test{$handle}{"RegisterCLTimed"} = undef;
    $test{$handle}{"RegisterCLTimedWait"} = undef;
    TestDebug( "Remove timer.......\n",4);
    Condor::RemoveCLTimed( );
}

sub RegisterSJTimed
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";
	my $alarm = shift || croak "missing wait time argument";

    $test{$handle}{"RegisterSJTimed"} = $function_ref;
    $test{$handle}{"RegisterSJTimedWait"} = $alarm;

	# relook at registration and re-register to allow a timer
	# to be set after we are running. 
	# Prior to this change timed callbacks were only regsitered
	# when we call "runTest" and similar calls at the start.

	CheckSJTimedRegistrations($handle);
}

# remove SimpleJob::RunCheck timer
#
sub RemoveSJTimed
{
    my $handle = shift || croak "missing handle argument";

    $test{$handle}{"RegisterSJTimed"} = undef;
    $test{$handle}{"RegisterSJTimedWait"} = undef;
    TestDebug( "Remove timer.......\n",4);
    Condor::RemoveSJTimed( );
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
# callback => 
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
		}

		my $queue = $choices{'_queue'};
		delete $choices{'_queue'};
		$submit_body = '';
		foreach my $k (sort keys %choices) {
			$submit_body .= "$k = $choices{$k}\n";
		}
		$submit_body .= "queue $queue\n";
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
	if( defined $args{ callback } ) {
		$undead = $args{ callback };
	}
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
	my %args = ();

	# global set
	if(defined $handle) {
		$args{testname} = $handle;
	} else { 
		die "DoTest must get a testname\n";
	}

	# global set
	if(defined $submit_file) {
		$args{submit_file} = $submit_file;
	} else {
		$args{submit_file} = "none";
	}

	# We will no longer  fail based on arg count.
	#  We will shorten our actions based on subit file or not.
	if( !(defined $wants_checkpoint)) {
		#die "DoTest must get at least 3 args!!!!!\n";
	} else {
		$args{wants_checkpoint} = $wants_checkpoint;
	}

	if(defined $clusterIDcallback) {
		$args{ClusterIdCallback} = $clusterIDcallback;
	}

	if(defined $dagman_args) {
		$args{dagman_args} = $dagman_args;
	}

	#foreach my $key (sort keys %args) {
		#print "$key:$args{$key}\n";
	#}
	StartTest(%args);

}
	

sub StartTest 
{
    my $status           = -1;
	my $monitorpid = 0;
	my $waitpid = 0;
	my $monitorret = 0;
	my $retval = 0;
	my %args = @_;


	# Many of our tests want to use RegisterResult and EndTest
	# but don't actually rely on RunTest to do the work. So
	# I am enabling a mode where we can call RunTest just to register
	# the test.
	
	# ensure the global test name does not have a newline
	fullchomp($handle);

	if($args{submit_file} eq "none") {
		Condor::SetHandle($handle);
		print "StartTest: No submit file passed in. Only registering test\n";
		return($retval);
	}
	
	TestDebug("StartTest says test: $handle\n",2);;
	# moved the reset to preserve callback registrations which includes
	# an error callback at submit time..... Had to change timing
	CondorTest::Reset();

    # this is kludgey :: needed to happen sooner for an error message callback in runcommand
    Condor::SetHandle($handle);

    # if we want a checkpoint, register a function to force a vacate
    # and register a function to check to make sure it happens
	if( exists $args{wants_checkpoint}) {
		if($args{wants_checkpoint} != 0) {
			Condor::RegisterExecute( \&ForceVacate );
			#Condor::RegisterEvictedWithCheckpoint( sub { $checkpoints++ } );
		} else {
			if(defined $test{$handle}{"RegisterExecute"}) {
				Condor::RegisterExecute($test{$handle}{"RegisterExecute"});
			}
		}
	} else {
		if(defined $test{$handle}{"RegisterExecute"}) {
			Condor::RegisterExecute($test{$handle}{"RegisterExecute"});
		}
	}

	CheckRegistrations();

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
			#print "CONDOR_CONFIG now: $ENV{CONDOR_CONFIG}\n";
			#runcmd("condor_config_val -config");
		}
	}

	AddRunningTest($handle);

	# submit the job and get the cluster id
	TestDebug( "Now submitting test job\n",4);
	my $cluster = 0;

	$teststrt = time();
	# submit the job and get the cluster id
	if(!(exists $args{dagman_args})) {
		$cluster = Condor::TestSubmit( $submit_file );
	} else {
		print "Dagman Test....\n";
		$cluster = Condor::TestSubmitDagman( $submit_file, $args{dagman_args} );
		if($cluster == 0) {
			print "*********** Dag Submit Failed ***********\n";
		}
	}

	if(exists $args{ClusterIdCallback}) {
		my $clusteridcallback = $args{ClusterIdCallback};
		&$clusteridcallback($cluster);
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
# I stumbled on values in the tests which were to be changed by the callbacks were not being
# changed. What was happening in DoTest was that we would fork and the child was
# running monitor and that callbacks were changing the variables in the child's copy.
# The process that is the test calling DoTest was simply waiting for the child to die
# so it could clean up. Now The test switches to be doing the monitoring and the 
# callbacks switch it back up to the test code for a bit. The monitor and the 
# test had always been lock-steped anyways so getting rid of the child
# has had little change except that call backs can be fully functional now.

# bt 4/9/15 bt
# Now we come to the need to handle multiple logs at the same time steming
# from some of the variants of condor_submit foreach. In particular
# queue [n] InitialDir (jobdir*/)
#
# The same callback issues exist so very limit callbacks make sense. We mostly
# want to know that the test passed so we will probably set a ExitSuccess callback
# our selves in the new MultiMonitor. The other place this can happen is in SimpleJob::RunCheck
# when it is called with no_wait.

		my @userlogs = ();
		my $joblogcount = Condor::AccessUserLogs(\@userlogs);
		
		if(exists $args{no_monitor}) {
			print "StartTest: no_monitor set by test - Skipping monitor\n";
		} else {
			if($joblogcount > 1) {
				$monitorret = Condor::MultiMonitor();
			} else {
				$monitorret = Condor::Monitor();
			}
		}
		if(  $monitorret == 1 ) {
			TestDebug( "Monitor happy to exit 0\n",4);
		} else {
			TestDebug( "Monitor not happy to exit 1\n",4);
		}
		$retval = $monitorret;
	}

	TestDebug( "************** condor_monitor back ************************ \n",4);

	$teststop = time();
	my $timediff = $teststop - $teststrt;

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
	my $logdir = GetLogDir();
	my $testsrunning = CountRunningTests();
	my $do_core_check = 1;
	if (($logdir =~ /Config/) && ($testsrunning > 1)) { $do_core_check = 0; }

	if ($do_core_check || defined $wrap_test) {
		my $core_check_reason = ""; if (defined $wrap_test) { $core_check_reason = $wrap_test; }
		debug("StartTest: Calling CoreCheck because $core_check_reason\n", 3);
		$failed_coreERROR = CoreCheck($logdir, $teststrt, $teststop);
		
		if (defined $wrap_test) {
			if ($config ne "") {
				debug("StartTest: Calling KillDaemons for CONDOR_CONFIG=$config\n", 1);
				my $condor = GetPersonalCondorWithConfig($config);
				$condor->SetCondorDirection("down");
				CondorPersonal::KillDaemons($config);
			}
			debug("StartTest: reverting to CONDOR_CONFIG=$lastconfig\n", 2);
			$ENV{CONDOR_CONFIG} = $lastconfig;
		}
	}

	# done with this test
	RemoveRunningTest($handle);

	if ($cluster == 0){
		if( exists $test{$handle}{"RegisterWantError"} ) {
			return(1);
		} else {
			return(0);
		}
	} elsif ($retval) {
		# ok we think we want to pass it but how did core and ERROR checking go
		if ($failed_coreERROR ne "") {
			# Write to STDERR as well as stdout so that we can easily scan the .err files tests that succeeded but failed on cores
			print STDERR "\n'Successful' Test FAILED because $failed_coreERROR\n";
			#
			print "\n'Successful' Test FAILED because $failed_coreERROR\n";
			print "\tThe first 15 lines of condor_tests/Cores/core_error_trace are\n\n";
			print "************************************\n";
			MyHead("-15", "Cores/core_error_trace");
			print "************************************\n";
			print "\n\n";
			$retval = 0;
		}
	}
	return $retval;
}

#############################################
#
# bt 5/4/15
#
#
# We have had timer clashes where we loose callbacks
# because someone else used it wiping out the first timer.
# We feel this will get rid some of the periodic failures.
# SimpleJob.pm will get ir's own timer. This will explain
# almost identicle code but with SJ within it.
#
#

sub CheckCLTimedRegistrations
{
	my $handle = shift;
	# this one event should be possible from ANY state
	# that the monitor reports to us. In this case which
	# initiated the change I wished to start a timer from
	# when the actual runtime was reached and not from
	# when we started the test and submited it. This is the time 
	# at all other regsitrations have to be registered by....

    if( exists $test{$handle} and defined $test{$handle}{"RegisterCLTimed"} )
    {
		TestDebug( "Found a timer to register.......\n",4);
		Condor::RegisterCLTimed( $test{$handle}{"RegisterCLTimed"} , $test{$handle}{"RegisterCLTimedWait"});
    }
}

sub CheckSJTimedRegistrations
{
	my $handle = shift;
	# this one event should be possible from ANY state
	# that the monitor reports to us. In this case which
	# initiated the change I wished to start a timer from
	# when the actual runtime was reached and not from
	# when we started the test and submited it. This is the time 
	# at all other regsitrations have to be registered by....

    if( exists $test{$handle} and defined $test{$handle}{"RegisterSJTimed"} )
    {
		TestDebug( "Found a timer to register.......\n",4);
		Condor::RegisterSJTimed( $test{$handle}{"RegisterSJTimed"} , $test{$handle}{"RegisterSJTimedWait"});
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
		Condor::CheckAllowed("RegisterExitSuccess", $handle, "(got unexpected successful termination)");
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
		Condor::CheckAllowed("RegisterExitFailure", "$handle", "(returned $info{'retval'})");
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
		Condor::CheckAllowed("RegisterExitAbnormal", "$handle", "(got signal $info{'signal'})");
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
		Condor::CheckAllowed("RegisterShadow", "$handle", "(got unexpected shadow exceptions)");
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
		Condor::CheckAllowed("RegisterAbort", "$handle", "(job aborted by user)");
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
		Condor::CheckAllowed("RegisterHold", "$handle", "(job held by user)");
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
		Condor::CheckAllowed("RegisterJobErr", "$handle", "(job error -- see $info{'log'})");
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
		Condor::CheckAllowed("RegisterULog", "$handle", "(job ulog)");
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
		Condor::CheckAllowed("RegisterSuspended", "$handle", "(Suspension not expected)");
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
		Condor::CheckAllowed("RegisterUnsuspended", "$handle", "(Unsuspension not expected)");
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
		Condor::CheckAllowed("RegisterDisconnected", "$handle", "(Disconnect not expected)");
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
		Condor::CheckAllowed("RegisterReconnected", "$handle", "(reconnect not expected)");
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
		Condor::CheckAllowed("RegisterReconnectFailed", "$handle", "(reconnect failed)");
	} );
    }

    # if we wanted to know about requeues.....
    if( defined $test{$handle}{"RegisterEvictedWithRequeue"} )
    {
        Condor::RegisterEvictedWithRequeue( $test{$handle}{"RegisterEvictedWithRequeue"} );
    } else { 
		Condor::RegisterEvictedWithRequeue( sub {
	    my %info = @_;
		Condor::CheckAllowed("RegisterEvictedWithRequeue", "$handle", "(Unexpected Eviction with requeue)");
	} );
	}

    # if we wanted to know about With Checkpoints .....
    if( defined $test{$handle}{"RegisterEvictedWithCheckpoint"} )
    {
        Condor::RegisterEvictedWithCheckpoint( $test{$handle}{"RegisterEvictedWithCheckpoint"} );
    } else { 
		Condor::RegisterEvictedWithCheckpoint( sub {
	    my %info = @_;
		Condor::CheckAllowed("RegisterEvictedWithCheckpoint", "$handle", "(Unexpected Eviction with checkpoint)");
	} );
	}

    # if we wanted to know about Without checkpoints.....
    if( defined $test{$handle}{"RegisterEvictedWithoutCheckpoint"} )
    {
		#print "******** Registering EvictedWithoutCheckpoint handle:$handle ****************\n";
        Condor::RegisterEvictedWithoutCheckpoint( $test{$handle}{"RegisterEvictedWithoutCheckpoint"} );
    } else { 
		#print "******** NOT Registering EvictedWithoutCheckpoint handle:$handle ****************\n";
		Condor::RegisterEvictedWithoutCheckpoint( sub {
	    my %info = @_;
		Condor::CheckAllowed("RegisterEvictedWithoutCheckpoint", "$handle", "(Unexpected Eviction without checkpoint)");
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

	my @ads = ();
	my $res = runCondorTool("condor_status -l $machine",\@ads,2,{emit_output=>0});
    
    TestDebug( "reading machine ads from $machine...\n" ,5);
    #while( <PULL> )
	foreach my $ad (@ads) {
		CondorUtils::fullchomp($ad);
		TestDebug("Raw AD is $ad\n",5);
		$line++;

		# skip comments & blank lines
		$_ = $ad;
		next if /^#/ || /^\s*$/;

		# if this line is a variable assignment...
		if( /^(\w+)\s*\=\s*(.*)$/ ) {
	    	$variable = lc $1;
	    	$value = $2;
	
	    	# if line ends with a continuation ('\')...
	    	while( $value =~ /\\\s*$/ ) {
				# remove the continuation
				$value =~ s/\\\s*$//;
	
				# read the next line and append it
				$ad = shift @ads || last;
				$value .= $ad;
	    	}

	    	# compress whitespace and remove trailing newline for readability
	    	$value =~ s/\s+/ /g;
	    	CondorUtils::fullchomp($value);
	
			# Do proper environment substitution
	    	if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ ) {
				my $envlookup = $ENV{$2};
	    		TestDebug( "Found $envlookup in environment \n",5);
				$value = $1.$envlookup.$3;
	    	}
	
	    	TestDebug( "$variable = $value\n" ,5);
	    	#print "$variable = $value\n";
	    
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
	my $verbose = shift;
	my $cmd = "condor_q $qstatcluster -af JobStatus";
	my $qstat = 1;
	# shhhhhhhh third arg 0 makes it hush its output
	if(defined $verbose) {
		$qstat = CondorTest::runCondorTool($cmd,\@status,0);
	} else {
		$qstat = CondorTest::runCondorTool($cmd,\@status,0,{emit_output=>0});
	}
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
			if(defined $verbose) {
				print "No job status????\n";
				runcmd("condor_q");
			}
			return("0");
		}
	}
	if(defined $verbose) {
		print "No job status????\n";
		runcmd("condor_q");
	}
	return("0");
}

#
# Run a condor tool and look for exit value. Apply multiplier
# upon failure and return 0 on failure.
#
sub PipeCheck {
	my $cmdstring = shift;
	if($cmdstring =~ /\|/) {
		print "*******************************************************\n";
		print "*                                                     *\n";
		print "*               WARNING: runCondorTool                *\n";
		print "*  Pipes will undo our recovery by masking a failure  *\n";
		print "*  with a later return value!!!!!                     *\n";
		print "*  cmd:$cmdstring   *\n";
		print "*                                                     *\n";
		print "*******************************************************\n";
	}
}


sub runCondorToolCarefully {
	my $array = shift( @_ );
	my $quiet = shift( @_ );
	my $options = shift( @_ );
	my $retval = shift( @_ );
	my @argv = @_;

	my %altOptions;
	if( ! defined( $options ) ) {
		$options = \%altOptions;
	}
	${$options}{arguments} = \@argv;

	return runCondorTool( $argv[0], $array, $quiet, $options, $retval );
}


sub runCondorTool
{
	my $trymultiplier = 1;
	my $start_time = time; #get start time
	my $delta_time = 0;
	my $status = 1;
	my $done = 0;
	my $cmd = shift;
	PipeCheck($cmd);
	my $arrayref = shift;
	# use unused third arg to skip the noise like the time
	my $quiet = shift;
	my $options = shift; #hash ref
	my $retval = shift; #ref to return value location, must have 5 args to use
	my $count = 0;
	my %altoptions = ();
	my $failconcerns = 1;

	if(exists ${$options}{expect_result}) {
		#print "for cmd: $cmd, any result is ok\n";
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

	my $attempts = 3; # was 15
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

		if(CondorUtils::is_windows() == 1) {
			if($cmd =~ /condor_who/) {
			} else {
				$ENV{_condor_TOOL_TIMEOUT_MULTIPLIER} = 4; #was 10
			}
			$hashref = runcmd("$cmd", $options);
		} else {
			$hashref = runcmd("_condor_TOOL_TIMEOUT_MULTIPLIER=4 $cmd", $options);
		}
		my @output =  @{${$hashref}{"stdout"}};
		my @error =  @{${$hashref}{"stderr"}};

		$status = ${$hashref}{"exitcode"};
		if(defined $retval ) {
			$retval = $status;
		}
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
			my $sawerror = 0;
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
					if($value =~ /^Error: communication error.*$/) {
						$sawerror = 1;
					}
				}
			}
			my $current_time = time;
			$delta_time = $current_time - $start_time;
			TestDebug("runCondorTool: its been $delta_time since call\n",4);
			#Condor::DebugLevel(2);
			if($sawerror == 1) {
				print "0 return from runcmd but saw:Error: communication error. Going again\n";
			} else {
				return(1);
			}
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
    my $date;
	my @outarrray;
	my $cmdstatus = 0;
    $stop = $goal;

    while($count < $stop) {
        @cmdout = ();
		@outarrray = ();
        $date = CondorUtils::TimeStr('T');
        #print "$date $cmd $count\n";
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

	$status = runCondorTool($cmd,\@cmdarray1,2,{emit_output=>0});
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
		$status = CondorTest::runCondorTool($cmd,\@cmdarray2,2,{emit_output=>0});
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure: $cmd\n";
			exit(1)
		}

		foreach my $line (@cmdarray2)
		{
			#print "$line\n";
			if($daemon eq "schedd") {
				if( $line =~ /.*Total.*/ ) {
					# hmmmm  scheduler responding
					#print "Schedd running\n";
					$foundTotal = "yes";
				}
			} elsif($daemon eq "startd") {
				if( $line =~ /.*Backfill.*/ ) {
					# hmmmm  Startd responding
					#print "Startd running\n";
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
#	condor_q -direct when passed schedd
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
#	compare output from condor_q -direct schedd

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
			if ($retval & 0x7f) {
				# died with signal and maybe coredump.
				# Ignore the fact a coredump happened for now.
				TestDebug( "Monitor done and status bad: \n",4);
				$retval = 1;
			} else {
				# Child returns valid exit code
				my $rc = $retval >> 8;
				$retval = $rc;
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
	my $arrayref = shift;

	my $daemonlog = "$daemon" . "_log";
	my @fetchlog = ();
	@fetchlog = `condor_config_val $daemonlog`;
	my $logdir = $fetchlog[0];
	CondorUtils::fullchomp($logdir);
	my $logloc = $logdir;
	print "SearchCondorLog for daemon:$daemon yielded:$logloc\n";

	my $count = 0;

    CondorTest::TestDebug("Search this log: $logloc for: $regexp\n",3);
    open(LOG,"<$logloc") || die "Can not open logfile: $logloc: $!\n";
    while(<LOG>) {
        if( $_ =~ /$regexp/) {
			$count += 1;
            CondorTest::TestDebug("FOUND IT! $_",2);
			if(defined $arrayref) {
				push @{$arrayref}, $_;
			} else {
            	return(1);
			}
        }
    }
	if($count > 0) {
    	return($count);
	} else {
    	return(0);
	}
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

    
	my $daemonlog = "$daemon" . "_log";
	my @fetchlog = ();
	@fetchlog = `condor_config_val $daemonlog`;
	my $logdir = $fetchlog[0];
	CondorUtils::fullchomp($logdir);
	my $logloc = $logdir;
	print timestamp() . " SearchCondorLog for daemon:$daemon $logloc\n";

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

	my $logdir = GetLogDir();
	my $logloc = $logdir;

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

	my $logdir = GetLogDir();

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
	
	my $logdir = GetLogDir();

	TestDebug( "log dir is: $logdir\n",2);
	if($logdir =~ /^.*condor_tests.*$/){
		print "Running within condor_tests\n";
		if($logdir =~ /^.*Config.*$/){
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

	my $logdir = GetLogDir();

	print "log dir is: $logdir\n";
	if($logdir =~ /^.*condor_tests.*$/){
		print "Running within condor_tests\n";
		if($logdir =~ /^.*Config.*$/){
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
    Condor::debug("TestGlue: $string", $level);
}

sub SetTLOGLevel {
    my $level = shift;
    if ( ! (defined $level)) { $level = 1; }
    $TLOGLEVEL = $level;
}

sub TLOG {
	my $msg = shift;
	my $level = shift;
	if ( ! (defined $level) || $level <= $TLOGLEVEL) {
		print timestamp() . " $msg";
	}
}

sub debug {
    my ($msg, $level) = @_;
    
    if(!(defined $level)) {
    	print timestamp() . " $msg";
    }
    elsif($level <= $DEBUGLEVEL) {
    	print timestamp() . " $msg";
    }
}

sub PrintTimeStamp {
	my $timing = timestamp();
	print "now: $timing\n";
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
# turning on, master alive
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
# Note the variance of the fields from condor who.
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
# These must be collected dynamically per daemon. We need a storage 
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
  sub GetDaemonAndPid
  {
      my $self = shift;
	  my $onedaemon = "$self->{daemon}" . ",$self->{pid}";
	  #print "GetDaemonAndPid: returning $onedaemon\n";
	  return($onedaemon);
  }
  sub GetPidIfAlive
  {
    my $self = shift;
	if ($self->{alive}) { return $self->{pid}; }
	return 0;
  }
  sub DisplayWhoDataInstance
  {
	my $self = shift;
	if(($self->{daemon} eq "Negotiator") ||($self->{daemon} eq "Collector")) {
		print "$self->{daemon}\t$self->{alive}\t$self->{pid}\t$self->{ppid}\t$self->{pidexit}\t$self->{address}\n";
	} else {
		print "$self->{daemon}\t\t$self->{alive}\t$self->{pid}\t$self->{ppid}\t$self->{pidexit}\t$self->{address}\n";
	}
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
	  	#print "master alive:$self->{alive}\n";
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
		print "Looked for condor instance for:$ENV{CONDOR_CONFIG}\n";
		print "LoadWhoData called before the condor instance exists\n";
		ListAllPersonalCondors();
		return(0);
	}
	#
	#print "<$condor> condor instance\n";
	#print "LoadWhoData:<$daemon> daemon ";
	#print "<$alive> alive ";
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
  sub GetCondorName
  {
      my $self = shift;
	  return($self->{name});
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
	  print "Daemon\t\tAlive\tPID\tPPID\tExit\tAddr\n";
	  print "______\t\t_____\t___\t____\t____\t____\n";
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	#print "$daemonkey: $self->{personal_who_data}->{$daemonkey}\n";
		$self->{personal_who_data}->{$daemonkey}->DisplayWhoDataInstance();
	  }
	  print "\n";
  }
  sub WritePidsFile
  {
      my $self = shift;
	  my $file = shift;
	  my $piddata = "";
	  my %refer = (
	  	"Schedd" => "condor_schedd",
		"Negotiator" => "condor_negotiator",
		"Collector" => "condor_collector",
		"Startd" => "condor_startd",
		"SharedPort" => "condor_shared_port",
		"Master" => "MASTER",
	  );
	  open(PF,">$file") or print "PIDS file create failed:$!\n";
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	print "$daemonkey: $self->{personal_who_data}->{$daemonkey}\n";
		$piddata = $self->{personal_who_data}->{$daemonkey}->GetDaemonAndPid();
		print "$piddata\n";
		my @pidandname = split /,/, $piddata;
		my $line = "$pidandname[1] $refer{$pidandname[0]}";
		print PF "$line\n";
		print "$line\n";
	  }
	  close(PF);
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
			CondorTest::runCondorTool("condor_status -schedd -autoformat MyAddress Name",\@statarray,0,{emit_output=>0});
			foreach my $line (@statarray) {
				if( $line =~ /^<\[[:0-9A-Za-z]+]:\d+.*$/ ||
				    $line =~ /^<\d+\.\d+\.\d+\.\d+:\d+.*$/ ) {
					#print "Got a sinful string for schedd:$line\n";
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
		  if(exists $daemon_tracking_all{$translateddaemon}) {
		  	$daemon_tracking_all{$translateddaemon} = $self->{personal_who_data}->{$daemonkey}->GetAlive();
		  	#print "$translateddaemon is $daemon_tracking_all{$translateddaemon}\n";
		  }
	  }
	  # look for any still no
	  my $return = "yes";
	  my $noticethis = "These are still down:";
	  foreach my $key (sort keys %daemon_tracking_all) {
	      next if $key =~ /^JOB_ROUTER$/;
	      next if $key =~ /^SHARED_PORT$/;
	      next if $key =~ /^VIEW_COLLECTOR$/;
	      next if $key =~ /^CKPT_SERVER$/;
	  	  if($daemon_tracking_all{$key} eq "no") {
		  	$noticethis = $noticethis . " $key";
		  	$return = "no";
		  }
	  }
	  $noticethis = $noticethis . "\n";
	  if($return eq "no") {
	  	#print "$noticethis";
	  }
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
		 #print "Setting $dmember to yes\n";
	  }
	  # get state of every current daemon into tracking hash
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	  my $translateddaemon = uc $self->{personal_who_data}->{$daemonkey}->GetDaemonName();
		  if(exists $daemon_tracking_all{$translateddaemon}) {
		  	$daemon_tracking_all{$translateddaemon} = $self->{personal_who_data}->{$daemonkey}->GetAlive();
		  	#print "$translateddaemon is $daemon_tracking_all{$translateddaemon}\n";
		  }
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
	  if($return eq "no") {
	  	#print "$noticethis";
	  }
	  return($return);
  }
  sub HasLiveMaster
  {
      my $self = shift;
	  foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
	  	#print "$daemonkey: $self->{personal_who_data}->{$daemonkey}\n";
		if($daemonkey eq "Master") {
			#print "found master*******************************************************\n";
			#$self->{personal_who_data}->{$daemonkey}->DisplayWhoDataInstance();
			return($self->{personal_who_data}->{$daemonkey}->MasterLives());
		}
	  }
	  return(0);
  }
  sub GetMasterPid
  {
	my $self = shift;
	foreach my $daemonkey (keys %{$self->{personal_who_data}}) {
		if ($daemonkey eq "Master") {
			return $self->{personal_who_data}->{$daemonkey}->GetPidIfAlive();
		}
	}
	return 0;
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

		  my @fetchlog = ();
		  @fetchlog = `condor_config_val daemon_list`;
		  my $dlist = $fetchlog[0];
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

sub PersonalBackUp 
{
	my $config = shift;
	my $retval = 0;
	my $condor_instance = GetPersonalCondorWithConfig($config);
	my $condorname = $condor_instance->GetCondorName();
	$retval = CondorPersonal::NewIsRunningYet($config,$condorname);
	# this function returns 1 for good and 0 for bad
	return($retval);
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
    my $tmp_config  = shift;
	my $condor_config = "";
    if(CondorUtils::is_windows() == 1) {
		if(is_windows_native_perl()) {
			#print "GetPersonalCondorWithConfig: windows_native_perl\n";
			$_ = $tmp_config;
			s/\//\\/g;
			$condor_config = $_;
		} else {
	    	$condor_config = `cygpath -m $tmp_config`;
			CondorUtils::fullchomp($condor_config);
		}
	} else {
		$condor_config = $tmp_config;
	}
	#print "Looking for condor instance matching:$condor_config\n";
    for my $name ( keys %personal_condors ) {
        my $condor = $personal_condors{$name};
        if ( $condor->{condor_config} eq $condor_config ) {
			#print "Found It!->$condor->{condor_config}\n";
            return $condor;
        }
    }
    # look after verification that we have a unix type path
    if(CondorUtils::is_windows() == 1) {
		if(is_windows_native_perl()) {
			print "look after verification that we have a unix type path:$condor_config\n";
		} else {
	    	my $convertedpath = `cygpath -m $condor_config`;
			CondorUtils::fullchomp($convertedpath);
			#print "RELooking for condor instance matching:$convertedpath\n";
    		for my $name ( keys %personal_condors ) {
        		my $condor = $personal_condors{$name};
        		if ( $condor->{condor_config} eq $convertedpath ) {
					#print "Found It!\n";
            		return $condor;
        		} else {
					#print "Consider:$condor->{condor_config}\n";
					#print "Lookup:$convertedpath\n";
				}
    		}
			print "Condor Instance Not created yet!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
			#print "These exist\n";
			#ListAllPersonalCondors();
			return 0;
		}
    } else {
	print "Condor Instance Not created yet!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
	return 0;
	}
}

sub CreatePidsFile {
	# use config to get acondor instance
	
	my $logdir = GetLogDir();

	print "CreatePidsFile: log:$logdir\n";
	my $pidsfile = $logdir . "/PIDS";
	print "File: PIDS:$pidsfile\n";
	my $config = $ENV{CONDOR_CONFIG};
	my $condor_instance = GetPersonalCondorWithConfig($config);
	if($condor_instance != 0) {
		print "Have condor instance to write PIDS file with\n";
		$condor_instance->WritePidsFile($pidsfile);
	} else {
		die "Failed to fetch our condor_instance\n";
	}
}

#************************************************
#
# The mypid arg has never gone past here, and I need to pass along a 
# nowait option. In the gsi test, the new way to determine all daemons
# are up is with condor_who which does not speak gsi. so $mypid
# used pretty much only in the lib_auth_protocol*.run tests goes away
# after this call.
# an optional $nowait passed to CondorPersonal::StartCondor will trigger
# a non-standard daemon up detection within the gsi test itself.
# **********************************************

sub StartPersonal {
    my ($testname, $paramfile, $version, $mypid, $nowait) = @_;

	TestDebug("CondorTest::StartPersonal called for test:$testname node:$version pid:$mypid nowait:$nowait paramfile:$paramfile\n", 4);

    $handle = $testname;
    debug("Creating and starting condor node <$version> for $testname using params from $paramfile\n", 1);

    my $condor_info = CondorPersonal::StartCondor( $testname, $paramfile ,$version, $nowait);

    my @condor_info = split /\+/, $condor_info;
    my $condor_config = shift @condor_info;
    my $collector_port = shift @condor_info;
    my $collector_addr = CondorPersonal::FindCollectorAddress();

	if(CondorUtils::is_windows() == 1) {
		if(is_windows_native_perl()) {
			$condor_config =~ s/\//\\/g;
		} else {
			my $windowsconfig = `cygpath -m $condor_config`;
			CondorUtils::fullchomp($windowsconfig);
			$condor_config = $windowsconfig;
		}
	}

	my $new_condor = new PersonalCondorInstance( $version, $condor_config, $collector_addr, 1 );
	StoreCondorInstance("StartPersonal",$version,$new_condor);
	TestDebug("Condor instance returned:  $new_condor\n", 4);

	$new_condor->SetCondorDirection("up");

    return($condor_info);
}

sub StoreCondorInstance {
	my $caller = shift;
	my $instancename = shift;
	my $instance = shift;
	TestDebug("StoreCondorInstance: caller:$caller name:$instancename assign instance:$instance\n", 4);
    $personal_condors{$instancename} = $instance;
}

sub CreateAndStoreCondorInstance
{
	my $version = shift;
	my $condorconfig = shift;
	my $collectoraddr = shift;
	my $amalive = shift;

	if(CondorUtils::is_windows() == 1) {
		if(is_windows_native_perl()) {
			$condorconfig =~ s/\//\\/g;
		} else {
			my $windowsconfig = `cygpath -m $condorconfig`;
			CondorUtils::fullchomp($windowsconfig);
			$condorconfig = $windowsconfig;
		}
	}

	debug ("Creating a HTCondor node <$version> CONDOR_CONFIG=$condorconfig\n", 1);

	my $new_condor = new PersonalCondorInstance( $version, $condorconfig, $collectoraddr, $amalive );
	StoreCondorInstance("CreateAndStoreCondorInstance",$version,$new_condor);
	TestDebug ("Condor instance returned:  $new_condor\n", 4);

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
		#print "CondorTest::StartCondorWithParams:condor_name unset in CondorTest::StartCondorWithParams\n";
		$condor_name = GenUniqueCondorName();
		$condor_params{condor_name} = $condor_name;
		#print "CondorTest::StartCondorWithParams:Using:$condor_name\n";
    } else {
		#print "CondorTest::StartCondorWithParams:Using requested name:$condor_name\n";
	}

    if( exists $personal_condors{$condor_name} ) {
		die "condor_name=$condor_name already exists!";
    } else {
		#print "StartCondorWithParams:<$condor_name> not in condor_personal hash yet\n";
	}

    if( ! exists $condor_params{test_name} ) {
		$condor_params{test_name} = GetDefaultTestName();
    }

	debug ("CondorTest::StartCondorWithParams added new node <$condor_name>\n", 1);
	if ($DEBUGLEVEL <= 1) {
		foreach my $key (sort keys %condor_params) { print "\t$key = $condor_params{$key}\n"; }
	}

	debug ("CondorTest: Calling CondorPersonal::StartCondorWithParam for node <$condor_name>\n", 2);
    my $condor_info = CondorPersonal::StartCondorWithParams( %condor_params );
	debug ("CondorTest: Back From Calling CondorPersonal::StartCondorWithParam for node <$condor_name>\n", 3);

	if(exists $condor_params{do_not_start}) {
		debug("CondorTest::StartCondorWithParams: node <$condor_name> bailing after config\n", 2);
		return(0);
	} else {
		debug("CondorTest::StartCondorWithParams: node <$condor_name> Full config and run\n", 2);
	}

    my @condor_info = split /\+/, $condor_info;
    my $condor_config = shift @condor_info;
    my $collector_port = shift @condor_info;

    my $collector_addr = CondorPersonal::FindCollectorAddress();

	my $new_condor = GetPersonalCondorWithConfig($condor_config);
	$new_condor->SetCollectorAddress($collector_addr);
	$new_condor->SetCondorAlive(1);

    return $new_condor;
}

sub StartCondorWithParamsStart
{
	my %personal_condor_params = CondorPersonal::FetchParams();
	print "In StartCondorWithParamsStart\n";

	foreach my $key (sort keys %personal_condor_params) {
		#print "$key:$personal_condor_params{$key}\n";
	}

    my $condor_name = $personal_condor_params{condor_name} || "";
    if( $condor_name eq "" ) {
		$condor_name = GenUniqueCondorName();
		$personal_condor_params{condor_name} = $condor_name;
    }

    my $condor_info = CondorPersonal::StartCondorWithParamsStart();
    my @condor_config_and_port = split /\+/, $condor_info;
    my $condor_config = shift @condor_config_and_port;
    my $collector_port = shift @condor_config_and_port;

    my $collector_addr = CondorPersonal::FindCollectorAddress();

    #my $new_condor = new PersonalCondorInstance( $condor_name, $condor_config, $collector_addr, 1 );
	my $new_condor = GetPersonalCondorWithConfig($condor_config);
	#$new_condor->{collector_addr} = $collector_addr;
	$new_condor->SetCollectorAddress($collector_addr);
	$new_condor->SetCondorAlive(1);
	#$new_condor->DisplayWhoDataInstances();
	#$new_condor->{is_running} = 1;

	print "StartCondorWithParamsStart : index into personal_condors with name:<$condor_name> and assignong:$new_condor\n";
	StoreCondorInstance("StartCondorWithParamsStart",$condor_name,$new_condor);
    #$personal_condors{$condor_name} = $new_condor;

    return $new_condor;
}

sub KillPersonal
{
	my $personal_config = shift;
	my $logdir = "";
	my $corecheckret = "";
	if($personal_config =~ /^(.*[\\\/])(.*)$/) {
		#TestDebug("LOG dir is $1/log\n",$debuglevel);
		$logdir = $1 . "/log";
	} else {
		TestDebug("KillPersonal passed this config: $personal_config\n",2);
		e ie "Can not extract log directory\n";
	}
	# mark the direction we are going is down/off
	my $condor = GetPersonalCondorWithConfig($personal_config);
	$condor->SetCondorDirection("down");
	CondorPersonal::KillDaemons($personal_config);
	#print "Doing core ERROR check in  KillPersonal\n";
	$corecheckret = CoreCheck($logdir, $teststrt, $teststop);
	$failed_coreERROR = "$failed_coreERROR" . "$corecheckret";

	if ( $condor ) {
	    $condor->{is_running} = 0;
	}
}


sub CoreCheck {
	my $logdir = shift;
	my $tstart = shift;
	my $tend = shift;
	my $count = 0;
	my $scancount = 0;
	my $fullpath = "";
	my $iswindows = CondorUtils::is_windows();
	
	CondorUtils::fullchomp($logdir);
	if(CondorUtils::is_windows() == 1) {
		if(is_windows_native_perl()) {
			$logdir =~ s/\//\\/g;
		} else {
			$logdir =~ s/\\/\//g;
			my $windowslogdir = `cygpath -m $logdir`;
			CondorUtils::fullchomp($windowslogdir);
			$logdir = $windowslogdir;
		}
	}

	TestDebug("CondorTest: checking for cores and errors in LOG=$logdir\n", 2);

	my @files = ();
	GetDirList(\@files, $logdir);
	my $totalerrors = 0;

	# dump list of files we will check.
	#foreach my $perp (@files) { TestDebug("\tCoreCheck will check: $perp:\n", 2); }

	foreach my $perp (@files) {
		CondorUtils::fullchomp($perp);

		# skip address files
		next if ($perp =~ /^\./);
		# skip lock files
		next if ($fullpath =~ /^(.*)\.lock$/);

		if($iswindows) {
			$fullpath = $logdir . "\\" . $perp;
		} else {
			$fullpath = $logdir . "/" . $perp;
		}

		if ( ! (-f $fullpath)) {
			TestDebug( "\tNot a File: $fullpath\n",2);
			next;
		}

		if ($perp =~ /^core/i) {
			# returns printable string
			TestDebug("CoreCheck Found core file $perp in $logdir\n",2);
			print "CoreCheck found a core file $perp in $logdir\n";

			# running sequentially or wrapped core should always
			# belong to the current test. Even if the test has ended
			# assign blame and move file so we can investigate.
			if (CondorUtils::is_windows() == 1) {
				# windows core files are text, going into test output
				open(CF,"<$fullpath") or die "Failed to open core file:$fullpath:$!\n";
				while (<CF>) { print "    $_"; }
				close(CF);
			}
			my $newname = MoveCoreFile($fullpath,$coredir);
			FindStackDump($logdir);

			my $filechange = GetFileTime($fullpath);
			AddFileTrace($fullpath,$filechange,$newname);
			$count += 1;

		} else {

			$scancount = ScanForERROR($fullpath,$tstart,$tend);
			$totalerrors += $scancount;

			TestDebug("CoreCheck ScanForERROR found $scancount 'ERROR's in $perp\n",2) if ($scancount > 0);
			print "CoreCheck ScanForERROR found $scancount 'ERROR's in $perp\n" if ($scancount > 0);
		}
	}
	
	my $retmsg = ""; 
	if(($count > 0) || ($totalerrors > 0)) {
		$retmsg = "Found $count Core Files and $scancount 'ERROR' statements in " . $logdir;
	} else {
		$retmsg = "";
	}
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
	if(CondorUtils::is_windows()) {
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
	my $tstart = shift;
	my $tend = shift;
	my $count = 0;
	my $ignore = 1;
	open(MDL,"<$daemonlog") or die "ScanForERROR: Cannot open log '$daemonlog' : $!\n";
	my $line = "";
	while(<MDL>) {
		CondorUtils::fullchomp();
		$line = $_;
		# look for ERROR preceeded by white space and trailed by white space, :, ; or -
		if($line =~ /^\s*(\d+\/\d+\s+\d+:\d+:\d+)\s+ERROR[\s;:\-!].*$/){
			# TestDebug("$line TStamp $1\n",2);
			$ignore = IsIgnorableError($1,$line,$tstart,$tend);
			if ( ! $ignore) {
				$count += 1;
				print "\nFound 'ERROR': $line\n";
				AddFileTrace($daemonlog, $1, $line);
			}
		} elsif($line =~ /^\s*(\d+\/\d+\s+\d+:\d+:\d+)\s+.*\s+ERROR[\s;:\-!].*$/){
			TestDebug("$line TStamp $1\n",2);
			$ignore = IsIgnorableError($1,$line,$tstart,$tend);
			if($ignore == 0) {
				$count += 1;
				print "\nFound ERROR: $line\n";
				AddFileTrace($daemonlog, $1, $line);
			}
		} elsif($line =~ /^.*ERROR.*$/){
			#TestDebug("Skipping this error: $line\n",2);
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

	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst);
	if ( ! (defined $ctime)) {
		($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
	} else {
		($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($ctime);
	}

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
	my ($testname, $required, $message) = split /,/, $line;
    my $save = $required . "," . $message;
    if( ! exists $exemptions{CURRENT}) {
        $exemptions{CURRENT} = ();
    }
	TestDebug("LoadExemption: adding ERROR exemption for CURRENT test\n",2);
	push @{$exemptions{CURRENT}}, $save;
}

sub IsIgnorableError
{
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
		die "IsIgnorableError: FATAL error - invalid time: $errortime\n";
	}
	TestDebug("IsIgnorableError: 'ERROR' at $timeloc is within test time range of $tstart to $tend\n",2);

	# First item we care about is if this ERROR hapened during this test
	if(($tstart == 0) && ($tend == 0)) {
		# this is happening within a personal condor so do not ignore
	} elsif( ($timeloc < $tstart) || ($timeloc > $tend)) {
		TestDebug("IsIgnorableError: ignoring 'ERROR' found outside of test time range\n",2);
		return(1); # not on our watch so ignore
	}

	# no no.... must acquire array for test and check all substrings
	# against current large string.... see DropExemptions below
	TestDebug("IsIgnorableError: checking: $errorstring\n",2);
	# get list of per/test specs
	if( exists $exemptions{CURRENT}) {
		my @testarray = @{$exemptions{CURRENT}};
		foreach my $oneexemption (@testarray) {
			my( $must, $partialstr) = split /,/,  $oneexemption;
			my $quoted = quotemeta($partialstr);
			if($errorstring =~ m/$quoted/) {
				TestDebug("\nIsIgnorableError: ignoring 'ERROR' because it matches: $quoted\n",2);
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
# JavaInialize is used to do 3 things
#
# 1. Fail if no java is found in our current search path using our Which
#
# 2. Set a timed limit to any java test
#
# 3. Call a timeout handler here which will scrap JavaDetect message from 
# 	StarterLog
#
##############################################################################

sub JavaInitialize
{
	my $testname = shift;
	my $returnvalue = 0;
	my $iswindows = is_windows();
	my $javacmd =  "";

	if($iswindows == 1) {
    	$javacmd = Which("java.exe");
	} else {
    	$javacmd = Which("java");
	}

	print "JAVA:$javacmd\n";

	# 1. error right away if no java
	if($javacmd eq "") {
		return($returnvalue);
	}
	CondorTest::RegisterTimed($testname, \&JavaTimeout , 120);
	return(1);
}

sub JavaTimeout
{
	my @responses = ();
	print "Timeout: peeking in starter log\n";
	CondorLog::RunCheck(
		daemon=>"STARTER",
		match_regexp=>"JavaDetect",
		all=>\@responses,
	);
	my $sizeresponses = @responses;
	if($sizeresponses > 0) {
		print "Starter log scraping for JavaDetect:\n";
		foreach my $line (@responses) {
			print "$line";
		}
		print "Starter log scraping for JavaDetect over:\n";
	}
	die "Java test timed out\n";
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
	#TestDebug( "Current working dir is: $cwd\n",$debuglevel);
	if($cwd =~ /^(.*condor_tests)(.*)$/) {
		$runningfile = $1 . "/" . $RunningFile;
		#TestDebug( "Running file test is: $runningfile\n",$debuglevel);
		if(!(-d $runningfile)) {
			TestDebug( "Creating control file directory: $runningfile\n",$debuglevel);
			CreateDir("-p $runningfile");
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
    #TestDebug( "Adding: $test to running tests\n",$debuglevel);
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

#sub is_windows {
	#if( $ENV{NMI_PLATFORM} =~ /_win/i ) {
		#return 1;
	#}
	#return 0;
#}

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
    open(FI,">$name") or die "Failed to create local config file: $name:$!\n";
    if(defined $extratext) {
        $text = "$text\n$extratext";
    }
    print FI "$text";
    close(FI);

    debug ("CreateLocalConfig: Created $name for use as a local config file\n$text\n", 1);

    return($name);
}

# we want to produce a temporary file to use as a fresh start
# # through StartCondorWithParams. This is from an array reference
sub CreateLocalConfigFromArrayRef
{
	my $arrayref = shift;
	my $name = shift;
	my $extratext = shift;
	$name = "$name$$";
	open(FI,">$name") or die "Failed to create local config starter file: $name:$!\n";
	#print "Created: $name\n";
	foreach my $line (@{$arrayref}) {
		print FI "$line\n";
	}
	if(defined $extratext) {
		print FI "$extratext";
	}
	close(FI);
	my @configarray = ();
	runCondorTool("cat $name",\@configarray,2,{emit_output=>0});
	print "\nIncorporating the following into the local config file:\n\n";
	foreach my $line (@configarray) {
		print "$line";
	}
	print "\n";
	return($name);
}

sub VerifyNumJobsInState
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
        @queue = `condor_q -tot -all`;
        foreach my $line (@queue) {
            fullchomp($line);
            if($line =~ /(\d+)\s+jobs;\s+(\d+)\s+completed,\s+(\d+)\s+removed,\s+(\d+)\s+idle,\s+(\d+)\s+running,\s+(\d+)\s+held,\s+(\d+)\s+suspended/i) {
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

sub GetLogDir {
	my $logreturn = "";
	my @canditate = `condor_config_val log`;
	$logreturn = $canditate[0];
	fullchomp($logreturn);
	return($logreturn);
}

# we care about a listen owned by one of our pids related
# to the fork of the python web server
# It may not be running yet or it may have failed to start
# Give it up to a few sleeps to be running and ours.
#
# After that restart the process of finding a port to use
# and seeing if it is running.
#

sub CleanUpChildren {
	my $iswindows = shift;
	my $kidhashref = shift;
	my $exitcode = 0;
	print "Interesting pids are:in hash reference\n";
	foreach my $key (keys %{$kidhashref}) {
		print "Trying to kill pid:$key\n";
		if($iswindows) {
			#system("taskkill /PID $child");
			if($key eq $WindowsWebServerPid) {
				print "Kill process which is python webserver\n";
				$$WindowsProcessObj->Kill($exitcode);
				print "Web server ExitCode:$exitcode\n";
			} else {
			system("taskkill /PID $key");
			}
		} else {
			#system("kill -9 $childlist");
				print "Shutting down a regular windows pid:$key\n";
			system("kill -0 $key");
		}
		my $returnedzoombie = 0;
		print "waiting on pid $key\n";
		while(($returnedzoombie = waitpid($key,0)) != -1) {
		}
	}
}

sub FindHttpPort {
	my $parent = shift;
	print "FindHttpPort passed parentpid:$parent\n";
	my $portfile = "portsavefile." . "$parent";
	while(!(-f $portfile)) {
		sleep(1);
	}
	open(PSF,"< $portfile") or die "Failed to open ephemeral:$portfile:$!\n";
	my $port = <PSF>;
	fullchomp($port);
	print "Web server port file:$portfile contains port $port\n";
	if($port > 0) {
		print "Positive port in FindHttpPort:$port\n";
		return($port);
	} else {
		print "Bad port in FindHttpPort:$port\n";
		return($port);
	}
}

sub StartWebServer {
	my $testname = shift;
	my $iswindows = shift;
	my $piddir = shift;
	my $parentpid = shift;
	my $childpidref = shift;
	my @children = ();
	my %childrenpids= ();
	my $childlist = "";
	my $count = 0;
	my $child = 0;
	my $webserverhunt = 0;
	my $havewebserver = 0;
	my $webport = 0;
	my $limit = 5;
	my $kidarraysize = 0;
	my $proc;
	my $spawnedpid = 0;
	my $ephem = 0;

	print "StartWebServer passsed parentpid:$parentpid\n";

	while(($webserverhunt < $limit) &&($havewebserver == 0)) {
		$kidarraysize = 0;
		$childlist = "";
		if($iswindows) {
			my @mypython = `where python`;
			my $pythonbinary = $mypython[0];
			fullchomp($pythonbinary);
			$_ = $pythonbinary;
			s/\\/\\\\/g;
			$pythonbinary = $_;
			chdir("$piddir");
			print "Binary for Process::Create:$pythonbinary\n";
			print " we want to be starting webserver where config files are:\n";
			DirLs();
			Win32::Process::Create($proc,
						"$pythonbinary",
						"python ../simplehttpwrapper.py $parentpid",
						0,
						Win32::Process::NORMAL_PRIORITY_CLASS(),
						".") || die ErrorReport($testname);
			print "Process Create Back\n";
			chdir("..");
			$ephem = FindHttpPort($parentpid);
			print "Trying wget from new server\n";
			system("wget http://127.0.01:$ephem/new_config.local");
			# lets try getting pid
			$spawnedpid = $proc->GetProcessID();
			print "Windows child actually:$spawnedpid\n";
			$child = $spawnedpid; 
			# we'll need to know tis later to kill the web server
			$WindowsProcessObj = $proc;
			$WindowsWebServerPid = $spawnedpid;
			print "Created Python simple web server\n";
			CondorTest::RegisterResult(1,"test_name","$testname");

		} else {
			$child = fork();
			if($child == 0) {
				#child
				chdir("$piddir");
				#system("pwd");
				system("python ../simplehttpwrapper.py $parentpid");
				exit(0);
			}
		}
		$count = 0;
		print "Parent has python webserver pid: Child Now $child\n";
		$ephem = FindHttpPort($parentpid);
		$childlist =  "$child";
		%childrenpids = ();
		@children = ();
		@children = split /\s/, $childlist;
		#Try for a limited time to get the web server going
		#while($count < $limit) {
			#sleep(1);
			#$count += 1;
			##$childlist =  "$child";
			# different child, differnet pids
			#%childrenpids = ();
			
			#@children = ();
			#@children = split /\s/, $childlist;
			#foreach my $kid (@children) {
				#print "adding pid:$kid to pid hash\n";
				#$childrenpids{$kid} = 1;
				#${$childpidref}{$kid} = 1;
			#}
			#$kidarraysize = @children;
			#if($kidarraysize != 2) {
				#print "No child web server yet\n";
				#CleanUpChildren($iswindows,\%childrenpids);
				#next;
			#} else {
				#print "Have a child, hopefully/probably our web server\n";
				#last;
			#}
		#}
		print "Parent moving on maybe with condor_urlfetch\n Mini web server pid:$child\n";

		if($ephem > 0) {
			$havewebserver = 1;
		}

		if($havewebserver != 0) {
			print "OK have a web server\n";
			CondorTest::RegisterResult(1, "test_name", $testname);
			last; # happy
			return(1);
		}
		if($count == $limit) {
			print "BAD Failed to get a webserver\nTry again\n";
			$webserverhunt += 1;
			next;
		}
	}

	if(($webserverhunt == $limit)&&($havewebserver == 0)) {
		CleanUpChildren($iswindows,\%childrenpids);
		print "Failed to get web server in $webserverhunt tries\n";
		CondorTest::RegisterResult(0,"test_name","$testname");
		#EndTest();
		return(0);
	}


}

sub ErrorReport {
	my $testname = shift;
	print Win32::FormatMessage( Win32::GetLastError() );
	CondorTest::RegisterResult(0,"test_name","$testname");
}

1;
