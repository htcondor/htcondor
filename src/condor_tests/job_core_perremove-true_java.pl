#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perremove-true_java.cmd';
$testname = 'Condor submit policy test for PERIODIC_REMOVE - java U';

my $killedchosen = 0;

my %args;
my $cluster;

$aborted = sub {
	print "Abort event expected from periodic_remove policy evaluating to true\n";
	print "Policy test worked.\n";
};

$executed = sub
{
	%args = @_;
	$cluster = $args{"cluster"};
	print "Good. for on_exit_remove cluster $cluster must run first\n";
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterAbort( $testname, $aborted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

