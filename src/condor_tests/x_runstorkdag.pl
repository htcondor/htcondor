#! /usr/bin/env perl
use CondorTest;
use CondorPersonal;
use Cwd;

CondorPersonal::DebugOff();
Condor::DebugOff();


# where am I running
$dummyhost = "gamgee.cs.wisc.edu";
$currenthost = `hostname`;
chomp($currenthost);

system("date");
print "Dummyhost was $dummyhost and really running on $currenthost\n";

$mypid = $$;
$mysaveme = $corename . ".saveme";
system("mkdir $mysaveme");

$mysubmitnm = $corename . $mypid . ".cmd";
$mysubmitnmout = $corename . $mypid . ".cmd.out";
$mysubmitnmlog = $corename . $mypid . ".log";
$mysubmitnmout = $corename . $mypid . ".log";
$mycorenm = $corename . ".cmd";
$mycorenmout = $corename . ".cmd.out";
$mycorenmlog = $corename . ".log";

print "Master PID is $mypid\n";
print "Master PID submit file is $mysubmitnm\n";

my $configloc = CondorPersonal::StartCondor("x_param.storksrvr" ,"local");
my @local = split /:/, $configloc;
my $locconfig = shift @local;
my $locport = shift @local;

print "---local config is $locconfig and local port is $locport---\n";

# find out the directory and cp stork submit file there and 
# build dag file there to submit from there and preserve all logs etc.

my $personaldir = "";
if($locconfig =~ /(.*)\/condor_config/)
{
	$personaldir = $1;
	print "Personal Condor and Dag runtime directory is $personaldir\n";
}


$ENV{CONDOR_CONFIG} = $locconfig;
# submit into stork personal condor
# we can not assume for a dagman test that we have a stork server
# running in the personal condor which is being run for the test suite
# so we setup and run our own.

if( ! -e "job_stork_file-file.in" )
{
	system("date > job_stork_file-file.in");
}

system("./x_makestorkdag.pl --root=$personaldir --dag=dag1 --dagnm=dag1.dag --src_url=/dev/null --dest_url=/dev/null");

$cmd = "$dagnm";
$testname = 'Condor submit dag - stork transfer test';
$dagman_args = "-v -storklog `condor_config_val LOG`/Stork.user_log";

$executed = sub
{
        print "Good. We need the dag to run\n";
};

$submitted = sub
{
        print "submitted: This test will see submit, executing and successful completion\n";
};

$success = sub
{
        print "executed successfully\n";
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunDagTest($testname, $cmd, 0, $dagman_args) ) {
        print "$testname: SUCCESS\n";
        exit(0);
} else {
        die "$testname: CondorTest::RunTest() failed\n";
}

system("cp -r $mypid $mysaveme");
system("cp job_stork_file-file.stork* $mysaveme");

if( -e "$mysubmitnm" )
{
	system("cp $mysubmitnm $mycorenm");
}

if( -e "$mysubmitnmout" )
{
	system("cp $mysubmitnmout $mycorenmout");
}

if( -e "$mysubmitnmlog" )
{
	system("cp $mysubmitnmlog $mycorenmlog");
}

my @adarray;
my $status = 1;
my $cmd = "condor_off -master";
$status = CondorTest::runCondorTool($cmd,\@adarray,2);
if(!$status)
{
	print "Test failure due to Condor Tool Failure<$cmd>\n";
	return(1)
}

system("date");

if( $result != 0 )
{
	# actual test failed but we allowed cleanup
	print "Stork test failed!\n";
	exit(1);
}
else
{
	print "Stork test SUCCESS!\n";
}

