#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_leaveinqueue-false_sched.cmd';
$testname = 'Condor submit with policy set to not trigger for leave_in_queue - scheduler U';

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub COMPLETED{4};
sub RUNNING{2};

my %testerrors;
my %info;
my $cluster;

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	print "Good. for leave_in_queue cluster $cluster must run first\n";
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Good, job should be done but NOT left in the queue!!!\n";
	my $qstat = CondorTest::getJobStatus($cluster);
	if($qstat == -1)
	{
		print "Job status unknown - Exactly what we want\n";
		exit(0);
	}
	if( $qstat == COMPLETED )
	{
		die "Job should not still be found in queue\n";
	}
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	print "submitted: \n";
	{
		print "good job $job expected submitted.\n";
	}
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

