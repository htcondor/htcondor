#! /usr/bin/env perl
use CondorTest;

Condor::DebugOff();

$cmd = 'x_cmdrunforever.cmd';


# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

print "Submit file for this test is $cmd\n";
print "looking at env for condor config\n";
system("printenv | grep CONDOR_CONFIG");

$condor_config = $ENV{CONDOR_CONFIG};
print "CONDOR_CONFIG = $condor_config\n";

$testname = 'Condor-C A & B test - vanilla U';

$submitted = sub {
	print "Run forever Job submitted\n";
};

$aborted = sub {
	my %info = @_;
	my $done;
	die "Abort event not expected!\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Held event not expected.....\n";
	exit(1);
};

$executed = sub
{
	my %info = @_;
    $cluster = $info{"cluster"};

    my $qstat = CondorTest::getJobStatus($cluster);
    while($qstat != 2)
    {
        print "Job status unknown - wait a bit\n";
        sleep 2;
        $qstat = CondorTest::getJobStatus($cluster);
    }

    print "It better be running... status is $qstat(2 is correct)\n";
    if($qstat != RUNNING)
    {
        print "Cluster $cluster failed to go on hold\n";
        exit(1);
    }
	print "Run forever job is executing\n";
	exit(0);
};

$timed = sub
{
	die "Test took too long!!!!!!!!!!!!!!!\n";
};

$success = sub
{
	print "Success: ok\n";
};

$release = sub
{
	print "Release expected.........\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_reschedule";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterSubmit( $testname, $submitted);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterRelease( $testname, $release );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

