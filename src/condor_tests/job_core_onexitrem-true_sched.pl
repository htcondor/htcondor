#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_onexitrem-true_sched.cmd';
$testname = 'Condor submit policy test for ON_EXIT_REMOVE - scheduler U';

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my %args;
my $cluster;

$abnormal = sub {

	print "Want to see only submit and abort events for periodic remove test\n";
	exit(1);
};

$aborted = sub {
	print "Abort event expected from expected condor_rm used to remove the requeueing job\n";
	print "Policy test worked.\n";
};

$held = sub {
	print "We don't hit Hold state for job restarted for not running long enough!\n";
	exit(1);
};

$executed = sub
{
	%args = @_;
	$cluster = $args{"cluster"};
	print "Good. for on_exit_remove cluster $cluster must run first\n";
};

$evicted = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

	print "Good a requeue.....after eviction....\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(0)
	}
};

CondorTest::RegisterEvictedWithRequeue($testname, $evicted);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

