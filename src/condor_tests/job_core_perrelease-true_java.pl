#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perrelease-true_java.cmd';
$testname = 'Condor submit with for periodic release test - java U';

$aborted = sub {
	my %info = @_;
	my $done;
	print "Abort event not expected!\n";
	die "Want to see only submit, release and successful completion events for periodic release test\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	die "Held event not expected.....\n";
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	print "Periodic Hold should see and execute, followed by a release and then we reschedule the job\n";
};

$success = sub
{
	print "Success: ok\n";
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
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

