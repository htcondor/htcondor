#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_onexithold-false_java.cmd';
$testname = 'Condor submit with hold for periodic remove test - java U';

my $killedchosen = 0;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my %testerrors;
my %info;
my $cluster;

$abnormal = sub {
	my %info = @_;

	die "Want to see only submit and abort events for periodic remove test\n";
};

$aborted = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	if( $job eq "000" )
	{
		print "Good, job - $job - aborted after Hold state reached\n";
	}
	elsif( $job eq "001" )
	{
		print "Bad, job $job should not be aborted EVER!\n";
		$testerrors{$job} = "job $job should not be aborted EVER!";
	}
	else
	{
		die "ABORT: ONLY 2 jobs expected - job $job - !!!!!!!!!!!!\n";
	}
};

$held = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	if( $job eq 0 )
	{
		my $fulljob = "$cluster"."."."$job";
		print "Good, good run of job - $fulljob - should be in queue on hold now\n";
		print "Removing $fulljob\n";
		my @adarray;
		my $status = 1;
		my $cmd = "condor_rm $cluster";
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			return(1)
		}
		my @nadarray;
		$status = 1;
		$cmd = "condor_reschedule";
		$status = CondorTest::runCondorTool($cmd,\@nadarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			return(1)
		}
	}
	elsif( $job eq 1 )
	{
		print "Bad, job $job should NOT be on hold!!!\n";
	}
	else
	{
		die "HOLD: ONLY 2 jobs expected - job $job - !!!!!!!!!!!!\n";
	}
};

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	print "Good. for on_exit_hold cluster $cluster must run first\n";
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};

	print "Good, good job - $job - should complete trivially\n";
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
#CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
#CondorTest::RegisterAbort( $testname, $aborted );
#CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

