#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_flocking_to.cmd';
$out = 'job_flocking_to.out';
$testname = 'flocking ';

$submitted = sub
{
	my %info = @_;
	my $name = $info{"error"};
	print "Flocking job submitted\n";
};

$aborted = sub 
{
	my %info = @_;
	my $done;
	die "Abort event NOT expected\n";
};

$timed = sub
{
	die "Job should have ended by now. Flock To error!\n";
};

$execute = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $name = $info{"error"};
	CondorTest::RegisterTimed($testname, $timed, 600);
};

$ExitSuccess = sub {
	my %info = @_;
	print "Flocking job completed without error\n";
};



CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
CondorTest::RegisterExecute($testname, $execute);
CondorTest::RegisterSubmit( $testname, $submitted );


if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

