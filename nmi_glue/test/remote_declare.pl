#!/usr/bin/env perl

######################################################################
# $Id: remote_declare.pl,v 1.1.4.1 2004-07-19 05:28:37 wright Exp $
# generate list of all tests to run
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $SrcDir = $ENV{SRC_DIR} || die "SRC_DIR not in environment!\n";

my $decl_type;
my $args = $ARGV[0];
if( $args ) {
    # passed special black-box args, do something smart with them.
    $decl_type = $args;
} else {
    $decl_type = "full";
}

# make sure that what we were given is valid and we recognize it
if( $decl_type eq "quick" ||
    $decl_type eq "fortran" ||
    $decl_type eq "gnu-c-fast" ||
    $decl_type eq "gnu-c-slow" ||
    $decl_type eq "gnu-cpp-fast" ||
    $decl_type eq "gnu-cpp-slow" ||
    $decl_type eq "vendor-c-fast" ||
    $decl_type eq "vendor-c-slow" ||
    $decl_type eq "vendor-cpp-fast" ||
    $decl_type eq "vendor-cpp-slow" ||
    $decl_type eq "full" )
{
    print "Declaring tests for type '$decl_type'\n";
} else {
    die "Unknown declare-type: '$decl_type'!\n";
}

# define a list of tests that are included in the 'slow' modes and
# exluded in the 'fast' modes...
my @slow_c = ( "big", "printer" );

open( TASKLIST, ">$BaseDir/tasklist.nmi" ) || 
    die "Can't open $BaseDir/tasklist.nmi: $!\n"; 

######################################################################
# run configure on the source tree
######################################################################

chdir( $SrcDir ) || die "Can't chdir($SrcDir): $!\n";
print "running CONFIGURE ...\n"; 
open( TESTCONFIG, "./configure --without-externals 2>&1 |") ||
    die "Can't open configure as a pipe: $!\n";
while ( <TESTCONFIG> ) {
  print $_;
}
close (TESTCONFIG);
$configstat = $?;
print "CONFIGURE returned a code of $configstat\n"; 
($configstat == 0) || die "CONFIGURE failed, aborting testsuite run.\n";  


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
	system( "make $compiler/Makefile" );
	chdir( $compiler ) || die "Can't chdir($compiler): $!\n";
	system( "make run_list" );
	open( "RUNLIST", "run_list" ) || die "cannot open run_list\n";
	while( <RUNLIST> ) {
	    chomp;
	    $testname = $_;
	    $taskname = "$compiler.$testname";

	    # Now, based on what declare type we're using, only print
	    # out tasks that we care about...

	    if( $decl_type eq "full" ) {
		# In 'full' mode, print all tests we have
		print TASKLIST "$taskname\n";

	    } elsif( $decl_type eq "fortran" ) {
		# In 'fortran' mode, print out all the fortran tests
		if( $compiler eq "g77" ||
		    $compiler eq "f77" ||
		    $compiler eq "f90" )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "gnu-c-fast" ) {
		# In 'gnu-c-fast' mode, use all gcc tests except the
		# really slow tests like big and printer.
		if( $compiler eq "gcc" &&
		    ! grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "gnu-c-slow" ) {
		# In 'gnu-c-slow' mode, use only the gcc tests we
		# excluded from the 'gnu-c-fast' mode above.
		if( $compiler eq "gcc" &&
		    grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "gnu-cpp-fast" ) {
		# In 'gnu-cpp-fast' mode, use all g++ tests except the
		# really slow tests like big and printer.
		if( $compiler eq "g++" &&
		    ! grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "gnu-cpp-slow" ) {
		# In 'gnu-cpp-slow' mode, use only the g++ tests we
		# excluded from the 'gnu-cpp-fast' mode above.
		if( $compiler eq "g++" &&
		    grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "vendor-c-fast" ) {
		# In 'vendor-c-fast' mode, use all cc tests except the
		# really slow tests like big and printer.
		if( $compiler eq "cc" &&
		    ! grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "vendor-c-slow" ) {
		# In 'vendor-c-slow' mode, use only the cc tests we
		# excluded from the 'vendor-c-fast' mode above.
		if( $compiler eq "cc" &&
		    grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "vendor-cpp-fast" ) {
		# In 'vendor-cpp-fast' mode, use all cc tests except the
		# really slow tests like big and printer.
		if( $compiler eq "CC" &&
		    ! grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "vendor-cpp-slow" ) {
		# In 'vendor-cpp-slow' mode, use only the cc tests we
		# excluded from the 'vendor-cpp-fast' mode above.
		if( $compiler eq "CC" &&
		    grep { $_ eq "$testname" } @slow_c )
		{
		    print TASKLIST "$taskname\n";
		}

	    } elsif( $decl_type eq "quick" ) {
		# In 'quick' mode, only use a subset for quick testing
		if( $compiler eq "g77" || 
		    $testname eq "env" || 
		    $testname eq "floats" || 
		    $testname eq "fgets" || 
		    $testname eq "fcntl" )
		{
		    print TASKLIST "$taskname\n";
		} else {
		    print "using quick tests, ignoring $compiler.$testname\n";
		}

	    } else {
		die "Impossible: unknown declare type: $decl_type after " .
		    "we already recognized it!\n";
	    }
	}
	close( RUNLIST );
    } 
    close( COMPILERS );
}     
close( TASKLIST );
