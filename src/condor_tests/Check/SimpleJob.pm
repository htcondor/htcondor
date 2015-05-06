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
	CondorTest::debug("Job submitted\n",1);
};

$aborted = sub 
{
	die "Abort event NOT expected\n";
};

$dummy = sub
{
};

$ExitSuccess = sub {
	CondorTest::debug("Job completed\n",1);
};

sub RunCheck
{
    my %args = @_;
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

	if($args{no_wait}) {
		my $pid = fork();
		if($pid != 0) {
			return(0);
		}
	}
	
	my $queuesz = $args{queue_sz} || 1;

	if($args{check_slots}) {
		# Make sure we never try to start more jobs then slots we have!!!!!!
		$availableslots = ExamineSlots($args{check_slots});
	
		if($availableslots < $queuesz) {
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
		print "duration not set, defaulting to 1\n";
	}

	if(exists $args{timeout}){
		# either way we have to set a timeout
		# overwrite default timer value here
		$timeout = $args{timeout};
	}
	 
	#print "Checking duration being passsed to RunCheck <$args{duration}>\n";
    my $execute_fn = $args{on_execute} || $dummy;
    my $hold_fn = $args{on_hold} || $dummy;
    my $shadow = $args{on_shadow} || $dummy;
    my $suspended_fn = $args{on_suspended} || $dummy;
    my $released_fn = $args{on_released} || $dummy;
    my $unsuspended_fn = $args{on_unsuspended} || $dummy;
    my $disconnected_fn = $args{on_disconnected} || $dummy;
    my $reconnected_fn = $args{on_reconnected} || $dummy;
    my $reconnectfailed_fn = $args{on_reconnectfailed} || $dummy;
    my $imageupdated_fn = $args{on_imageupdated} || $dummy;
    my $evicted_ewc_fn = $args{on_evictedwithcheckpoint} || $dummy;
    my $evicted_ewoc_fn = $args{on_evictedwithoutcheckpoint} || $dummy;
    my $evicted_wreq_fn = $args{on_evictedwithrequeue} || $dummy;
	my $wanterror_fn =$args{on_wanterror} || $dummy;
    my $submit_fn = $args{on_submit} || $submitted;
    my $ulog_fn = $args{on_ulog} || $dummy;
    my $abort_fn = $args{on_abort} || $aborted;
	my $donewithsuccess_fn = $args{on_success} || $ExitSuccess;


	if(defined $args{alt_timed}) {
		CondorTest::RegisterTimed( $testname, $args{alt_timed}, $timeout);
	} else {
		CondorTest::RegisterTimed( $testname, $timed_callback, $timeout);
	}

    CondorTest::RegisterAbort( $testname, $abort_fn );
    CondorTest::RegisterExitedSuccess( $testname, $donewithsuccess_fn );
    CondorTest::RegisterExecute($testname, $execute_fn);
    CondorTest::RegisterULog($testname, $ulog_fn);
    CondorTest::RegisterSubmit( $testname, $submit_fn );

	#If we register thees to dummy, then we don't get
	#the error function registered which says this is bad

	if( exists $args{on_shadow} ) {
    	CondorTest::RegisterShadow( $testname, $shadow );
	}
	if( exists $args{on_wanterror} ) {
    	CondorTest::RegisterWantError( $testname, $wanterror_fn );
	}
	if( exists $args{on_imageupdated} ) {
    	CondorTest::RegisterImageUpdated( $testname, $imageupdated_fn );
	}
	if( exists $args{on_evictedwithcheckpoint} ) {
    	CondorTest::RegisterEvictedWithCheckpoint( $testname, $evicted_ewc_fn );
	}
	if( exists $args{on_evictedwithoutcheckpoint} ) {
    	CondorTest::RegisterEvictedWithoutCheckpoint( $testname, $evicted_ewoc_fn );
	}
	if( exists $args{on_evictedwithrequeue} ) {
    	CondorTest::RegisterEvictedWithRequeue( $testname, $evicted_wreq_fn );
	}
	if( exists $args{on_hold} ) {
    	CondorTest::RegisterHold( $testname, $hold_fn );
	}
	if( exists $args{on_released} ) {
    	CondorTest::RegisterRelease( $testname, $released_fn );
	}
	if( exists $args{on_exitedabnormal} ) {
    	CondorTest::RegisterExitedAbnormal( $testname, $args{on_exitedabnormal} );
	}
	if( exists $args{on_evicted} ) {
    	CondorTest::RegisterEvicted( $testname, $evicted_fn );
	}
	if( exists $args{on_disconnected} ) {
    	CondorTest::RegisterDisconnected( $testname, $disconnected_fn );
	}
	if( exists $args{on_reconnectfailed} ) {
    	CondorTest::RegisterReconnectFailed( $testname, $reconnectfailed_fn );
	}
	if( exists $args{on_reconnected} ) {
    	CondorTest::RegisterReconnected( $testname, $reconnected_fn );
	}
	if( exists $args{on_suspended} ) {
    	CondorTest::RegisterSuspended( $testname, $suspended_fn );
	}
	if( exists $args{on_unsuspended} ) {
    	CondorTest::RegisterUnsuspended( $testname, $unsuspended_fn );
	}
	if(exists $args{on_failure}) {
		CondorTest::RegisterExitedFailure( $testname, $args{on_failure} );
	}

	my $program = $args{runthis} || "x_sleep.pl";
    my $submit_fname = CondorTest::TempFileName("$testname.submit");

	my $namecallback;
	if(exists $args{return_submit_file_name}) {
		$namecallback = $args{return_submit_file_name};
		&$namecallback($submit_fname);
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
    print SUBMIT "queue $queuesz\n";
	if($multi_queue ne "") {
        print SUBMIT "\n" . $multi_queue . "\n";
	}
    close( SUBMIT );

	if (defined $args{GetClusterId}) {
    	$result = CondorTest::RunTest($testname, $submit_fname, 0, $args{GetClusterId});
	} else {
    	$result = CondorTest::RunTest($testname, $submit_fname, 0);
	}
    CondorTest::RegisterResult( $result, %args );
	 if($args{no_wait}) {
		exit(0);
	} else {
		return $result;
	}

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

1;
