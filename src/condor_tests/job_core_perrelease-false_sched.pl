#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perrelease-false_sched.cmd';
$testname = 'Condor submit policy test for periodic_release - scheduler U';

my %info;
my $cluster;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	print "Bad. This job tests for policy to periodically release NEVER! Can't run!\n";
	exit(1);
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Bad. This job tests for policy to periodically release NEVER! Can't complete!\n";
	exit(1);
};

$timed = sub
{
	my %info = @_;
	#my $cluster = $info{"cluster"};

	print "Cluster $cluster alarm wakeup\n";
	print "wakey wakey!!!!\n";
	print "good\n";
	system("condor_rm $cluster");
	sleep 5;
};

$submit = sub
{
	my %info = @_;
	$cluster = $info{"cluster"};
	my $qstat = CondorTest::GoodCondorQ_Result($cluster);
	while($qstat == -1)
	{
		print "Job status unknown - wait a bit\n";
		sleep 2;
		$qstat = CondorTest::GoodCondorQ_Result($cluster);
	}

	print "It better be on hold... status is $status(5 is correct)";
	if($qstat != HELD)
	{
		print "Cluster $cluster failed to go on hold\n";
		exit(1);
	}


	print "Cluster $cluster submitted\n";
};

$abort = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Cluster $cluster aborted after hold state verified\n";
};

CondorTest::RegisterSubmit($testname, $submit);
CondorTest::RegisterAbort($testname, $abort);
CondorTest::RegisterExecute($testname, $executed);
# How long to wait for job submited on hold
# testing policy for periodic release to never release it??????
# lets say 8 minutes..... Is there another way?
CondorTest::RegisterTimed($testname, $timed, 480);
CondorTest::RegisterExitedSuccess( $testname, $success );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

