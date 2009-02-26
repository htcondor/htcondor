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

$cmd      = 'job_filexfer_trans-excut-true_van.cmd';
$testdesc =  'Jobs complaining cause No FT on when tansfer_executables = true - vanilla U';
$testname = "job_filexfer_trans-excut_van";

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my $alreadydone=0;

$execute = sub {
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("Running $cluster\n",1);

};

$abort = sub {
	die "Job is gone now, cool.\n";
};

$hold = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	CondorTest::debug("Good. Cluster $cluster is supposed to be held.....\n",1);
};

$release = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

};

$success = sub
{
	die "Base file transfer job!\n";
};

$wanterror = sub
{
	CondorTest::debug("Base file transfer job, submit supposed to fail...error 1 ..!\n",1);
	my %args = @_;
	my $errmsg = $args{"ErrorMessage"};

    if($errmsg =~ /^.*died abnormally.*$/) {
        CondorTest::debug("BAD. Submit died was to fail but with error 1\n",1);
        CondorTest::debug("$testname: Failure\n",1);
        return(1);
    } elsif($errmsg =~ /^.*\(\s*returned\s*(\d+)\s*\).*$/) {
        if($1 == 1) {
            CondorTest::debug("Good. Job was not to submit with File Transfer off and input files requested\n",1);
            CondorTest::debug("$testname: SUCCESS\n",1);
            return(0);
        } else {
            CondorTest::debug("BAD. Submit was to fail but with error 1 not <<$1>>\n",1);
            CondorTest::debug("$testname: Failure\n",1);
            return(1);
        }
    } else {
            CondorTest::debug("BAD. Submit failure mode unexpected....\n",1);
            CondorTest::debug("$testname: Failure\n",1);
            return(1);
    }
};

$timed = sub
{
	die "Job should have ended by now. file transfer broken!\n";
};

# make some needed files. All 0 sized and xxxxxx.txt for
# easy cleanup

system("touch submit_filetrans_input.txt");
system("touch submit_filetrans_inputa.txt");
system("touch submit_filetrans_inputb.txt");
#system("touch submit_filetrans_inputc.txt");

CondorTest::RegisterExecute($testname, $execute);
CondorTest::RegisterWantError($testname, $wanterror);
CondorTest::RegisterAbort($testname, $abort);
CondorTest::RegisterHold($testname, $hold);
CondorTest::RegisterRelease($testname, $release);
CondorTest::RegisterExitedSuccess($testname, $success);
CondorTest::RegisterTimed($testname, $timed, 3600);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

