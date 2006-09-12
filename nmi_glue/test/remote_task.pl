#!/usr/bin/env perl

######################################################################
# $Id: remote_task.pl,v 1.2.2.3 2006-09-12 17:00:26 wright Exp $
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

use File::Copy;

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
my $testname = $testinfo[0];
my $compiler = $testinfo[1];

if( ! $testname ) {
    c_die("Invalid input for testname\n");
}
print "testname is $testname\n";
if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
    if( $compiler ) {
        print "compiler is $compiler\n";
        $targetdir = "$SrcDir/$testdir/$compiler";
    } else {
        $compiler = ".";
        $targetdir = "$SrcDir/$testdir";
    }
} else {
    $compiler = ".";
    $targetdir = "$SrcDir/$testdir";
}


######################################################################
# build the test
######################################################################

chdir( "$targetdir" ) || c_die("Can't chdir($targetdir): $!\n");

if( !($ENV{NMI_PLATFORM} =~ /winnt/)) {
    print "Attempting to build test in: $targetdir\n";
    print "Invoking \"make $testname\"\n";
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
}


######################################################################
# run the test using batch_test.pl
######################################################################

print "RUNNING $testinfo\n";
chdir("$SrcDir/$testdir") || c_die("Can't chdir($SrcDir/$testdir): $!\n");

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
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
    system( "make CondorPersonal.pm" );
    if( $? >> 8 ) {
        c_die("Can't build CondorPersonal.pm\n");
    }
} else {
    my $scriptdir = $SrcDir . "/condor_scripts";
    copy_file("$scriptdir/batch_test.pl", "batch_test.pl");
    copy_file("$scriptdir/Condor.pm", "Condor.pm");
    copy_file("$scriptdir/CondorTest.pm", "CondorTest.pm");
    copy_file("$scriptdir/CondorPersonal.pm", "CondorPersonal.pm");
}
print "About to run batch_test.pl\n";

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
# print output from .run script to stdout of this task, and final exit
######################################################################

chdir( "$SrcDir/$testdir/$compiler" ) || 
  c_die("Can't chdir($SrcDir/$testdir/$compiler): $!\n");
$run_out = "$testname.run.out";
$run_out_full = "$SrcDir/$testdir/$compiler/$run_out";

if( ! -f $run_out_full ) {
    if( $teststatus == 0 ) {
        # if the test passed but we don't have a run.out file, we
        # should consider that some kind of weird error
        c_die("ERROR: test passed but $run_out does not exist!");
    } else {
        # if the test failed, this isn't suprising.  we can print it, 
        # but we should just treat it as if the test failed, not an
        # internal error. 
        print "\n\nTest failed and $run_out does not exist\n";
    }
} else {
    # spit out the contents of the run.out file to the stdout of the task
    if( open(RES, "<$run_out_full") ) {
        print "\n\n----- Start of $run_out -----\n";
        while(<RES>) {
            print "$_";
        }
        close RES;
        print "\n----- End of $run_out -----\n";
    } else {
        print "\n\nERROR: failed to open $run_out_full: $!\n";
    }
}

exit $teststatus;


######################################################################
# helper methods
######################################################################

sub copy_file {
    my( $src, $dest ) = @_;
    copy($src, $dest);
    if( $? >> 8 ) {
        print "Can't copy $src to $dest: $!\n";
    } else {
        print "Copied $src to $dest\n";
    }
}

sub c_die {
    my( $msg ) = @_;
    print $msg;
    exit 3;
}
