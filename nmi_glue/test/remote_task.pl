#!/usr/bin/env perl

######################################################################
# $Id: remote_task.pl,v 1.1.4.2 2004-10-22 06:55:37 wright Exp $
# run a test in the Condor testsuite
# return val is the status of the test
# 0 = built and passed
# 1 = build failed
# 2 = run failed
# 3 = internal fatal error (a.k.a. die)
######################################################################

######################################################################
###### WARNING!!!  The return value of this script has special  ######
###### meaning, so you can NOT just call die().  you MUST       ######
###### use the special c_die() method so we return 3!!!!        ######
######################################################################

if( ! defined $ENV{_NMI_TASKNAME} ) {
    die "_NMI_TASKNAME not in environment, can't test anything!\n";
}
my $fulltestname = $ENV{_NMI_TASKNAME};
if( ! $fulltestname ) {
    # if we have no task, just return success immediately
    print "No tasks specified, returning SUCCESS\n";
    exit 0;
}

my $BaseDir = $ENV{BASE_DIR} || c_die("BASE_DIR is not in environment!\n");
my $SrcDir = $ENV{SRC_DIR} || c_die("SRC_DIR is not in environment!\n");
my $testdir = "condor_tests";

######################################################################
# get the testname and group
######################################################################

@testinfo = split(/\./, $fulltestname);
my $testname = @testinfo[0];
my $compiler = @testinfo[1];

if( ! $testname ) {
    c_die("Invalid input for testname\n");
}
print "testname is $testname\n";
if( $compiler ) {
    print "compiler is $compiler\n";
    $targetdir = "$SrcDir/$testdir/$compiler";
} else {
    $compiler = ".";
    $targetdir = "$SrcDir/$testdir";
}


######################################################################
# build the test
######################################################################

chdir( "$targetdir" ) || c_die("Can't chdir($targetdir): $!\n");

open( TESTBUILD, "make $testname 2>&1 |" ) || 
    c_die("Can't run make $testname\n");
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

print "RUNNING $testinfo\n";
chdir("$SrcDir/$testdir") || c_die("Can't chdir($SrcDir/$testdir): $!\n");

system( "make batch_test.pl" );
if( $? >> 8 ) {
    c_die("Can't build batch_test.pl\n");
}
system( "make CondorTest.pm" );
if( $? >> 8 ) {
    c_die("Can't build CondorTest.pm\n");
}
system( "make Condor.pm" );
if( $? >> 8 ) {
    c_die("Can't build Condor.pm\n");
}

open(BATCHTEST, "perl ./batch_test.pl -d $compiler -t $testname 2>&1 |" ) || 
    c_die("Can't open \"batch_test.pl -d $compiler -t $testname\": $!\n");
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
	c_die("Can't mkdir($BaseDir/results): $!\n");
}
if( $compiler eq "." ) {
    $resultdir = "$BaseDir/results/base";
} else {
    $resultdir = "$BaseDir/results/$compiler";
}
if( ! -d "$resultdir" ) {
    mkdir( "$resultdir" ) || c_die("Can't mkdir($resultdir): $!\n");
}
chdir( "$SrcDir/$testdir/$compiler" ) || 
    c_die("Can't chdir($SrcDir/$testdir/$compiler): $!\n");

$copy_failure = 0;

safe_copy( "$testname.out*", $resultdir );
safe_copy( "$testname.err*", $resultdir  );
safe_copy( "$testname.log", $resultdir  );
safe_copy( "$testname.run.out", $resultdir  );
safe_copy( "$testname.cmd.out", $resultdir  );

if( $copy_failure ) {
    if( $teststatus == 0 ) {
        # if the test passed but we failed to copy something, we
	# should consider that some kind of weird error
	c_die("Failed to copy some output to results!\n");
    } else {
	# if the test failed, we can still mention we failed to copy
	# something, but we should just treat it as if the test
	# failed, not an internal error.
	print "Failed to copy some output to results!\n";
    }
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


sub c_die {
    my( $msg ) = @_;
    print $msg;
    exit 3;
}
