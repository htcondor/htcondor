#! /usr/bin/env perl
use CondorTest;
use CondorPersonal;
use Cwd;

CondorPersonal::DebugOff();
Condor::DebugOff();


my $LogFile = "sdmkdag.log";
print "Build log will be in ---$LogFile---\n";
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!\n";
open(STDERR, ">&STDOUT");
select(STDERR); $| = 1;
select(STDOUT); $| = 1;

$dir = $ARGV[0];
$cmd = $ARGV[1];
#$testname = 'Condor submit dag - stork transfer test';
$testname =  $ARGV[2];
$dagman_args = "-v -storklog `condor_config_val LOG`/Stork.user_log";

chdir("$dir");

my $loc = getcwd();
print "Currently in $loc\n";

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


