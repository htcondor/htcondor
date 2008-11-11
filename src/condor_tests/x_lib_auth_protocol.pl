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
$testname = 'Condor submit to test security negotiations';

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
$cmd = $submitfile;

print "Submit file is $cmd\n";

print "Handed args from main test loop.....<<<<<<<<<<<<<<<<<<<<<<$piddir/$subdir/$expectedres>>>>>>>>>>>>>>\n";
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
	$status = CondorTest::runCondorTool($cmd,\@adarray,2,"Security");
	if(!$status) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1);
	} else {
		foreach $line (@adarray) {
			if( $line =~ /.*Authentication was a Success.*/ ) {
				print "SWEET: client got Authentication as expected\n";
				return(0);
			}
		}
		print "Bad: client did not get Authentication as expected\n";
		exit(1)
	}
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "submitted: \n";
	{
		print "Check authenticated user....\n";
	}
	my @adarray;
	my $status = 1;
	my $cmd = "condor_q -l $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1);
	} else {
		foreach $line (@adarray) {
			if( $line =~ /^Owner\s*=\s*(.*)\s*.*/ ) {
				print "Owner is $1\n";
			} elsif ( $line =~ /^User\s*=\s*(.*)\s*.*/ ) {
				print "User is $1\n";
			}
		}
	}
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	my $stat = PersonalSearchLog( $piddir, $subdir, "Authentication was a Success", "SchedLog");
	if( $stat == 0 ) {
		print "Good completion!!!\n";
	} else {
		die "Expected match for Authentication is a Success and could not find it in SchedLog\n";
	}

};

sub PersonalSearchLog 
{
	my $pid = shift;
	my $personal = shift;
	my $searchfor = shift;
	my $logname = shift;

	my $logloc = $pid . "/" . $pid . $personal . "/log/" . $logname;
	print "Search this log <$logloc> for <$searchfor>\n";
	open(LOG,"<$logloc") || die "Can not open logfile<$logloc>: $!\n";
	while(<LOG>) {
		if( $_ =~ /$searchfor/) {
			print "FOUND IT! $_";
			return(0);
		}
	}
	return(1);
}

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

