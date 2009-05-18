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


#$cmd = 'job_condorc_ab_van.cmd';
$cmd = $ARGV[0];

CondorTest::debug("Submit file for this test is $cmd\n",1);
CondorTest::debug("looking at env for condor config\n",1);

$condor_config = $ENV{CONDOR_CONFIG};
CondorTest::debug("CONDOR_CONFIG = $condor_config\n",1);

$testdesc =  'job_core_periodicremove_van';
$testname = "job_core_periodicremove_van";

$aborted = sub {
	my %info = @_;
	my $done;
	CondorTest::debug("Abort event\n",1);
	#die "Want to see only submit, release and successful completion events for periodic release test\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $holdreason = $info{"holdreason"};

	CondorTest::debug("Held event: $holdreason \n",1);
	system("condor_status -any -l");
	system("ps auxww | grep schedd");
	system("cat `condor_config_val LOG`/SchedLog");
	system("cat `condor_config_val LOG`/MasterLog");

	exit(1);
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
};

$timed = sub
{
	die "Test took too long!!!!!!!!!!!!!!!\n";
};

$success = sub
{
	my %info = @_;

	# Verify that output file contains expected "Done" line
	my $founddone - 0;
	$output = $info{"transfer_output_files"};
	open( OUTPUT, "< $output" );
	@output_lines = <OUTPUT>;
	close OUTPUT;
	foreach $res (@output_lines) {
		CondorTest::debug("res: $res\n",1);
		if($res =~ /^.*Done.*$/) {
			$founddone = 1;
			last;
		}
	}
	if( $founddone != 1 ) {
	    die "Output file $output is missing \"Done\"!\n";
	}

	# Verify that output file contains the contents of the
	# input file.
	$input = $info{"input"};
	open( INPUT, "< $input" );
	@input_lines = <INPUT>;
	close INPUT;
	for my $input_line (@input_lines) {
	    if( !grep($_ eq $input_line,@output_lines) ) {
		die "Output file is missing echoed input!\n";
	    }
	}

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
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterHold( $testname, $held );
#CondorTest::RegisterTimed($testname, $timed, 900);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

