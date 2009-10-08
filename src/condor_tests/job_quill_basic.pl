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


$cmd      = 'job_quill_basic.cmd';
$testdesc =  'Verify same data from schedd and rdbms';
$testname = "job_quill_basic";

sub HELD{5};
sub RUNNING{2};

my $alreadydone=0;
my $result;

my $submitcount = 0;
my $releasehandled = "no";
my $successhandled = "no";
my $aborthandled = "no";


#######################
#
# compare_3arrays_byrows
#
#   We examine each line from an array and verify that each array has the same contents
#   by having a value for each key equalling the number of arrays. First used to
#   compare output from condor_q -direct quilld, schedd and rdbms

sub compare_3arrays_byrows
{
	$location = 0;

	my $harray1, $harray2, $harray3;
	$harray1 = shift;
	$harray2 = shift;
	$harray3 = shift;
	$href = shift;
	$delayedfail = 0;

	Condor::DebugOn();
	Condor::DebugLevel(1);
	CondorTest::debug("Make sure they all have the same number of entries\n",1);

	my $count1, $count2, $count3;

	$count1 = $#{$harray1};
	$count2 = $#{$harray2};
	$count3 = $#{$harray3};

	if ( ($count1 == $count2) && ($count1 == $count3)) {
		CondorTest::debug("Good the sizes<<$count1>> match up!!!!\n",1);
	} else {
		CondorTest::debug("Compare failed because $count1, $count2 and $count3 are not the same\n",1);
		$delayedfail = 1;
		#return(1);
	}

	$current = "";

	#print "Skip items follow:\n";
	#while (($key, $value) = each %{$href}) {
		#print "$key => $value\n";
	#}

	while( $location < ($count1 + 1)) {
		# look at current item
		$current = ${$harray1}[$location];
		$current =~ /^\s*(\w*)\s*=\s*(.*)\s*$/;
		$thiskey = $1;
		#print "Current<<$current>> Thiskey<<$thiskey>>\n";
		#print "${$href}{$thiskey}\n";
		if($current =~ /^.*Submitter.*$/) {
			#print "Skip Submitter entry\n";
		} elsif( exists ${$href}{$thiskey}) {
			#print "Skip compare of -$thiskey-\n";
		} else {
			if((${$harray1}[$location] eq ${$harray2}[$location]) && (${$harray1}[$location] eq ${$harray3}[$location])) {
				#print "Good match:${$harray1}[$location] at entry $location\n";
			} else {
				CondorTest::debug("Match FAIL position <<$location>>: 1: ${$harray1}[$location] 2: ${$harray2}[$location] 3: ${$harray3}[$location]\n",1);
				#print "Match FAIL position <<$location>>: 1: ${$harray1}[$location] 2: ${$harray2}[$location] 3: ${$harray3}[$location]\n";
				return(1);
			}
		}

		$location = $location + 1;
		print "Location now <<$location>>\n";
	}

	return(0);
};

#######################
#
# compare_2arrays_byrows
#
#   We examine each line from an array and verify that each array has the same contents
#   by having a value for each key equalling the number of arrays. First used to
#   compare output from condor_q -direct quilld, schedd and rdbms

sub compare_2arrays_byrows
{
	$location = 0;

	my $harray1, $harray2;
	$harray1 = shift;
	$harray2 = shift;
	$href = shift;
	$delayedfail = 0;

	CondorTest::debug("Make sure they all have the same number of entries\n",1);

	my $count1, $count2;

	$count1 = $#{$harray1};
	$count2 = $#{$harray2};

	if($count1 == $count2){
		CondorTest::debug("Good the sizes<<$count1>> match up!!!!\n",1);
	} else {
		CondorTest::debug("Compare failed because $count1 and $count2 are not the same\n",1);
		$delayedfail = 1;
		#return(1);
	}

	$current = "";

	CondorTest::debug("Skip items follow:\n",1);
	while (($key, $value) = each %{$href}) {
		CondorTest::debug("$key => $value\n",1);
	}

	while( $location < ($count1 + 1)) {
		# look at current item
		$current = ${$harray1}[$location];
		$current =~ /^\s*(\w*)\s*=\s*(.*)\s*$/;
		$thiskey = $1;
		#print "Current<<$current>> Thiskey<<$thiskey>>\n";
		#print "${$href}{$thiskey}\n";
		if($current =~ /^.*Submitter.*$/) {
			CondorTest::debug("Skip Submitter entry\n",1);
			CondorTest::debug("${$harray1}[$location] vs ${$harray2}[$location]\n",1);
		} elsif( exists ${$href}{$thiskey}) {
			CondorTest::debug("Skip compare of -$thiskey-\n",1);
			CondorTest::debug("${$harray1}[$location] vs ${$harray2}[$location]\n",1);
		} else {
			if(${$harray1}[$location] eq ${$harray2}[$location]){
				#print "Good match:${$harray1}[$location] at entry $location\n";
			} else {
				CondorTest::debug("Match FAIL position <<$location>>\n",1);
				CondorTest::debug("    schedd: ${$harray1}[$location]\n",1);
				CondorTest::debug("    rdbms : ${$harray2}[$location]\n",1);
				return(1);
			}
		}

		$location = $location + 1;
		#print "Location now <<$location>>\n";
	}

	if($delayedfail == 1) {
		CondorTest::debug("delayfail set so really failing\n",1);
		print "delayfail set so really failing\n";
		return(1);
	}
	return(0);
};

$abort = sub
{
	if($aborthandled eq "no") {
		$aborthandled = "yes";
		CondorTest::debug("Jobs Cleared out\n",1);
	}
};

$success = sub
{
	if($successhandled eq "no") {
		$successhandled = "yes";
		CondorTest::debug("Jobs done\n",1);
	}
};

$release = sub
{
	if($releasehandled eq "no") {
		$releasehandled = "yes";
		CondorTest::debug("Jobs running\n",1);
	}

};

$submitted = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
	my $doneyet = "no";


	$submitcount = $submitcount + 1;

	if($submitcount == 75) {

	sleep(60); # database settling time

		CondorTest::debug("75 submitted: <<<",1);
		print scalar localtime() . "\n";
		CondorTest::debug(">>>\n",1);
		CondorTest::debug("Collecting queue details from schedd\n",1);
		my @adarray;
		my $adstatus = 1;
		my $adcmd = "condor_q -long  -direct schedd $cluster";
		$adstatus = CondorTest::runCondorTool($adcmd,\@adarray,2,{emit_output=>\&FALSE});
		if(!$adstatus)
		{
			CondorTest::debug("Test failure due to Condor Tool Failure<$adcmd>\n",1);
			exit(1)
		}

		#foreach my $line (@adarray)
		#{
			#print "$line\n";
		#}
		CondorTest::debug("Collecting queue details from rdbms\n",1);
		my @bdarray;
		my $bdstatus = 1;
		my $bdcmd = "condor_q -long  -direct rdbms $cluster";
		$bdstatus = CondorTest::runCondorTool($bdcmd,\@bdarray,2,{emit_output=>\&FALSE});
		if(!$bdstatus)
		{
			CondorTest::debug("Test failure due to Condor Tool Failure<$bdcmd>\n",1);
			exit(1)
		}

		#foreach my $line (@bdarray)
		#{
			#print "$line\n";
		#}
		$startrow = CondorTest::find_pattern_in_array("MyType", \@adarray);
		CondorTest::debug("Start row returned is $startrow\n",1);
		CondorTest::debug("Size of array is $#adarray\n",1);
		%skip = ("Submitter", 1, "LocalSysCpu", 1, "LocalUserCpu", 1,
					"Rank", 1, "RemoteSysCpu", 1, "RemoteWallClockTime", 1,
					"ServerTime", 1, "RemoteUserCpu", 1, "Environment", 1);
		print scalar localtime() . "\n";
		@scheddads = sort(@adarray);
		@rdbmsads = sort(@bdarray);
		$scheddout = "job_quill_basic.schedd_ads";
		$rdbsout = "job_quill_basic.rdbms_ads";
		CondorTest::debug("Done sorting arrays\n",1);
		open(SCHEDD,">>$scheddout") or die "Can not open $scheddout:$!\n";
		open(RDBS,">>$rdbsout") or die "Can not open $rdbsout:$!\n";
		foreach $ad (@scheddads) {
			print SCHEDD "$ad\n";
		}
		print SCHEDD "*******************\n";
		foreach $ad (@rdbmsads) {
			print RDBS "$ad\n";
		}
		print RDBS "*******************\n";
		close(SCHEDD);
		close(RDBS);
		print scalar localtime() . "\n";
		$result = compare_2arrays_byrows(\@scheddads, \@rdbmsads, \%skip);
		# save the time and leave the jobs behind before stopping condor
		if( $result != 0 ) {
			die "Ads from both sources were not the same!!!\n";
		}
		CondorTest::debug("$testname: SUCCESS\n",1);
		exit(0);
	}

};

CondorTest::RegisterAbort($testname, $abort);
CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

