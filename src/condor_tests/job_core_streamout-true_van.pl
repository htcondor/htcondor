#! /usr/bin/env perl
use CondorTest;

$cmd = 'job_core_streamout-true_van.cmd';
$testname = 'Environment is preserved - vanilla U';


$execute = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $name = $info{"output"};
	my $size1 = -s $name;
	while($size1 == 0)
	{
		#print "Size 1 of $name is $size1\n";
		$size1 = -s $name;
	}
	print "Size 1 of $name is $size1\n";
	sleep 4;
	my $size2 = -s $name;
	print "Size 2 of $name is $size2\n";
	if( $size1 == $size2 )
	{
		die "Output size should vary if streaming\n";
	}
	if( $size1 == 0 )
	{
		my $size3 = -s $name;
		print "Size 3 of $name is $size3\n";
		#compare size 2 and 3
		if( $size3 == $size2 )
		{
			die "Output size should vary if streaming\n";
		}
	}
	print "OK remove the job!\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_rm $cluster";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(0)
	}
};

$aborted = sub 
{
	my %info = @_;
	my $done;
	print "Abort event expected as we clear job\n";
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

