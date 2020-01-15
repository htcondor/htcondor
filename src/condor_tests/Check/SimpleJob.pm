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

package SimpleJob;

use CondorTest;

# need a memory so one can callbacks one per test
# not every call to RunCheck  bt 4/29/15
my %savedcallbacks = ();
my %args = ();

# track children so we can reap them later
my @children = ();

my $timeout = 0;
my $defaulttimeout = 300;
$timeout = $defaulttimeout;

$timed_callback = sub
{
	my $now = CondorTest::timestamp();
	print "SimpleJob timeout expired!\n";
	CondorTest::RegisterResult( 0, %args );
    die "$now Test failed from timeout expiring\n";
};

$submitted = sub
{
	my %info = @_;
	my $jobid = $info{'cluster'} . '.' . $info{'job'};
	CondorTest::debug("$jobid Job submitted\n",1);
};

$aborted = sub 
{
	die "Abort event NOT expected\n";
};

$dummy = sub
{
};

$ExitSuccess = sub {
	my %info = @_;
	my $jobid = $info{'cluster'} . '.' . $info{'job'};
	CondorTest::debug("$jobid Job completed successfully\n",1);
};

my %defaultcallbacks = (
	"on_success" => $ExitSuccess,
	"on_submit" => $submitted,
	"on_abort" => 0,
	"on_failure" => 0,
	"on_exitedabnormal" => 0,
	"return_submit_file_name" => 0,
);

sub ShowKeys {
	foreach my $key (sort keys %savedcallbacks) {
		print "key: $key\n";
	}
}

sub CallbackReset {
	%savedcallbacks = ();
}

# we only want to set things into memory if they have been
# explicitly set and explicitly setting it again will override
# first setting.
sub CallBackMemoryUsage {
	my $action = shift;
	#print "Current action in CallBackMemoryUsage is:$action\n";
	#ShowKeys();
	if(exists $args{$action}) {
		#print "Saved and defaults for $action being requested\n";
	}
	if(exists $savedcallbacks{$action}) {
		#print "we have a saved callback for $action\n";
	}
	if((exists $savedcallbacks{$action}) & (!(exists $args{$action}))) {
		# this one has been set before and is not being reset
		#print "CallBackMemoryUsage: saved callback for $action\n";
		return($savedcallbacks{$action});
	} elsif((exists $savedcallbacks{$action}) & (exists $args{$action})) {
		# set before but overriding
		#print "CallBackMemoryUsage: saved callback for $action being changed\n";
		$savedcallbacks{$action} = $args{$action};
		return($args{$action});
	} elsif((!(exists $savedcallbacks{$action})) & (exists $args{$action})) {
		# being explictely set for first time
		#print "CallBackMemoryUsage:$action being set for first time\n";
		#ShowKeys();
		$savedcallbacks{$action} = $args{$action};
		#ShowKeys();
		return($args{$action});
	}
	# now we must look at default vaules which get returned but
	# NOT saved
	if((exists $defaultcallbacks{$action}) & ($defaultcallbacks{$action} != 0)) {
		#print "CallBackMemoryUsage: reserved call back for $action being set, not saved\n";
		# a non dummy callback
		return($defaultcallbacks{$action});
	} elsif((exists $defaultcallbacks{$action}) & ($defaultcallbacks{$action} == 0)) {
		#print "CallBackMemoryUsage: $action has no defaullt callback\n";
		# this one gets no default callback
		return(undef);
	} else {
		# this one gets the function $dummy
		#print "CallBackMemoryUsage: $action gets standard default callback\n";
		return($dummy);
	}
}

sub FigureTimedCallback {
	if((exists $savedcallbacks{alt_timed}) & (!(exists $args{alt_timed}))) {
		# this one has been set before and is not being reset
		#return($savedcallbacks{alt_timed});
	} elsif((exists $savedcallbacks{alt_timed}) & (exists $args{alt_timed})) {
		# set before but overriding
		#$savedcallbacks{alt_timed} = $args{alt_timed};
		return($args{alt_timed});
	} elsif((!(exists $savedcallbacks{alt_timed})) & (exists $args{alt_timed})) {
		# being explictely set for first time
		$savedcallbacks{alt_timed} = $args{alt_timed};
		return($args{alt_timed});
	} elsif((!(exists $savedcallbacks{alt_timed})) & (!(exists $args{alt_timed}))) {
		# using default time callback
		return($timed_callback);
	}
}

sub RunCheck
{
    %args = @_;
	my $availableslots = 0;
    my $result = 0;

################################################################################
#
#
# NOTE: In general there are callbacks requiring action and variable changes
# in the main test file. You can only use no_wait to fork for running really
# simple jobs like start a job that runs forever or start a job on hold.
#
# ANYTHING ELSE WILL NOT WORK AS YOU EXPECT	(bt 1/10/15)
#
#
################################################################################

	#print "Checking duration being passsed to RunCheck <$args{duration}>\n";
    my $execute_fn = CallBackMemoryUsage("on_execute");
    my $hold_fn = CallBackMemoryUsage("on_hold");
    my $shadow_fn = CallBackMemoryUsage("on_shadow");
    my $suspended_fn = CallBackMemoryUsage("on_suspended");
    my $released_fn = CallBackMemoryUsage("on_released");
    my $unsuspended_fn = CallBackMemoryUsage("on_unsuspended");
    my $disconnected_fn = CallBackMemoryUsage("on_disconnected");
    my $reconnected_fn = CallBackMemoryUsage("on_reconnected");
    my $reconnectfailed_fn = CallBackMemoryUsage("on_reconnectfailed");
    my $imageupdated_fn = CallBackMemoryUsage("on_imageupdated");
    my $evicted_ewc_fn = CallBackMemoryUsage("on_evictedwithcheckpoint");
    my $evicted_ewoc_fn = CallBackMemoryUsage("on_evictedwithoutcheckpoint");
    my $evicted_wreq_fn = CallBackMemoryUsage("on_evictedwithrequeue");
	my $wanterror_fn =CallBackMemoryUsage("on_wanterror");
    my $submit_fn = CallBackMemoryUsage("on_submit");
    my $ulog_fn = CallBackMemoryUsage("on_ulog");
    my $abort_fn = CallBackMemoryUsage("on_abort");
	# no default on abort handling handled in above function call
	my $donewithsuccess_fn = CallBackMemoryUsage("on_success");
	my $exitedabnormal_fn = CallBackMemoryUsage("on_exitedabnormal");
	my $exitedfailure_fn = CallBackMemoryUsage("on_failure");
	my $namecallback_fn = CallBackMemoryUsage("return_submit_file_name");

	# question when do I clear this timeout?
	my $timedcallback_fn = FigureTimedCallback();

	# we preset call backs before fork since if child does this and requests saved
	# callbacks, the next child won't see it

	my $submit_only = 0;
	if($args{no_wait}) {
		$submit_only = 1;
		$timedcallback_fn = undef;
# tj - 2015 - dont do this, no_wait really means 'I don't care if the job ever runs!'
#		my $pid = fork();
#		if($pid != 0) {
#			push @children, $pid;
#			#ShowChildren();
#			return(0);
#		}
	}
	
	my $queuesz = $args{queue_sz} || 1;

	if($args{check_slots}) {
		# Make sure we never try to start more jobs then slots we have!!!!!!
		$availableslots = ExamineSlots($args{check_slots});
	
		if($availableslots < $queuesz) {
			printf "RunCheck returning before submission. available slots:$availableslots vs requested:$queuesz\n";
    		CondorTest::RegisterResult( $result, %args );
    		return $result;
		}
	}

    my $testname = $args{test_name} || CondorTest::GetDefaultTestName();
    my $universe = $args{universe} || "vanilla";
	my $hold = $args{hold} || "False";
    my $user_log = $args{user_log} || CondorTest::TempFileName("$testname.user_log");
    my $output = $args{output} || "";
	if($output eq "*") {
		$output = "$testname.out";
	}
    my $input = $args{input} || "";
    my $error = $args{error} || "";
	if($error eq "*") {
		$error = "$testname.err";
	}
	my $deferralpreptime = $args{deferralpreptime} || "";
	my $deferraltime = $args{deferraltime} || "";
	my $deferralwindow = $args{deferralwindow} || "";
    my $requirements = $args{requirements} || "";
    my $streamoutput = $args{stream_output} || "";
    my $append_submit_commands = $args{append_submit_commands} || "";
    my $multi_queue = $args{multi_queue} || "";
    my $grid_resource = $args{grid_resource} || "";
	my $transfer_output_files = $args{transfer_output_files} || "";
	my $transfer_input_files = $args{transfer_input_files} || "";
    my $should_transfer_files = $args{should_transfer_files} || "";
    my $when_to_transfer_output = $args{when_to_transfer_output} || "";
    my $duration = "1";
	if(exists $args{duration}) {
		$duration = $args{duration};
	} else {
		#print "duration not set, defaulting to 1\n";
	}

	if(exists $args{timeout}){
		# either way we have to set a timeout
		# overwrite default timer value here
		$timeout = $args{timeout};
	}
	 
	if(defined $abort_fn) {
    	CondorTest::RegisterAbort( $testname, $abort_fn );
	}
	if(defined $donewithsuccess_fn) {
    	CondorTest::RegisterExitedSuccess( $testname, $donewithsuccess_fn );
	}

	if(defined $execute_fn) {
    	CondorTest::RegisterExecute($testname, $execute_fn);
	}
	if(defined $ulog_fn) {
    	CondorTest::RegisterULog($testname, $ulog_fn);
	}
	if(defined $submit_fn) {
    	CondorTest::RegisterSubmit( $testname, $submit_fn );
	}

	#If we register thees to dummy, then we don't get
	#the error function registered which says this is bad

	if(defined $shadow_fn) {
    	CondorTest::RegisterShadow( $testname, $shadow_fn );
	}
	if(defined $wanterror_fn) {
    	CondorTest::RegisterWantError( $testname, $wanterror_fn );
	}
	if(defined $imageupdated_fn) {
    	CondorTest::RegisterImageUpdated( $testname, $imageupdated_fn );
	}
	if(defined $evicted_ewc_fn) {
    	CondorTest::RegisterEvictedWithCheckpoint( $testname, $evicted_ewc_fn );
	}
	if(defined $evicted_ewoc_fn) {
    	CondorTest::RegisterEvictedWithoutCheckpoint( $testname, $evicted_ewoc_fn );
	}
	if(defined $evicted_wreq_fn) {
    	CondorTest::RegisterEvictedWithRequeue( $testname, $evicted_wreq_fn );
	}
	if(defined $hold_fn) {
    	CondorTest::RegisterHold( $testname, $hold_fn );
	}
	if(defined $released_fn) {
    	CondorTest::RegisterRelease( $testname, $released_fn );
	}
	if(defined $exitedabnormal_fn) {
    	CondorTest::RegisterExitedAbnormal( $testname, $exitedabnormal_fn );
	}
	if(defined $evicted_fn) {
    	CondorTest::RegisterEvicted( $testname, $evicted_fn );
	}
	if(defined $disconnected_fn) {
    	CondorTest::RegisterDisconnected( $testname, $disconnected_fn );
	}
	if(defined $reconnectfailed_fn) {
    	CondorTest::RegisterReconnectFailed( $testname, $reconnectfailed_fn );
	}
	if(defined $reconnected_fn) {
    	CondorTest::RegisterReconnected( $testname, $reconnected_fn );
	}
	if(defined $suspended_fn) {
    	CondorTest::RegisterSuspended( $testname, $suspended_fn );
	}
	if(defined $unsuspended_fn) {
    	CondorTest::RegisterUnsuspended( $testname, $unsuspended_fn );
	}
	if(defined $exitedfailure_fn) {
		CondorTest::RegisterExitedFailure( $testname, $exitedfailure_fn );
	}
	
	if(defined $timedcallback_fn) {
		#print "Timed callbach set in simplejob\n";
		# here starts our new timer just for this module
		# look at timer use in other module here in Check
		# like the log checking ones
		CondorTest::RegisterSJTimed( $testname, $timedcallback_fn, $timeout  );
	}

	my $program = $args{runthis} || "x_sleep.pl";
    my $submit_fname = CondorTest::TempFileName("$testname.submit");

	# was used in concurrency tests is used job_max_running
	if(defined $namecallback_fn) {
		# call it
		&$namecallback_fn($submit_fname);
	}

    open( SUBMIT, ">$submit_fname" ) || die "error writing to $submit_fname: $!\n";
    print SUBMIT "universe = $universe\n";
    print SUBMIT "executable = $program\n";
    print SUBMIT "log = $user_log\n";
    print SUBMIT "hold = $hold\n";
    print SUBMIT "arguments = $duration\n";
    print SUBMIT "notification = never\n";
    if( $grid_resource ne "" ) {
	print SUBMIT "GridResource = $grid_resource\n"
    }
	if($output ne "") {
		print SUBMIT "output = $output\n";
	}
	if($input ne "") {
		print SUBMIT "input = $input\n";
	}
	if($error ne "") {
		print SUBMIT "error = $error\n";
	}
	if($args{request_memory}) {
		print SUBMIT "request_memory = $args{request_memory}\n";
	}
	if($deferralpreptime ne "") {
		print SUBMIT "DeferralPrepTime = $deferralpreptime\n";
	}
	if($deferraltime ne "") {
		print SUBMIT "DeferralTime = $deferraltime\n";
	}
	if($deferralwindow ne "") {
		print SUBMIT "DeferralWindow = $deferralwindow\n";
	}
	if($requirements ne "") {
		print SUBMIT "Requirements = $requirements\n";
	}
	if($streamoutput ne "") {
		print SUBMIT "stream_output = $streamoutput\n";
	}
	if( $transfer_output_files ne "" ) {
		print SUBMIT "transfer_output_files = $transfer_output_files\n";
	}
	if( $transfer_input_files ne "" ) {
		print SUBMIT "transfer_input_files = $transfer_input_files\n";
	}
    if( $should_transfer_files ne "" ) {
		print SUBMIT "should_transfer_files = $should_transfer_files\n";
    }
    if( $when_to_transfer_output ne "" ) {
		print SUBMIT "when_to_transfer_output = $when_to_transfer_output\n";
    }
    if( $append_submit_commands ne "" ) {
        print SUBMIT "\n" . $append_submit_commands . "\n";
    }
    print SUBMIT "queue $queuesz";
	if($multi_queue ne "") {
        print SUBMIT " " . $multi_queue;
	}
	print SUBMIT "\n";
    close( SUBMIT );

	if ($submit_only) {
		my $cluster = Condor::TestSubmit( $submit_fname );

		if(exists $args{ClusterIdCallback}) {
			my $clusteridcallback = $args{ClusterIdCallback};
			&$clusteridcallback($cluster);
		}

		return $cluster ? 0 : 1;
	}

	CondorTest::debug("  RunCheck: calling RunTest($testname, $submit_fname)\n");
	if (defined $args{GetClusterId}) {
    	$result = CondorTest::RunTest($testname, $submit_fname, 0, $args{GetClusterId});
	} else {
    	$result = CondorTest::RunTest($testname, $submit_fname, 0);
	}
	#print "Simple job back!\n";

	Condor::RemoveSJTimed();

    CondorTest::RegisterResult( $result, %args );
#	if(($args{no_wait}) & ($pid == 0)) {
#		#print "Child exiting\n";
#		exit(0);
#	}

}

sub ExamineSlots
{
	my $expectedslots = shift;
	my $slots = $expectedslots;
	my $line = "";
	my $timelimit = 30;
	my $currenttime = 0;
	my $sleeptime = 3;
	my $slotcount = 0;

	print "checking for $slots\n";

	my $available = 0;
	my @jobs = ();
	my $cmd = "condor_status";
	print "In ExamineSlots\n";
	# allow some time to stabilize
	while(($slotcount < $slots) & ($currenttime < $timelimit)) {
		runCondorTool($cmd,\@jobs,2,{emit_output=>0});
		foreach my $job (@jobs) {
			chomp($job);
			$line = $job;
			#print "ExamineSlots\n";
			if($line =~ /^\s*Total\s+(\d+)\s*(\d+)\s*(\d+)\s*(\d+).*/) {
				print "<$4> unclaimed slots\n";
				$available = $4;
			}
		}
		if($avaiilable != $slots) {
			$slotcount = $available;
			$currenttime += $sleeptime;
			sleep($sleeptime);
		}
	}
	print "ExamineSlots returning $slotcount after $currenttime seconds\n";
	return($slotcount);
}

sub ShowChildren {
	my $kids = @children;
	print "ShowChildren: $kids kids\n";
	foreach my $child (@children) {
	}
}

sub HarvestChildren {
	my $kids = @children;
	#print "HarvestChildren: $kids kids\n";
	foreach my $child (@children) {
		#print "waiting for child:$child\n";
		while(($childpid = waitpid($child,0)) != -1) {
			$retvalue = $?;
			if($childpid == 0) {
				#print "Waitinng on child:$child waitpid says:$childpid\n";
			} else {
				#print "Child:$childpid has returned\n";
				if ($retvalue & 0x7f) {
					# died with signal and maybe coredump.
					# Ignore the fact a coredump happened for now.
					TestDebug( "Monitor done and status bad: \n",4);
					$accumulatedreturn += 1;
				} else {
					# Child returns valid exit code
					my $rc = $retvalue >> 8;
					#print "ProcessReturn: Exited normally $rc\n";
					$accumulatedreturn += $rc;
				}
			}
		}
	}
	#print "HarvestChildren: all done\n";
	return(0);
}

1;
