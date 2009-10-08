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
use strict;
use warnings;


my $debuglevel = 1;

my $pid = $ARGV[0];


my $testdesc =  'Condor-C A & B test - vanilla U';
my $testname = "x_cmdstartonhold";
my $cmd = "x_cmdstartonhold$pid.cmd";
my $template = 'x_cmdstartonhold.template';

my $line = "";
open(TEMPLATE,"<$template") or die "Can not open $template: $!\n";
open(CMD,">$cmd") or die "Can not open $cmd: $!\n";
while(<TEMPLATE>) {
	chomp();
	$line = $_;
	if($line =~ /^\s*log\s*=.*$/) {
		print CMD "log = $testname$pid.log\n";
	} elsif($line =~ /^\s*error\s*=.*$/) {
		print CMD "error = $testname$pid.err\n";
	} elsif($line =~ /^\s*output\s*=.*$/) {
		print CMD "output = $testname$pid.out\n";
	} else {
		print CMD "$line\n";
	}
}
close(TEMPLATE);
close(CMD);


# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

CondorTest::debug("Submit file for this test is $cmd\n",$debuglevel);

my $condor_config = $ENV{CONDOR_CONFIG};
CondorTest::debug("CONDOR_CONFIG = $condor_config\n",$debuglevel);

my $submitted = sub {
	my %info = @_;
    my $cluster = $info{"cluster"};
	my $qstat;

	$qstat = CondorTest::getJobStatus($cluster);
    while($qstat == -1)
    {
        CondorTest::debug("Job status unknown - wait a bit\n",$debuglevel);
        sleep 2;
        $qstat = CondorTest::getJobStatus($cluster);
    }

    CondorTest::debug("It better be on hold... status is $qstat(5 is correct)\n",$debuglevel);
    if($qstat != HELD)
    {
        CondorTest::debug("Cluster $cluster failed to go on hold\n",$debuglevel);
        exit(1);
    }
	CondorTest::debug("Job on hold submitted\n",$debuglevel);
	exit(0);
};

my $aborted = sub {
	my %info = @_;
	my $done;
	die "Abort event not expected!\n";
};

my $held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("Held event not expected.....\n",$debuglevel);
	exit(1);
};

my $executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("Start test timer from execution time\n",$debuglevel);
	#CondorTest::RegisterTimed($testname, $timed, 1800);
};

my $timed = sub
{
	die "Test took too long!!!!!!!!!!!!!!!\n";
};

my $success = sub
{
	CondorTest::debug("Success: ok\n",$debuglevel);
};

my $release = sub
{
	CondorTest::debug("Release expected.........\n",$debuglevel);
	my @adarray;
	my $status = 1;
	my $cmd = "condor_reschedule";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",$debuglevel);
		exit(1)
	}
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterSubmit( $testname, $submitted);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",$debuglevel);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

