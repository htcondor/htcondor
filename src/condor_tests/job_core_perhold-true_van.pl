#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perhold-true_van.cmd';
$testname = 'Condor submit with for periodic hold test - vanilla U';
$ENV{UNIVERSE} = "vanilla";
$ENV{UPREPEND} = "v_";

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

$abnormal = sub {
	my %info = @_;

	die "Want to see only execute, hold and abort events for periodic hold test\n";
};

$aborted = sub {
	my %info = @_;
	my $done;
	print "Abort event expected from periodic remove after hold event seen\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Held event expected, removing job.....\n";
	system("condor_rm $cluster");
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	print "Periodic Hold should see and execute, followed by a hold and then we remove the job\n";
};

$submitted = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	print "submitted: ok\n";
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

