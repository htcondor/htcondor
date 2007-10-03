#! /usr/bin/env perl
use CondorTest;
Condor::DebugOn();

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
	my $return = 1;

	my $done = 0;
	my $retrycount = 0;
	my $status = 1;
	my $cmd = "condor_q -format \"%s\" Owner -format \" ClusterId = %d\" ClusterId -format \" Status = %d\n\" JobStatus";
	while($done == 0) {
		my @adarray;
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if(!$status) {
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1)
		}
		foreach my $line (@adarray) {
			print "$line\n";
			if($line =~ /^\s*([\w\-\.]+)\s+ClusterId\s*=\s*$cluster\s*Status\s*=\s*(\d+)\s*.*$/) {
				print "Following Line shows it is still in the queue...\n";
				print "$line\n";
				if($2 != COMPLETED) {
					$retrycount = $retrycount +1;
					if($retrycount == 4) {
						print "Can not find the cluster completed in the queue\n";
						last;
					} else {
						sleep((10 * $retrycount));
						next;
					}
				}
				print "Found the cluster completed in the queue\n";
				$done = 1;
				$return = 0;
				last;
			}
		}
	}
	if($done == 0) {
		# we never found completed
		$return = 1;
	}

	print "job should be done AND left in the queue!!\n";
	my @bdarray;
	my @cdarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@bdarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}
	$cmd = "condor_rm -forcex $cluster";
	$status = CondorTest::runCondorTool($cmd,\@cdarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}
	exit($return);
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "submitted: \n";
	{
		print "good job $cluster expected submitted.\n";
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

