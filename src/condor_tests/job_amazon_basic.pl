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

#$cmd = 'job_amazon_basic.cmd';
my $cmd = $ARGV[0];
my $debuglevel = 2;

CondorTest::debug( "Submit file for this test is $cmd\n",$debuglevel);
CondorTest::debug( "looking at env for condor config\n",$debuglevel);

my $condor_config = $ENV{CONDOR_CONFIG};

CondorTest::debug("CONDOR_CONFIG = $condor_config\n",$debuglevel);

my $testdesc =  'Amazon EC2 basic test';
my $testname = "job_amazon_basic";

my $aborted = sub {
	my %info = @_;
	my $done;
	CondorTest::debug( "Abort event not expected \n",$debuglevel);
	die "Abort event not expected!\n";
};

my $held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $holdreason = $info{"holdreason"};

	CondorTest::debug( "Held event not expected: $holdreason \n",$debuglevel);
	system("condor_status -any -l");
	die "Amazon EC2 job being held not expected\n";
};

my $submit = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
	print "ok\n";
};

my $executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
};

# Expect 8 Return Succes exchanges including a CreateKeyPair
# and a DeleteKeyPair.
my $ec2simout = "ec2_sim.out";

my $success = sub
{
	my %info = @_;
	my $line = "";
	my $successcount = 0;
	my $keypaircount = 0;
	my $keyspairs = 2;
	my $successes = 8;

	print "Checking simulator output - ";

	open(EC2,"<$ec2simout") or die "Failed to open <$ec2simout>:$!\n";
	while(<EC2>) {
		chomp();
		$line = $_;
		if($line =~ /^CreateKeyPair/) {
			$keypaircount += 1;
		} elsif($line =~ /^DeleteKeyPair/) {
			$keypaircount += 1;
		} elsif($line =~ /^Return Success/) {
			$successcount += 1;
		} else {
			# ignore the rest
		}
	}
	close(EC2);
	if(($successcount == $successes) && ($keypaircount == $keyspairs)) {
		print "ok\n";
	} else {
		print "bad\n";
	}


	print "Job executed successfully\n";
	# Verify that output file contains expected "Done" line
};

CondorTest::RegisterSubmit( $testname, $submit );
CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug( "$testname: SUCCESS\n",$debuglevel);
	exit(0);
} else {
	CondorTest::debug( "$testname: FAILED\n",$debuglevel);
	die "$testname: CondorTest::RunTest() failed\n";
}

