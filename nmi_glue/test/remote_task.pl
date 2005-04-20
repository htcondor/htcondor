#!/usr/bin/env perl

######################################################################
# $Id: remote_task.pl,v 1.1.2.18 2005-04-20 17:25:08 bgietzel Exp $
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
system( "make CondorPersonal.pm" );
if( $? >> 8 ) {
    c_die("Can't build CondorPersonal.pm\n");
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
    mkdir( "$BaseDir/results", 0777 ) || 
	c_die("Can't mkdir($BaseDir/results): $!\n");
}
if( $compiler eq "." ) {
    $resultdir = "$BaseDir/results/base";
} else {
    $resultdir = "$BaseDir/results/$compiler";
}
if( ! -d "$resultdir" ) {
    mkdir( "$resultdir", 0777 ) || c_die("Can't mkdir($resultdir): $!\n");
}
chdir( "$SrcDir/$testdir/$compiler" ) || 
    c_die("Can't chdir($SrcDir/$testdir/$compiler): $!\n");

$copy_failure = 0;
$have_run_out_file = 0;

# Here is where we need to copy files we wanna save for humans
# to later figure out why this test most likely failed.  :).

# first we will copy the files that MUST be there.  we do this with
# safe_copy(), which will flag the test as failed if the files 
# do not exist or cannot be copied.  Variable copy_failure is
# being set as a side-effect of function safe_copy().  Isn't perl
# great?  Every function can have side-effects.  Mmmmm... functional
# programming apparently is lost upon the Perl-addicts of the world.
safe_copy( "$testname.run.out", $resultdir  );
$have_run_out_file = $copy_failure;
# TODO : what happened to run.err ?

# these files are all optional.  if they exist, we'll save 'em, if
# they do not, we don't worry about it.
unsafe_copy( "$testname.out*", $resultdir );
unsafe_copy( "$testname.err*", $resultdir  );
unsafe_copy( "$testname.log", $resultdir  );
unsafe_copy( "$testname.cmd.out", $resultdir  );

# try to tarup a 'saveme' subdirectory for this test, which may
# contain test debug info etc.
system( "tar zcf $testname.saveme.tar.gz $testname.saveme/" );
if( $? >> 8 ) {
	print "No $testname.saveme subdir exists, fine.\n";
} else {
	print "Created $testname.saveme.tar.gz.\n";
	safe_copy( "$testname.saveme.tar.gz", $resultdir  );
}


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

print "\n\n----- Start of $testname.run.out ----\n";
if( $have_run_out_file ) {
	# no run.out file... ?
	print "   (( Did not exist!!! ))\n";
} else {
	# spit out the contents of the run.out file to the stdout of
	if( open(RES,"<$testname.run.out") ) {
		while(<RES>)
		{
			print "$_";
		}
		close RES;
	} else {
		print "   (( Failed to open $testname.run.out ))\n";
	}
}
print "\n----- End of $testname.run.out ----\n";


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

sub unsafe_copy {
    my( $src, $dest ) = @_;
    system( "cp $src $dest" );
    if( $? >> 8 ) {
	print "   Optional file $src not copied into $dest: $\n";
    } else {
	print "Copied $src to $dest\n";
    }
}

sub c_die {
    my( $msg ) = @_;
    print $msg;
    exit 3;
}
