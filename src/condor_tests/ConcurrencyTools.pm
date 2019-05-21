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

package ConcurrencyTools;

use CondorTest;
use CondorUtils;
use base 'Exporter';

our @EXPORT = qw(WaitForIt InitGlobals CheckStatus CountIdle ExamineSlots ExamineQueue QueueMoreJobs DelayOnNegotiator CountRunning InitLimits);


BEGIN 
{
}

END
{
}

my $count = 0;
my $result = 0;
my $running_now = 0;
my $done = 0;
my $allow_too_few_idle_once = 0;
my $expect_idle = 0;
my $expect_run = 0;
my $expect_checks = 0;
my $total_checks = 8;

sub InitLimits {
	TLOG("InitLimits: this function now does nothing. starting real test..\n");
	# we used to start a regular non-concurrency personal and do
	# two set of jobs before testing concurrenct. No More. Just start
	# real test.
};

sub InitGlobals{
	$running_now = shift;
	$done = shift;
	$allow_too_few_idle_once = shift;
	$expect_checks = shift;
	$expect_run = shift;
	$expect_idle = shift;
	TLOG("InitGlobals: expect idle $expect_idle, run $expect_run,  checks $expect_checks, total checks $total_checks\n");
};

sub WaitForIt {
    my $count = 0;
    my $looplimit = 7;
    my $variance = 10;
    my $sleeptime = 0;
    my $res = 0;
    my $final = 0;

	TLOG("entered ConcurrencyTools::WaitForIt\n");

    #my @catchstuff;
    while ($count < $looplimit) {
        #@catchstuff = {};
        $count += 1;
        $sleeptime = ($variance);
        sleep($sleeptime);
        TLOG("Loop $count in ConcurrencyTools::WaitForIt\n");

        #print "\tCurrent queue in WaitForIt\n";
        #CondorTest::runCondorTool("condor_q",\@catchstuff,2,{emit_output=>0});
        TLOG("Calling CheckStatus(final=$final)\n");
        $res = CheckStatus($final);
        TLOG("Result from CheckStatus: $res\n");
        if($res == 1) {
            print "\tWaitForIt got 1, we are really done\n";
            return(1);
        } elsif ($res == 2) {
            # wait on the next negotiator cycle to ensure
            # no more jobs start. Check one last time
            $final = 1;
            TLOG("Looking for the start of next negotiation cycle\n");
            DelayOnNegotiator();
            TLOG("BACK FROM Looking for the start of next negotiation cycle\n");
        } elsif ($res == -1) {
            print "\tWaitForIt got -1, something went wrong\n";
            return (-1);
        } elsif ($res == 0) {
            print "\tWe need more time in WaitForIt\n";
        }
        if( $count != $looplimit ) {
            #$sleeptime = ($count * $variance);
            print "\tsleep time set to $sleeptime\n";
        } else { 
            print "\tTimeout in WaitForIt\n";
            return(-1);
        }
    }
	TLOG("leaving ConcurrencyTools::WaitForIt\n");
};

# return 0 needs more time
# return -1 for failure
# return 1 for happpy

sub CheckStatus {
    my $amidone = shift;
    $running_now = CountRunning();
    print "\tSaw Running: $running_now, Idle: $idles; Expecting Running: $expect_run, Idle: $expect_idle\n";
    if($running_now > $expect_run) {
        # clearly unhappy
        print "Running jobs <$running_now> exceeded concurrency limits <$expect_run>\n";
        $expect_run = 1000; # remove will let a job start, bump count way up now.
        CondorTest::runToolNTimes("condor_rm -all",1,0);
        $done = 1;
        CondorTest::RegisterResult(0, "test_name", $testname);
        return(-1);
    }
    # compare current running to expected total
    if($running_now == $expect_run) {
        #print "Hit target for running jobs!\n";
        # if we expect no idle jobs, don't check.
        # remove jobs and return
        if($expect_idle == 0) {
            $done = 1;
            print "Expected idle 0 and run number met, remove jobs\n";
            CondorTest::runToolNTimes("condor_rm -all",1,0);
            #clearly done and happy
            return(1);
        } else {
			$idles = CountIdle($expect_idle);
            print "Running Now:$running_now ($expect_run) Idle:$idles ($expect_idle)\n";
            if($idles == $expect_idle) {
                $done = 1;
                print "Runs met and expected idle, About to remove these jobs\n";
                if($amidone == 1) {
                    $expect_run = 1000; # remove will let a job start, bump count way up now.
                    CondorTest::runToolNTimes("condor_q",1,0);
                    CondorTest::runToolNTimes("condor_rm -all",1,0);
                    return(1)
                } else {
                    return(2);
                }
            } else {
                print "Run and Idle counts off running:$running_now idle:idles\n";
                return(0);
            }
        }
    } else {
        #print "running $running_now expecting $expect_run: not removing jobs\n";
        return(0);
    }
};


#    JobStatus codes:
#         1 I IDLE
#         2 R RUNNING
#         3 X REMOVED
#         4 C COMPLETED
#         5 H HELD
#         6 > TRANSFERRING_OUTPUT
#         7 S SUSPENDED

sub CountRunning
{
    my $runcount = 0;
    my @goods = (); 

	TLOG("CountRunning: enter and get queue information\n");
    CondorTest::runCondorTool("condor_q -af:j JobStatus",\@goods,2,{emit_output => 1});
	TLOG("CountRunning: have queue information\n");
    foreach my $line (@goods) {
        #print "JOB: $line";
        if($line =~ /\s+2\s*$/) {
            $runcount += 1;
            #print "$runcount: $runcount\n";
        } else {
            #print "Parse error or Idle:$line";
        }   
    }   
	print "\tCountRunning: returning $runcount\n";
    return($runcount);
}

sub CountIdle
{
    my $expectidle = shift;
    my $idlecount = 0;
    my @goods = ();

    TLOG("In count Idle allow_too_few_idle_once=$allow_too_few_idle_once\n");
    #runcmd("condor_q");
    CondorTest::runCondorTool("condor_q -af:j JobStatus",\@goods,2,{emit_output => 1});
    foreach my $line (@goods) {
        #print "JOB: $line";
        if($line =~ /\s+1\s*$/) {
            $idlecount += 1;
            # print "Idle count now <$idlecount>, expecting <$expectidle>\n";
        }
    }
	print "\tIdle count now $idlecount, expecting $expectidle\n";
    if($allow_too_few_idle_once > 1) {
        # Case in point is a concurrency limit of one but two jobs
        # start. Triggering a fail on too few idle, could be failing
        # on a slow submit of the jobs. I'd rather fail on too many running
        # so the fist check gets a pass.
        # with sequential submits($burst = 0) submits happen much slower
        # and tolerance of only 1 not sufficient
        #$allow_too_few_idle_once = 0;
        $allow_too_few_idle_once -= 1;
    } else {
        if($idlecount != $expectidle) {
            CondorTest::runToolNTimes("condor_q -af:j JobStatus", 1, 1);
            die "Expected $expectidle idle but found $idlecount - die\n";
        }
    }

    return($idlecount);
}

sub ExamineSlots
{
    my $waitforit = shift;

    my $available = 0;
    my $looplimit = 20;
    my $count = 5; # go just once
    my @goods = ();
    if(defined $waitforit) {
        $count = 0; #enable looping with 10 second sleep
    }
    my $waitready = $waitforit;
    my $slottype = `condor_config_val SLOT_TYPE_1_PARTITIONABLE`;
    chomp($slottype);
    if($slottype =~ /Not defined/){
        print "Looking for static slots";
    } else {
        print "Looking for partitionable slots";
        $waitready = 1;
    }

    while($count <= $looplimit) {
        $count += 1;
        CondorTest::runCondorTool("condor_status -tot",\@goods,2,{emit_output => 0});
        foreach my $line (@goods) {
            if($line =~ /^\s*Total\s+(\d+)\s*(\d+)\s*(\d+)\s*(\d+).*/) {
                print "<$4> unclaimed <$1> Total slots\n";
                $available = $4;
            }
        }
        if(defined $waitforit) {
            if($available >= $waitready) {
                last;
            } else {
                sleep 10;
            }
        } else{
        }
    }
	if((defined $waitforit) && ($count == $looplimit)) {
		print "NOTE: loop limit reached while waiting for enough slots\n";
	}
	if($available == 1) {
		#could be a partionable slot
		if($slottype =~ /Not defined/){
    			return($available);
		}
		my $numcpus = `condor_config_val NUM_CPUS`;
		chomp($numcpus);
		my $cores = 0;
		if($numcpus =~ /(\d+)/) {
			$cores = $1;
			print "Dealing with case of one partionable slot\n";
			if($cores >= $waitforit) {
				return($waitforit);
			}
		}
	}
    return($available);
}

sub ExamineQueue
{
    TLOG("Expecting all jobs to be gone. Lets See.\n");
    my @goods = ();
    CondorTest::runCondorTool("condor_q -tot",\@goods,2);
    foreach my $line (@goods) {
        # print "\tJOB: $line";
        if($line =~ /^\s*(\d+)\s*jobs;\s+/) {
            if ($1 ne "0") {
               print "\t$1 jobs still in the queue\n";
            }
        }
    }
    TLOG("Total slots available here:\n\n");
    CondorTest::runToolNTimes("condor_status",1,0);
}

sub QueueMoreJobs
{
    my $submitfile = shift;
    my $taskname = shift;
    if($taskmorejobs{$taskname} > 0) {
        my $taskcount = $taskmorejobs{$taskname};
        #print "submitting $submitfile\n";
        #CondorTest::runToolNTimes("condor_submit $submitfile",1,0);
        my $pid = fork();

        if($pid == -1) {
            die "Fork error:$!\n";
        } elsif($pid == 0) {
            # we want the same callbacks!
            CondorTest::RunTest($testname,$submitfile,0);
            exit(0);
        } else {
            my $childpid = waitpid($pid,0);
            if($childpid == -1) {
                print "something went wrong waiting for child\n";
            } else {
                my $retval = $?;
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
            # parent gets pid of child fork
            # save pid
            $children{$pid} = 1;
        }
        $taskcount--;
        print "Additonal jobs wanted:$taskcount\n";
        if($taskcount == 0) {
            SetIdleTolerance($idletolerance);
        }
        $taskmorejobs{$taskname} = $taskcount;
    }
}

sub SetIdleTolerance {
 	#my $tolerance = shift;
 	#$allow_too_few_idle_once = $tolerance;
 	#print "Tolerance of no idle set:$allow_too_few_idle_once\n";
 };

sub DelayOnNegotiator {
    TLOG("Watching for another job to start up\n");
    # Started Negotiation Cycle A wait of about 10 seconds is caused here
    # leading the next one to occur in about 20 seconds
    #print scalar(localtime()) . " before multicheck\n";
    CondorLog::RunCheckMultiple(
        daemon => "Negotiator",
        match_regexp => "Started Negotiation Cycle",
        match_instances => 1,
        match_callback => $on_match,
        alt_timed => \&multi_timeout_callback,
        match_timeout => 80,
        match_new => "true",
        no_result => "true",
    );
    TLOG("After multicheck\n");
    # CondorTest::RemoveTimed(); done before returning from CondorLog::RunCheckMultiple
};

1;
