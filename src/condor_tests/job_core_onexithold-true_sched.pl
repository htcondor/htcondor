#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_onexithold-true_sched.cmd';
$testname = 'Condor submit with hold for periodic remove test - scheduler U';

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

	print "Good, job - $cluster $job - aborted after Hold state reached\n";
};

$held = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	my $fulljob = "$cluster"."."."$job";
	print "Good, good run of job - $fulljob - should be in queue on hold now\n";
	print "Removing $fulljob\n";
	system("condor_rm $fulljob");
	system("condor_reschedule");
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

	die "Bad, job should be on hold and then cpondor_rm and not run to completion!!!\n";
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
#CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

