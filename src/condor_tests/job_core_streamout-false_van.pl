#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_streamout-false_van.cmd';
$testname = 'Environment is preserved - vanilla U';

$timed = sub
{
	die "Test should have eneded by now.... stuck!\n";
};

$aborted = sub 
{
	my %info = @_;
	my $done;
	print "Abort event expected as we clear job\n";
};

$execute = sub
{
	my %info = @_;
	my $name = $info{"output"};
	my $cluster = $info{"cluster"};
	CondorTest::RegisterTimed($testname, $timed, 180);
	my $size1 = -s $name;
	#print "Size 1 of $name is $size1\n";
	while($size1 == 0)
	{
		#print "Size 1 of $name is $size1\n";
		$size1 = -s $name;
	}
	print "Size 1 of $name is $size1\n";
	sleep 4;
	my $size2 = -s $name;
	print "Size 2 of $name is $size2\n";
	if( $size1 != $size2 )
	{
		die "Output size should be stable if not streaming\n";
	}

	print "OK remove the job!\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		return(1)
	}
};

$ExitSuccess = sub {
	my %info = @_;
};


# before the test let's throw some weird crap into the environment

CondorTest::RegisterAbort( $testname, $aborted );
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

