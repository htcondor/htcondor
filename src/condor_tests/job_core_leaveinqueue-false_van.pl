#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_leaveinqueue-false_van.cmd';
$testname = 'Condor submit with policy set to not trigger for leave_in_queue - vanilla U';

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
	my @adarray;
	my $status = 1;
	my $delay = 1;
	my $backoffmax = 17; #(1 + 2 + 4 + 8 + 16 = 31 seconds max)
	my $foundit = 0;
	my $cmd = "condor_q $cluster";
	while( $delay < $backoffmax) {
		$foundit = 0;
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1)
		}
		foreach my $line (@adarray)
		{
			print "$line\n";
			if($line =~ /^\s*$cluster\..*$/) {
				print "$line\n";
				print "job should be done but NOT left in the queue!!\n";
				$foundit = 1;
			}
		}
		if($foundit == 1) {
			sleep $delay;
			$delay = 2 * $delay;
			next;
		} else {
			print "Job not in queue as expected\n";
			exit(0);
		}
	}
	if($foundit == 1) {
		die "Job still in the queue after multiple looks\n";
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

