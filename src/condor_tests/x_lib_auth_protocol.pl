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

#$cmd = 'lib_auth_protocol-negot.cmd';
$testdesc =  'Condor submit to test security negotiations';
$testname = "lib_auth_protocol";

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my %testerrors;
my %info;
my $cluster;

my $piddir = $ARGV[0];
my $subdir = $ARGV[1];
my $expectedres = $ARGV[2];
my $submitfile = $ARGV[3];
my $variation = $ARGV[4];

if(!(defined $variation)) {
	die "Must have variation to build real testname not just <<$testname>>\n";
} else {
	print "Variation for this test is <<$variation>>\n";
}

$testname = $testname . "-" . $variation;
$cmd = $submitfile;

CondorTest::debug("Submit file is $cmd\n",1);

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

	my @adarray;
	my $status = 1;
	my $cmd = "condor_q -debug";
	#$status = CondorTest::runCondorTool($cmd,\@adarray,2,"Security");
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status) {
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(1);
	} else {
		foreach $line (@adarray) {
			if( $line =~ /.*Authentication was a Success.*/ ) {
				CondorTest::debug("SWEET: client got Authentication as expected\n",1);
				return(0);
			}
		}
		CondorTest::debug("Bad: client did not get Authentication as expected\n",1);
		exit(1)
	}
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	CondorTest::debug("submitted: \n",1);
	{
		CondorTest::debug("Check authenticated user....\n",1);
	}
	my @adarray;
	my $status = 1;
	my $cmd = "condor_q -l $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status) {
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(1);
	} else {
		foreach $line (@adarray) {
			if( $line =~ /^Owner\s*=\s*(.*)\s*.*/ ) {
				CondorTest::debug("Owner is $1\n",1);
			} elsif ( $line =~ /^User\s*=\s*(.*)\s*.*/ ) {
				CondorTest::debug("User is $1\n",1);
			}
		}
	}
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	my $stat = CondorTest::SearchCondorLog( "SCHEDD", "Authentication was a Success" );
	if( $stat == 1 ) {
		CondorTest::debug("Good completion!!!\n",1);
	} else {
		die "Expected match for Authentication is a Success and could not find it in SchedLog\n";
	}

};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

