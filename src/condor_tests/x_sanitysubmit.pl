#! /usr/bin/env perl
use CondorTest;

Condor::DebugOff();

$cmd = 'x_sanitysubmit.cmd';

print "Submit file for this test is $cmd\n";

$testname = 'sanity submit test - vanilla U';

$aborted = sub {
	my %info = @_;
	my $done;
	die "Abort event not expected!\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $holdreason = $info{"holdreason"};

	die "Held event not expected: $holdreason \n";
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

};

$success = sub
{
	print "Success: ok\n";
};

$release = sub
{
	die "Release NOT expected.........\n";
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

