#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perrelease-false_van.cmd';
$testname = 'Condor submit policy test for periodic_release - vanilla U';

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

	print "Bad. Job submitted on hold and NEVER released by periodic release policy... Should not RUN!\n";
	exit(1);
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Bad. Job submitted on hold and NEVER released by periodic release policy... Should not Complete!\n";
	exit(1);
};

$timed = sub
{
	my %info = @_;
	#my $cluster = $info{"cluster"};

	print "Cluster $cluster alarm wakeup\n";
	print "wakey wakey!!!!\n";

	system("condor_rm $cluster");
	sleep 5;
};

$submit = sub
{
	my %info = @_;
	$cluster = $info{"cluster"};

	my $qstat = CondorTest::getJobStatus($cluster);
	while($qstat == -1)
	{
		print "Job status unknown - wait a bit\n";
		sleep 2;
		$qstat = CondorTest::getJobStatus($cluster);
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
# Note we are never going to release this job if preriodic release
# policy is false.... so eventually we simply remove it
# Question is....... how long after submit do we wait to make sure
CondorTest::RegisterTimed($testname, $timed, 480); # kill in queue 8 minutes
CondorTest::RegisterExitedSuccess( $testname, $success );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

