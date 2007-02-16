#! /usr/bin/env perl
use CondorTest;

Condor::DebugOff();

$cmd      = 'job_quill_basic.cmd';
$testname = 'Verify same data from schedd, quilld and rdbms';

sub HELD{5};
sub RUNNING{2};

my $alreadydone=0;
my $result;

my $submithandled = "no";
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

	print "Make sure they all have the same number of entries\n";

	my $count1, $count2, $count3;

	$count1 = $#{$harray1};
	$count2 = $#{$harray2};
	$count3 = $#{$harray3};

	if ( ($count1 == $count2) && ($count1 == $count3)) {
		print "Good the sizes<<$count1>> match up!!!!\n";
	} else {
		print "Compare failed because $count1, $count2 and $count3 are not the same\n";
		return(1);
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
				print "Match FAIL position <<$location>>: 1: ${$harray1}[$location] 2: ${$harray2}[$location] 3: ${$harray3}[$location]\n";
				return(1);
			}
		}

		$location = $location + 1;
		#print "Location now <<$location>>\n";
	}

	return(0);
};

$abort = sub
{
	if($aborthandled eq "no") {
		$aborthandled = "yes";
		print "Jobs Cleared out\n";
	}
};

$success = sub
{
	if($successhandled eq "no") {
		$successhandled = "yes";
		print "Jobs done\n";
	}
};

$release = sub
{
	if($releasehandled eq "no") {
		$releasehandled = "yes";
		print "Jobs running\n";
	}

};

$submitted = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
	my $doneyet = "no";


	if($submithandled eq "no") {
		$submithandled = "yes";

sleep(30);

		print "submitted\n";
		print "Collecting queue details from schedd\n";
		my @adarray;
		my $adstatus = 1;
		my $adcmd = "condor_q -long  -direct schedd $cluster";
		$adstatus = CondorTest::runCondorTool($adcmd,\@adarray,2);
		if(!$adstatus)
		{
			print "Test failure due to Condor Tool Failure<$adcmd>\n";
			exit(1)
		}

		#foreach my $line (@adarray)
		#{
			#print "$line\n";
		#}
		print "Collecting queue details from quilld\n";
		my @bdarray;
		my $bdstatus = 1;
		my $bdcmd = "condor_q -long  -direct quilld $cluster";
		$bdstatus = CondorTest::runCondorTool($bdcmd,\@bdarray,2);
		if(!$bdstatus)
		{
			print "Test failure due to Condor Tool Failure<$bdcmd>\n";
			exit(1)
		}

		#foreach my $line (@bdarray)
		#{
			#print "$line\n";
		#}
		print "Collecting rdbms details from quilld\n";
		my @cdarray;
		my $cdstatus = 1;
		my $cdcmd = "condor_q -long  -direct quilld $cluster";
		$cdstatus = CondorTest::runCondorTool($cdcmd,\@cdarray,2);
		if(!$cdstatus)
		{
			print "Test failure due to Condor Tool Failure<$cdcmd>\n";
			exit(1)
		}

		#foreach my $line (@cdarray)
		#{
			#print "$line\n";
		#}
		$startrow = CondorTest::find_pattern_in_array("MyType", \@cdarray);
		print "Start row returned is $startrow\n";
		print "Size of array is $#cdarray\n";
		%skip = ("Submitter", 1, "LocalSysCpu", 1, "LocalUserCpu", 1,
					"Rank", 1, "RemoteSysCpu", 1, "RemoteWallClockTime", 1,
					"ServerTime", 1, "RemoteUserCpu", 1);
		system("date");
		@adarray = sort(@adarray);
		@bdarray = sort(@bdarray);
		@cdarray = sort(@cdarray);
		print "Done sorting arrays\n";
		system("date");
		$result = compare_3arrays_byrows(\@adarray, \@bdarray, \@cdarray, \%skip);
		# save the time and leave the jobs behind before stopping condor
		if( $result != 0 ) {
			die "Ads from all three sources were not the same!!!\n";
		}
		print "$testname: SUCCESS\n";
		exit(0);
	}

};

CondorTest::RegisterAbort($testname, $abort);
CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

