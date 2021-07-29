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

package CondorCmdStatusWorker2;

use strict;
use warnings;
use Cwd;
use CondorTest;
use CondorUtils;
use Check::SimpleJob;

my $debuglevel = 2;

my $firstappend_condor_config = '
	DAEMON_LIST = MASTER,SCHEDD,COLLECTOR,NEGOTIATOR,STARTD
	MAX_NEGOTIATOR_LOG = 5000000000
	MAX_SCHEDD_LOG = 5000000000
	STARTD_NAME = master_local
	MASTER_NAME = master_local
	SCHEDD_NAME = master_local
	WANT_SUSPEND = False
	KILL = FALSE
	NUM_CPUS = 2
	MASTER_UPDATE_INTERVAL = 2
';

my $secondappend_condor_config = '
	SHARED_PORT_PORT = 0
	DAEMON_LIST = MASTER,SCHEDD,STARTD
	MAX_SCHEDD_LOG = 5000000000
	WANT_SUSPEND = False
	KILL = FALSE
	SCHEDD_NAME = master_remote
	STARTD_NAME = master_remote
	MASTER_NAME = master_remote
	NUM_CPUS = 4
	MASTER_UPDATE_INTERVAL = 2
';


BEGIN
{
}

sub reset
{
}

# truly const variables in perl
sub NOINFO{0};
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my $on_abort = sub {
	print "removal expected\n";
};

my $on_evictedwithoutcheckpoint = sub {
};



sub SetUp
{
	my $testname = shift;
	my $cmd;
	my $cmdstatus;
	my @adarray;

	print "CondorCmdStatusWorker2::SetUp - doing common setup for all cmd_status_* tests : start 2 condor nodes and run 2 jobs\n";
	my $configfile = CondorTest::CreateLocalConfig($firstappend_condor_config,"statusworker1");

	my $new_condor = CondorTest::StartCondorWithParams(
		condor_name => "worker1",
		fresh_local => "TRUE",
		test_name => "$testname",
		condorlocalsrc => "$configfile",
	);

	CondorTest::SetTestName($testname);

	my $pool1 = $new_condor->GetCondorConfig();
	my $collectorport = $new_condor->GetCollectorAddress();
	my $masterpid1 = $new_condor->GetMasterPid();
	print "CondorCmdStatusWorker2::SetUp - Created condor node 'worker1'. MasterPID=$masterpid1, CollectorAddr=$collectorport\n";
	CondorTest::debug("Primary collector for other nodes = $collectorport\n",$debuglevel);

	# Start second worker node
	#
	my $primarycollector = $collectorport;
	my $configfile2 = CondorTest::CreateLocalConfig($secondappend_condor_config,"statusworker2","COLLECTOR_HOST = $primarycollector");

	my $saveconfig = $ENV{CONDOR_CONFIG};
	$ENV{CONDOR_CONFIG} = $pool1;

	my $done = 0;

	my $new_condor2 = CondorTest::StartCondorWithParams(
		condor_name => "worker2",
		fresh_local => "TRUE",
		test_name => "$testname",
		condorlocalsrc => "$configfile2",
	);

	my $pool2 = $new_condor2->GetCondorConfig();
	my $masterpid2 = $new_condor2->GetMasterPid();
	print "CondorCmdStatusWorker2::SetUp - Created submit/exec node 'worker2'. MasterPID=$masterpid2\n";

	CondorTest::SetTestName($testname);

	$ENV{CONDOR_CONFIG} = $pool2;

	# start two jobs which run till killed

	print "CondorCmdStatusWorker2::SetUp - submitting 2 infinite sleep jobs to 'worker2'\n";

	my $limit = 120;

	SimpleJob::RunCheck(
		no_wait => 1,
		duration => 3600,
		on_abort => $on_abort,
		on_evictedwithoutcheckpoint => $on_evictedwithoutcheckpoint,
	);

	SimpleJob::RunCheck(
		no_wait => 1,
		duration => 3600,
		on_abort => $on_abort,
		on_evictedwithoutcheckpoint => $on_evictedwithoutcheckpoint,
	);

	print "CondorCmdStatusWorker2::SetUp - Waiting $limit sec for job 2.0 from 'worker2' to start running.\n";

	my $qstat = CondorTest::getJobStatus(2.0);
	CondorTest::debug("remote cluster 2.0 status is $qstat\n",$debuglevel);
	my $counter = 0;
	while($qstat != RUNNING)
	{
		CondorTest::debug("remote Job status 2.0 not RUNNING - wait a bit\n",$debuglevel);
		if($counter >= $limit) {
			die "CondorCmdStatusWorker2::SetUp - gave up after $limit seconds waiting for Job 2.0 from 'worker2' to run\n";
		} else {
			$counter += 4;
		}
		sleep 4;

		$qstat = CondorTest::getJobStatus(2.0);
	}
	print "CondorCmdStatusWorker2::SetUp - job 2.0 from 'worker2' is running\n";

	print "CondorCmdStatusWorker2::SetUp - Waiting $limit sec for job 1.0 from 'worker2' running\n";
	$qstat = CondorTest::getJobStatus(1.0);
	CondorTest::debug("remote cluster 1.0 status is $qstat\n",$debuglevel);
	$counter = 0;
	while($qstat != RUNNING)
	{
		CondorTest::debug("remote Job status 1.0 not RUNNING - wait a bit\n",$debuglevel);
		sleep 4;
		$qstat = CondorTest::getJobStatus(1.0);
		if($counter >= $limit) {
			die "CondorCmdStatusWorker2::SetUp - gave up after $limit seconds waiting for Job 1.0 to run\n";
		} else {
			$counter += 4;
		}
	}
	print "CondorCmdStatusWorker2::SetUp - job 1.0 from 'worker2' is running\n";
	print "CondorCmdStatusWorker2::SetUp - 'worker2' pool steady and ready\n";

	# submit into collector node schedd
	#print "Changing from: $pool2 to:$pool1\n";
	$ENV{CONDOR_CONFIG} = $pool1;

	print "Lets look at status from first pool....\n";
	system("condor_status;condor_q");

	SimpleJob::RunCheck(
		no_wait => 1,
		duration => 3600,
		on_abort => $on_abort,
		on_evictedwithoutcheckpoint => $on_evictedwithoutcheckpoint,
	);

	SimpleJob::RunCheck(
		no_wait => 1,
		duration => 3600,
		on_abort => $on_abort,
		on_evictedwithoutcheckpoint => $on_evictedwithoutcheckpoint,
	);

	print "Wait for jobs running on local schedd.\n";
	print "Wait for job 2.0 to be running\n";
	$qstat = CondorTest::getJobStatus(2.0);
	CondorTest::debug("local cluster 2.0 status is $qstat\n",$debuglevel);
	$counter = 0;
	while($qstat != RUNNING)
	{
		CondorTest::debug("local Job status 2.0 not RUNNING - wait a bit\n",$debuglevel);
		sleep 4;
		$qstat = CondorTest::getJobStatus(2.0);
		if($counter >= $limit) {
			die "local Job status 2.0 failed 6 minutes to running test\n";
		} else {
			$counter += 4;
		}
	}
	print "Job 2 running - ok\n";

	print "Wait for job 1.0 to be running\n";
	$qstat = CondorTest::getJobStatus(1.0);
	CondorTest::debug("local cluster 1.0 status is $qstat\n",$debuglevel);
	$counter = 0;
	while($qstat != RUNNING)
	{
		CondorTest::debug("local Job status 1.0 not RUNNING - wait a bit\n",$debuglevel);
		sleep 4;
		$qstat = CondorTest::getJobStatus(1.0);
		if($counter >= $limit) {
			die "local Job status 1.0 failed 6 minutes to running test\n";
		} else {
			$counter += 4;
		}
	}
	print "job 1 running - ok\n\n";

	 # allow time for all the nodes to update the collector
    # by allowing N attempts
    my $nattempts = 8;
    my $count = 0;

	print "CondorCmdStatusWorker2: local pool steady and ready\n";

	print "Looking for expected number of startds(6) - ";
    CondorTest::debug("Looking at new pool<condor_status>\n",$debuglevel);
    while($count < $nattempts) {
		print "startd check loop $count\n";
    my $masterlocal = 0;
    my $mastersched = 0;
        my $found1 = 0;
        $cmd = "condor_status";
        $cmdstatus = CondorTest::runCondorTool($cmd,\@adarray,2);
        if(!$cmdstatus)
        {
            print "bad\n";
			return("");
            CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",$debuglevel);
        }

        CondorTest::debug("Looking at condor_status \n",$debuglevel);

        foreach my $line (@adarray) {
            #print "$line\n";
            if($line =~ /^.*master_loc.*$/) {
                CondorTest::debug("found masterLocal: $line\n",$debuglevel);
                $masterlocal = $masterlocal + 1;;
            } elsif($line =~ /^.*master_rem.*$/) {
                CondorTest::debug("found master_schedd: $line\n",$debuglevel);
                $mastersched = $mastersched + 1;;
            } else {
                #print "$line\n";
            }
        }

		# masterschedd is pool 2 - remote from collector schedd
        if(($masterlocal == 2) && ($mastersched == 4)) {
            $done = 1;
            print " Found 2 collecter pool startds and four remote startds - ok\n";
            CondorTest::debug("Found expected number of startds(6)\n",$debuglevel);
            CondorTest::debug("Found 2 on local collector and 4 on alternate node in pool\n",$debuglevel);
            last;
        } else {
            CondorTest::debug("Keep going masterlocal is $masterlocal and mastersched is $mastersched\n",$debuglevel);
        }

        $count = $count + 1;
        sleep($count * 5);
    }

	my $configreturn = $pool1 . "&" . $pool2;

	print "CondorCmdStatusWorker2::SetUp  done\n";

	if($done != 1) {
		return("");
	} else {
		#print "returning <<$configreturn>>\n";
		return($configreturn);
	}
}

sub IsSystemStable
{
	# allow time for all the nodes to update the collector
    # by allowing N attempts
    # with 4 run forever jobs we are not stable until -claimed finds exactly 4
    my $nattempts = 8;
    my $count = 0;
    my $done = 0;
	my @adarray;

	print "Is system stable - ";
    while($count < $nattempts) {
        my $cmd = "condor_status -claimed -format \"%s\\n\" name";
        CondorTest::debug("Looking for exactly 4 claimed slots\n",$debuglevel);
        my $cmdstatus = CondorTest::runCondorTool($cmd,\@adarray,2);
        if(!$cmdstatus)
        {
            CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",$debuglevel);
            exit(1)
        }

        # 4 are busy so 4 should be claimed....
        # yes but some could be evicted so settle for 2 or more

        my $numclaimed = 0;
        foreach my $line (@adarray) {
            if($line =~ /^\s*slot(\d+)[@]master.*@.*$/) {
                #print "found claimed slot: $line\n";
                $numclaimed = $numclaimed + 1;;
                CondorTest::debug("found claimed slot: $numclaimed of 4\n",$debuglevel);
            } else {
                #print "skip: $line\n";
            }
        }

        if($numclaimed >= 2) {
            CondorTest::debug("Condor_status -claimed found the expected 4 slots\n",$debuglevel);
			print "ok\n";
            $done = 1;
            last;
        } else {
            CondorTest::debug("Condor_status -claimed found incorrect claimed slot count<$numclaimed>\n",$debuglevel);
        }

		$count = $count + 1;
        sleep($count * 5);
    }
	if($done == 0) {
		print "bad\n";
	}
	return($done)
}

1;
