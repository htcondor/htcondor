#!/usr/bin/perl

######################################################################
# $Id: remote_declare.pl,v 1.1.2.4 2004-06-24 19:39:08 wright Exp $
# generate list of all tests to run
######################################################################

use Cwd;

my $BaseDir = $ENV{BASE_DIR};
my $SrcDir = $ENV{SRC_DIR};

open( TASKLIST, ">$BaseDir/tasklist.nmi" ) || 
    die "Can't open $BaseDir/tasklist.nmi: $!\n"; 

######################################################################
# generate makefile, compiler_list and run_list for each test group 
######################################################################

chdir( $SrcDir ) || die "Can't chdir($SrcDir): $!\n";

my $testdirs = ();
push (@testdirs, "condor_test_suite_C.V5");
push (@testdirs, "condor_test_suite_F.V5");

foreach $testdir (@testdirs) {
    chdir( "$SrcDir" ) || die "Can't chdir($SrcDir): $!\n";
    system( "make $testdir/Makefile" );
    chdir( "$SrcDir/$testdir" ) || die "Can't chdir($SrcDir/$testdir): $!\n";
    system( "make compiler_list" );
    open( "COMPILERS", "compiler_list" ) || die "cannot open compiler_list\n";
    while( <COMPILERS> ) {
	chomp;
	$compiler = $_;
	chdir( "$SrcDir/$testdir" ) || 
	    die "Can't chdir($SrcDir/$testdir): $!\n";
	print "Working on $compiler\n";
	print "cwd is: " . getcwd() . "\n";
	system( "make $compiler/Makefile" );
	chdir( $compiler ) || die "Can't chdir($compiler): $!\n";
	system( "make run_list" );
	open( "RUNLIST", "run_list" ) || die "cannot open run_list\n";
	while( <RUNLIST> ) {
	    chomp;
	    $testname = $_;
	    print TASKLIST "$compiler/$testname\n";
	}
	close( RUNLIST );
    } 
    close( COMPILERS );
}     
close( TASKLIST );
