#!/usr/bin/perl

######################################################################
# $Id: remote_task.pl,v 1.1.2.5 2004-06-24 21:33:59 wright Exp $
# run a test in the Condor testsuite
# return val is the status of the test
# 0 = good
# 1 = build failed
# 2 = run failed
# 3 = internal fatal error (a.k.a. die)
######################################################################

my $fulltestname = $ARGV[0] || die "Task name not passed as argument!\n";

###
### We want to re-define die to exit with status 3 !!!!
###

######################################################################
# set up path 
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR is not in environment!\n";
my $SrcDir = $ENV{SRC_DIR} || die "SRC_DIR is not in environment!\n";

######################################################################
# get the testname and group
######################################################################

@testinfo = split(/\//, $fulltestname);
my $compiler = @testinfo[0];
my $testname = @testinfo[1];
if( ! $compiler || ! $testname ) {
    die "Invalid input for compiler/testname\n";
}
print "compiler is $compiler\n";
print "testname is $testname\n";

if( ($compiler =~ m/gcc/) || ($compiler =~ m/g\+\+/) || 
    ($compiler =~ /cc/) || ($compiler =~ /CC/) ) {
    $testdir = "condor_test_suite_C.V5";
} elsif ( ($compiler =~ /g77/) || ($compiler =~ /f77/) || 
	  ($compiler =~ /f90/) ) {
    $testdir = "condor_test_suite_F.V5";
} else {
    die "compiler $compiler is not valid\n";
}

######################################################################
# build the test
######################################################################

chdir( "$SrcDir/$testdir/$compiler" ) || 
    die "Can't chdir($SrcDir/$testdir/$compiler): $!\n";

open( TESTBUILD, "make $testname 2>&1 |" ) || 
    die "Can't run make $testname\n";
while( <TESTBUILD> ) {
    print $_;
}
close( TESTBUILD );
$buildstatus = $?;
print "BUILD TEST for $testname returned $buildstatus\n";
if( $buildstatus != 0 ) {
    print "Build failed for $testname\n";
    exit 2;
}


######################################################################
# run the test using batch_test.pl
######################################################################

print "RUNNING $compiler/$testname\n";
chdir("$SrcDir/$testdir") || die "Can't chdir($SrcDir/$testdir): $!\n";

system( "make batch_test.pl" );
if( $? >> 8 ) {
    die "Can't build batch_test.pl\n";
}
system( "make CondorTest.pm" );
if( $? >> 8 ) {
    die "Can't build CondorTest.pm\n";
}
system( "make Condor.pm" );
if( $? >> 8 ) {
    die "Can't build Condor.pm\n";
}

open(BATCHTEST, "perl ./batch_test.pl -d $compiler -t $testname 2>&1 |" ) || 
    die "Can't open \"batch_test.pl -d $compiler -t $testname\": $!\n";
while ( <BATCHTEST> ) {
    print $_;
}
close (BATCHTEST);
$batchteststatus = $?;

# figure out here if the test passed or failed.  
if( $batchteststatus != 0 ) {
    $teststatus = 2;
} else {
    $teststatus = 0;
}


######################################################################
# copy test results to results dir
######################################################################

if( ! -d "$BaseDir/results" ) {
    mkdir( "$BaseDir/results" ) || 
	die "Can't mkdir($BaseDir/results): $!\n";
}
if( ! -d "$BaseDir/results/$compiler" ) {
    mkdir( "$BaseDir/results/$compiler" ) || 
	die "Can't mkdir($BaseDir/results/$compiler): $!\n";
}
chdir( "$SrcDir/$testdir/$compiler" ) || 
    die "Can't chdir($SrcDir/$testdir/$compiler): $!\n";

$copy_failure = 0;

safe_copy( "$testname.out*", "$BaseDir/results/$compiler" );
safe_copy( "$testname.err*", "$BaseDir/results/$compiler" );
safe_copy( "$testname.log", "$BaseDir/results/$compiler" );
safe_copy( "$testname.run.out", "$BaseDir/results/$compiler" );
safe_copy( "$testname.cmd.out", "$BaseDir/results/$compiler" );

if( $copy_failure ) {
    die "Failed to copy some output to results!\n";
}

exit $teststatus;


sub safe_copy {
    my( $src, $dest ) = @_;
    system( "cp $src $dest" );
    if( $? >> 8 ) {
	print "Can't copy $src to $dest: $!\n";
	$copy_failure = 1;
    } else {
	print "Copied $src to $dest\n";
    }
}
