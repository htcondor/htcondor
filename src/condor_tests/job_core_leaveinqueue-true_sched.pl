#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_leaveinqueue-true_sched.cmd';
$testname = 'Condor submit with test for policy trigger of leave_in_queue - scheduler U';

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

	system("condor_q");
	print "Good, job should be done but left in the queue!!!\n";
	my $status = `condor_q $cluster -format %d JobStatus`;
	print "Status from condor_q $cluster -format %d JobStatus is -- $status --\n";
	if( $status != COMPLETED )
	{
		die "Job not still found in queue in completed state\n";
	}
	else
	{
		system("condor_rm $cluster");
	}

};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

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

