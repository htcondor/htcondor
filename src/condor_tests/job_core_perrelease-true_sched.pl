#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perrelease-true_sched.cmd';
$testname = 'Condor submit with for periodic release test - scheduler U';

$aborted = sub {
	my %info = @_;
	my $done;
	print "Abort event not expected!\n";
	print "Want to see only submit, release and successful completion events for periodic release test\n";
	exit(1);
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Held event not expected.....\n";
	exit(1);
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
	system("condor_reschedule"); # get job running to completion.
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

