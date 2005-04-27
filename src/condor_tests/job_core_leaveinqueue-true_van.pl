#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_leaveinqueue-true_van.cmd';
$testname = 'Condor submit with test for policy trigger of leave_in_queue - vanilla U';

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

	sleep 10;
	print "Good, job should be done but left in the queue!!!\n";
	my $qstat = CondorTest::getJobStatus($cluster);
	while($qstat == -1)
	{
		print "Job status unknown - wait a bit\n";
		sleep 2;
		$qstat = CondorTest::getJobStatus($cluster);
	}
	if( $qstat != COMPLETED )
	{
		die "Job not still found in queue in completed state\n";
	}
	else
	{
		my @adarray;
		my $status = 1;
		my $cmd = "condor_rm $cluster";
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			return(1)
		}
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

