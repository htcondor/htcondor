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

$cmd = 'lib_auth_protocol-negot.cmd';
$testdesc =  'Condor submit to test security negotiations';
$testname = "lib_auth_protocol-negot";

my $killedchosen = 0;

my %securityoptions =
(
"NEVER" => "1",
"OPTIONAL" => "1",
"PREFERRED" => "1",
"REQUIRED" => "1",
);

CondorTest::LoadExemption("lib_auth_protocol-negot,no,SECMAN:2004:Failed to create security session to");
CondorTest::LoadExemption("lib_auth_protocol-negot,no,SECMAN:2004:Was waiting for TCP auth session to");
CondorTest::LoadExemption("lib_auth_protocol-negot,no,FAILED TO SEND INITIAL KEEP ALIVE TO OUR PARENT");
CondorTest::LoadExemption("lib_auth_protocol-negot,no,DC_AUTHENTICATE unable to receive auth_info!");
CondorTest::LoadExemption("lib_auth_protocol-negot,no,SECMAN:2003:TCP auth connection to");

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my %testerrors;
my %info;
my $cluster;

my $piddir = $ARGV[0];
my $subdir = $ARGV[1];
my $expectedres = "";
$expectedres = $ARGV[2];

CondorTest::debug("Handed args from main test loop.....<<<<<<<<<<<<<<<<<<<<<<$piddir/$subdir/$expectedres>>>>>>>>>>>>>>\n",1);
$abnormal = sub {
	my %info = @_;

	die "Do not want to see abnormal event\n";
};

$aborted = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	die "Do not want to see aborted event\n";
};

$held = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	die "Do not want to see aborted event\n";
};

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	CondorTest::debug("Good. for on_exit_hold cluster $cluster must run first\n",1);
	my @adarray;
	my $status = 1;
	my $cmd = "condor_q -debug";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status) {
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(1);
	} else {
		foreach $line (@adarray) {
			#print "Client: $line\n";
			if( $line =~ /.*KEYPRINTF.*/ ) {
				if( $expectedres eq "yes" ) {
					CondorTest::debug("SWEET: client got key negotiation as expected\n",1);
				} elsif($expectedres eq "no") {
					CondorTest::debug("Client: $line\n",1);
					CondorTest::debug("BAD!!: client got key negotiation as NOT!!! expected\n",1);
				} elsif($expectedres eq "fail") {
					CondorTest::debug("Client: $line\n",1);
					CondorTest::debug("BAD!!: client got key negotiation as NOT!!! expected\n",1);
				}
			}
		}
	}
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("submitted: \n",1);
	{
		CondorTest::debug("good job $job expected submitted.\n",1);
	}
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("Good completion!!!\n",1);
	my $found = CondorTest::PersonalPolicySearchLog( $piddir, $subdir, "Encryption", "SchedLog");
	CondorTest::debug("PersonalPolicySearchLog found $found\n",1);
	if( $found eq $expectedres ) {
		CondorTest::debug("Good completion!!! found $found\n",1);
	} else {
		die "Expected <<$expectedres>> but found <<$found>>\n";
	}

};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	if( $expectedres eq "fail" ) {
		die "$testname: Test FAILED: ran but was supposed to fail\n";
	} else {
		CondorTest::debug("$testname: SUCCESS\n",1);
		exit(0);
	}
} else {
	if( $expectedres eq "fail" ) {
		CondorTest::debug("$testname: SUCCESS (failures were expected!)\n",1);
		exit(0);
	} else {
		die "$testname: CondorTest::RunTest() failed\n";
	}
}

