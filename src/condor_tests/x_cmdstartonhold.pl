#! /usr/bin/env perl
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

use CondorTest;


$cmd = 'x_cmdstartonhold.cmd';


# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

CondorTest::debug("Submit file for this test is $cmd\n",1);
CondorTest::debug("looking at env for condor config\n",1);
system("printenv | grep CONDOR_CONFIG");

$condor_config = $ENV{CONDOR_CONFIG};
CondorTest::debug("CONDOR_CONFIG = $condor_config\n",1);

$testname = 'Condor-C A & B test - vanilla U';

$submitted = sub {
	my %info = @_;
    $cluster = $info{"cluster"};

    my $qstat = CondorTest::getJobStatus($cluster);
    while($qstat == -1)
    {
        CondorTest::debug("Job status unknown - wait a bit\n",1);
        sleep 2;
        $qstat = CondorTest::getJobStatus($cluster);
    }

    CondorTest::debug("It better be on hold... status is $qstat(5 is correct)\n",1);
    if($qstat != HELD)
    {
        CondorTest::debug("Cluster $cluster failed to go on hold\n",1);
        exit(1);
    }
	CondorTest::debug("Job on hold submitted\n",1);
	exit(0);
};

$aborted = sub {
	my %info = @_;
	my $done;
	die "Abort event not expected!\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("Held event not expected.....\n",1);
	exit(1);
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("Start test timer from execution time\n",1);
	#CondorTest::RegisterTimed($testname, $timed, 1800);
};

$timed = sub
{
	die "Test took too long!!!!!!!!!!!!!!!\n";
};

$success = sub
{
	CondorTest::debug("Success: ok\n",1);
};

$release = sub
{
	CondorTest::debug("Release expected.........\n",1);
	my @adarray;
	my $status = 1;
	my $cmd = "condor_reschedule";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(1)
	}
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterSubmit( $testname, $submitted);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

