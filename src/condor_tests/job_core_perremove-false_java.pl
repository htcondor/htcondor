#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_perremove-false_java.cmd';
$testname = 'Condor submit policy test for periodic_remove - java U';

my %info;
my $cluster;

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	print "Good. for periodic_remove cluster $cluster must run first\n";
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Good, job should complete trivially\n";
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

