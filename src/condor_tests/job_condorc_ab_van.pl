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

#$cmd = 'job_condorc_ab_van.cmd';
my $cmd = $ARGV[0];

my $testdesc =  'Condor-C A & B test - vanilla U';
my $testname = $ARGV[1];
my $debuglevel = 2;

CondorTest::debug("Submit file for this test is $cmd\n",$debuglevel);
CondorTest::debug("looking at env for condor config\n",$debuglevel);

my $condor_config = $ENV{CONDOR_CONFIG};
CondorTest::debug("CONDOR_CONFIG = $condor_config\n",$debuglevel);

my $aborted = sub {
	my %info = @_;
	my $done;
	CondorTest::debug("Abort event not expected!\n",1);
	#die "Want to see only submit, release and successful completion events for periodic release test\n";
};

my $held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $holdreason = $info{"holdreason"};

	CondorTest::debug("Held event not expected: $holdreason \n",1);
	system("condor_status -any -l");
	system("ps auxww | grep schedd");
	system("cat `condor_config_val LOG`/SchedLog");
	system("cat `condor_config_val LOG`/MasterLog");

	exit(1);
};

my $executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
	print "Condor-C job is currently executing.\n";
};

my $success = sub
{
	my %info = @_;
	print "Condor-C completed, checking results - ";
	# Verify that output file contains expected "Done" line
	my $founddone = 0;
	my $output = $info{"transfer_output_files"};
	open( OUTPUT, "< $output" );
	CondorTest::debug("Opening output file <$output>\n",$debuglevel);
	my @output_lines = <OUTPUT>;
	my @chomped_output;
	close OUTPUT;
	foreach my $res (@output_lines) {
		CondorTest::fullchomp($res);
		push @chomped_output, $res;
		CondorTest::debug("res: $res\n",$debuglevel);
		if($res =~ /^.*Done.*$/) {
			$founddone = 1;
			last;
		}
	}
	if( $founddone != 1 ) {
		print "bad\n";
	    die "Output file $output is missing \"Done\"!\n";
	}

	# chomp results
	my @chomped_output;
	print "Result File:\n";
	foreach my $res (@output_lines) {
		CondorTest::fullchomp($res);
		push  @chomped_output, $res;
		print "out: <$res>\n";
	}
	# Verify that output file contains the contents of the
	# input file.
	my $input = $info{"transfer_input_files"};
	CondorTest::debug("Opening input file <$input>\n",$debuglevel);
	open( INPUT, "< $input" );
	my @input_lines = <INPUT>;
	close INPUT;
	for my $input_line (@input_lines) {
		CondorTest::fullchomp($input_line);
		CondorTest::debug("checking for this entry in output:<$input_line>\n",$debuglevel);
	    if( !grep($_ eq $input_line,@chomped_output) ) {
			print "bad\n";
			die "Output file is missing echoed input!\n";
	    }
	}
	print "ok\n";

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
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",$debuglevel);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

