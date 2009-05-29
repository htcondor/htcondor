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

package CondorCmdStatusWorker;

use strict;
use warnings;
use Cwd;
use CondorTest;

my $debuglevel = 2;


BEGIN
{
}

sub reset
{
}

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

sub SetUp
{
	my $testname = shift;
	my $cmd;
	my $cmdstatus;
	my @adarray;

	print "worker for test $testname\n";
	my $currenthost = CondorTest::getFqdnHost();
	chomp($currenthost);
	my $primarycollector = $currenthost;

	print "Start up the local pool with collector.\n";
	my $version = "local";
	# get a local scheduler running (side a)
	my $configloc = CondorTest::StartPersonal( "$testname", "x_param.cmdstatus-multi" ,$version);
	my @local = split /\+/, $configloc;
	my $locconfig = shift @local;
	my $locport = shift @local;

	print "ok\n";
	CondorTest::debug("---local config is $locconfig and local port is $locport---\n",$debuglevel);

	$primarycollector = $primarycollector . ":" . $locport;

	print "Start up additonal schedd with startd.\n";
	CondorTest::debug("Primary collector for other nodes <<$primarycollector>>\n",$debuglevel);

	my $saveconfig = $ENV{CONDOR_CONFIG};
	$ENV{CONDOR_CONFIG} = $locconfig;
	CondorTest::debug("New collector is this:\n",$debuglevel);
	system("condor_config_val COLLECTOR_HOST");
	$ENV{CONDOR_CONFIG} = $saveconfig;

	my $line;
	open(SCHEDDSRC,"<x_param.cmdstatus-multi.template") || die "Could not open x_param.cmdstatus-multi.template: $!\n";
	open(SCHEDDONE,">x_param.$testname") || die "Could not open x_param.$testname: $!\n";
	while(<SCHEDDSRC>) {
		$line = $_;
		chomp($line);
		if($line =~ /^\s*collector\s*=.*$/) {
			print SCHEDDONE "collector  = $primarycollector\n";
		} else {
			print SCHEDDONE "$line\n";
		}
	}
	close(SCHEDDSRC);
	close(SCHEDDONE);

	$version = "scheddone";
	# get another node running
	my $configscheddone = CondorTest::StartPersonal( "$testname", "x_param.$testname" ,$version);
	my @scheddone = split /\+/, $configscheddone;
	my $scheddoneconfig = shift @scheddone;
	my $scheddoneport = shift @scheddone;

	print "ok\n";
	CondorTest::debug("---scheddone config is $scheddoneconfig and scheddone port is $scheddoneport---\n",$debuglevel);

	my $done = 0;

	# submit into scheddone
	$ENV{CONDOR_CONFIG} = $scheddoneconfig;

	# start two jobs which run till killed
	$cmd = "condor_submit ./x_cmdrunforever.cmd2";
	$cmdstatus = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$cmdstatus)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",$debuglevel);
		exit(1)
	}

	print "Wait for jobs running on remote schedd.\n";
	print "Wait for job 1.1 to be running - ";
	my $qstat = CondorTest::getJobStatus(1.1);
	CondorTest::debug("remote cluster 1.1 status is $qstat\n",$debuglevel);
	while($qstat != RUNNING)
	{
		CondorTest::debug("remote Job status 1.1 not RUNNING - wait a bit\n",$debuglevel);
		sleep 4;
		$qstat = CondorTest::getJobStatus(1.1);
	}
	print "ok\n";

	print "Wait for job 1.0 to be running - ";
	$qstat = CondorTest::getJobStatus(1.0);
	CondorTest::debug("remote cluster 1.0 status is $qstat\n",$debuglevel);
	while($qstat != RUNNING)
	{
		CondorTest::debug("remote Job status 1.0 not RUNNING - wait a bit\n",$debuglevel);
		sleep 4;
		$qstat = CondorTest::getJobStatus(1.0);
	}
	print "ok\n\n";

	# submit into collector node schedd
	$ENV{CONDOR_CONFIG} = $locconfig;

	#print "Lets look at status from first pool....\n";
	#system("condor_status");

	# start two jobs which run till killed
	$cmd = "condor_submit ./x_cmdrunforever.cmd2";
	$cmdstatus = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$cmdstatus)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",$debuglevel);
		exit(1)
	}

	print "Wait for jobs running on local schedd.\n";
	print "Wait for job 1.1 to be running - ";
	$qstat = CondorTest::getJobStatus(1.1);
	CondorTest::debug("local cluster 1.1 status is $qstat\n",$debuglevel);
	while($qstat != RUNNING)
	{
		CondorTest::debug("local Job status 1.1 not RUNNING - wait a bit\n",$debuglevel);
		sleep 4;
		$qstat = CondorTest::getJobStatus(1.1);
	}
	print "ok\n";

	print "Wait for job 1.0 to be running - ";
	$qstat = CondorTest::getJobStatus(1.0);
	CondorTest::debug("local cluster 1.0 status is $qstat\n",$debuglevel);
	while($qstat != RUNNING)
	{
		CondorTest::debug("local Job status 1.0 not RUNNING - wait a bit\n",$debuglevel);
		sleep 4;
		$qstat = CondorTest::getJobStatus(1.0);
	}
	print "ok\n\n";

	 # allow time for all the nodes to update the collector
    # by allowing N attempts
    my $nattempts = 8;
    my $count = 0;

	print "Looking for expected number of startds(6) - ";
    CondorTest::debug("Looking at new pool<condor_status>\n",$debuglevel);
    while($count < $nattempts) {
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
            } elsif($line =~ /^.*master_sch.*$/) {
                CondorTest::debug("found master_schedd: $line\n",$debuglevel);
                $mastersched = $mastersched + 1;;
            } else {
                #print "$line\n";
            }
        }

        if(($masterlocal == 2) && ($mastersched == 4)) {
            $done = 1;
            print "ok\n";
            CondorTest::debug("Found expected number of startds(6)\n",$debuglevel);
            CondorTest::debug("Found 2 on local collector and 4 on alternate node in pool\n",$debuglevel);
            last;
        } else {
            CondorTest::debug("Keep going masterlocal is $masterlocal and mastersched is $mastersched\n",$debuglevel);
        }

        $count = $count + 1;
        sleep($count * 5);
    }

	my $configreturn = $locconfig . "&" . $scheddoneconfig;

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
