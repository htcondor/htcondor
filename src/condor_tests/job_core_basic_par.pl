#! /usr/bin/env perl
use CondorTest;

$cmd = $ARGV[0];

print "Submit file for this test is $cmd\n";
print "looking at env for condor config\n";
system("printenv | grep CONDOR_CONFIG");

$testname = 'Basic Parallel - Parallel U';

$aborted = sub {
	my %info = @_;
	my $done;
	print "Abort event not expected!\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Held event not expected.....\n";
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	print "Parallel job executed\n";
};

$timed = sub
{
	die "Test took too long!!!!!!!!!!!!!!!\n";
};

$success = sub
{
	print "Success: Parallel Test ok\n";
};

$release = sub
{
	print "Release expected.........\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_reschedule";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		return(1)
	}
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterTimed($testname, $timed, 600);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

