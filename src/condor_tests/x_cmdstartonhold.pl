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

Condor::DebugOff();

$cmd = 'x_cmdstartonhold.cmd';


# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

print "Submit file for this test is $cmd\n";
print "looking at env for condor config\n";
system("printenv | grep CONDOR_CONFIG");

$condor_config = $ENV{CONDOR_CONFIG};
print "CONDOR_CONFIG = $condor_config\n";

$testname = 'Condor-C A & B test - vanilla U';

$submitted = sub {
	my %info = @_;
    $cluster = $info{"cluster"};

    my $qstat = CondorTest::getJobStatus($cluster);
    while($qstat == -1)
    {
        print "Job status unknown - wait a bit\n";
        sleep 2;
        $qstat = CondorTest::getJobStatus($cluster);
    }

    print "It better be on hold... status is $qstat(5 is correct)\n";
    if($qstat != HELD)
    {
        print "Cluster $cluster failed to go on hold\n";
        exit(1);
    }
	print "Job on hold submitted\n";
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

	print "Held event not expected.....\n";
	exit(1);
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	print "Start test timer from execution time\n";
	#CondorTest::RegisterTimed($testname, $timed, 1800);
};

$timed = sub
{
	die "Test took too long!!!!!!!!!!!!!!!\n";
};

$success = sub
{
	print "Success: ok\n";
};

$release = sub
{
	print "Release expected.........\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_reschedule";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterSubmit( $testname, $submitted);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

