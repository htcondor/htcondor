#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_streamerr-true_van.cmd';
$err = 'job_core_streamerr-true_van.err';
$testname = 'Environment is preserved - vanilla U';

$submitted = sub
{
	my %info = @_;
	my $name = $info{"error"};
	system("rm $name");
};

$timed = sub
{
	die "Job should have ended by now. file streaming error file broken!\n";
};

$execute = sub
{
	my %info = @_;
	my $name = $info{"error"};
	CondorTest::RegisterTimed($testname, $timed, 180);
	my $size1 = -s $name;
	#print "Size 1 of $name is $size1\n";

	while( $size1 == 0 )
	{
		$size1 = -s $name;
		#print "Size 1 of $name is $size1\n";
	}
	print "Size 1 of $name is $size1\n";

	my $size2 = -s $name;
	#print "Size 2 of $name is $size2\n";
	while( $size1 == $size2 )
	{
		#print "keep checking: size should vary if streaming\n";
		$size2 = -s $name;
		#print "Size 2 of $name is $size2\n";
	}
	print "Size 2 of $name is $size2\n";
};

$ExitSuccess = sub {
	my %info = @_;
};


# before the test let's throw some weird crap into the environment

CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
CondorTest::RegisterExecute($testname, $execute);
CondorTest::RegisterSubmit( $testname, $submitted );

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

