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

	die "Bad. Job submitted on hold and NEVER released by periodic release policy... Should not RUN!\n";
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	die "Bad. Job submitted on hold and NEVER released by periodic release policy... Should not Complete!\n";
};

$timed = sub
{
	my %info = @_;
	#my $cluster = $info{"cluster"};
	my $status = `condor_q $cluster -format %d JobStatus`;
	sleep 2;

	print "Cluster $cluster alarm wakeup\n";
	print "wakey wakey!!!!\n";
	print "It better be on hold...staus is $status(5 is good)";

	# under all cases if we hit timeout, remove job or try

	#if($status != HELD)
	#{
		#die "Cluster $cluster failed to go on hold\n";
	#}
	#else
	#{
		print "good\n";
		system("condor_rm $cluster");
	#}
	sleep 5;
};

$submit = sub
{
	my %info = @_;
	$cluster = $info{"cluster"};
	my $status = `condor_q $cluster -format %d JobStatus`;
	sleep 2;

	print "It better be on hold... status is $status(5 is correct)";
	if($status != HELD)
	{
		die "Cluster $cluster failed to go on hold\n";
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

