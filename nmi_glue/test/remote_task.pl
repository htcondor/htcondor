#!/usr/bin/env perl

######################################################################
# $Id: remote_task.pl,v 1.1.2.12 2004-06-25 02:35:59 wright Exp $
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


######################################################################
# get the testname and group
######################################################################

@testinfo = split(/\./, $fulltestname);
my $compiler = @testinfo[0];
my $testname = @testinfo[1];
if( ! $compiler || ! $testname ) {
    c_die("Invalid input for compiler/testname\n");
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
    c_die("compiler $compiler is not valid\n");
}

######################################################################
# build the test
######################################################################

chdir( "$SrcDir/$testdir/$compiler" ) || 
    c_die("Can't chdir($SrcDir/$testdir/$compiler): $!\n");

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

print "RUNNING $compiler.$testname\n";
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
if( ! -d "$BaseDir/results/$compiler" ) {
    mkdir( "$BaseDir/results/$compiler" ) || 
	c_die("Can't mkdir($BaseDir/results/$compiler): $!\n");
}
chdir( "$SrcDir/$testdir/$compiler" ) || 
    c_die("Can't chdir($SrcDir/$testdir/$compiler): $!\n");

$copy_failure = 0;

safe_copy( "$testname.out*", "$BaseDir/results/$compiler" );
safe_copy( "$testname.err*", "$BaseDir/results/$compiler" );
safe_copy( "$testname.log", "$BaseDir/results/$compiler" );
safe_copy( "$testname.run.out", "$BaseDir/results/$compiler" );
safe_copy( "$testname.cmd.out", "$BaseDir/results/$compiler" );

    
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
