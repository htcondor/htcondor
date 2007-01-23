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
sleep(60);

		print "submitted\n";
		print "Collecting queue details from schedd\n";
		my @adarray;
		my $adstatus = 1;
		my $adcmd = "condor_q -direct schedd $cluster";
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
		my $bdcmd = "condor_q -direct quilld $cluster";
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
		my $cdcmd = "condor_q -direct quilld $cluster";
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
		$startrow = CondorTest::find_pattern_in_array("OWNER", \@cdarray);
		print "Start row returned is $startrow\n";
		print "Size of array is $#cdarray\n";
		$result = CondorTest::compare_arrays($startrow, $#cdarray, 3, \@adarray, \@bdarray, \@cdarray);
		my @ddarray;
		my $ddstatus = 1;
		my $ddcmd = "condor_rm $cluster";
		$ddstatus = CondorTest::runCondorTool($ddcmd,\@ddarray,2);
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

