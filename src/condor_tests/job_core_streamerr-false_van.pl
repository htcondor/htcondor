#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_streamerr-false_van.cmd';
$testname = 'Environment is preserved - vanilla U';


$execute = sub
{
	my %info = @_;
	my $name = $info{"error"};
	my $size1 = -s $name;
	print "Size 1 of $name is $size1\n";
	sleep 2;
	my $size2 = -s $name;
	print "Size 2 of $name is $size2\n";
	if( $size1 != $size2 )
	{
		print "Error size should be stable if not streaming\n";
		exit(1);
	}
};

$ExitSuccess = sub {
	my %info = @_;
};


# before the test let's throw some weird crap into the environment

CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
CondorTest::RegisterExecute($testname, $execute);

#empty local environment and add only a few things that way......
my $path = $ENV{"PATH"};

$condor_config = $ENV{CONDOR_CONFIG};
%ENV = ();
$ENV{CONDOR_CONFIG} = $condor_config;
$ENV{PATH} = $path;
$ENV{UNIVERSE} = "vanilla";

print "join(` `,keys %ENV)";

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

