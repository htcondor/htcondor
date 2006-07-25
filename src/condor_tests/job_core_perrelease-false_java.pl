#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perrelease-false_java.cmd';
$testname = 'Condor submit policy test for periodic_release - java U';

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

	print "Good. for periodic_release cluster $cluster must run first\n";
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Good, job should complete trivially\n";
};

$timed = sub
{
	my %info = @_;
	# use submit cluster as timed info not assured
	#my $cluster = $info{"cluster"};

	print "Cluster $cluster alarm wakeup\n";
	print "wakey wakey!!!!\n";
	print "good\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		return(1)
	}
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
CondorTest::RegisterTimed($testname, $timed, 3600);
CondorTest::RegisterExitedSuccess( $testname, $success );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

