#!/usr/bin/perl

######################################################################
# $Id: remote_declare.pl,v 1.1.2.7 2004-06-25 00:03:49 wright Exp $
# generate list of all tests to run
######################################################################

my $BaseDir = $ENV{BASE_DIR};
my $SrcDir = $ENV{SRC_DIR};

my $decl_type;
my $args = $ARGV[0];
if( $args ) {
    # passed special black-box args, do something smart with them.
    $decl_type = $args;
} else {
    $decl_type = "full";
}

# make sure that what we were given is valid and we recognize it
if( $decl_type =~ /^quick$/ ||
    $decl_type =~ /^full$/ )
{
    print "Declaring tests for type '$decl_type'\n";
} else {
    die "Unknown declare-type: '$decl_type'!\n";
}


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
	system( "make $compiler/Makefile" );
	chdir( $compiler ) || die "Can't chdir($compiler): $!\n";
	system( "make run_list" );
	open( "RUNLIST", "run_list" ) || die "cannot open run_list\n";
	while( <RUNLIST> ) {
	    chomp;
	    $testname = $_;
	    if( $decl_type =~ /^full$/ ) {
		# In 'full' mode, print all tests we fine
		print TASKLIST "$compiler.$testname\n";
	    } elsif( $decl_type =~ /^quick$/ ) {
		# In 'quick' mode, only use a subset for quick testing
		if( $compiler =~ /g77/ || 
		    $testname =~ /^env/ || 
		    $testname =~ /floats/ || 
		    $testname =~ /fgets/ || 
		    $testname =~ /fcntl/ )
		{
		    print TASKLIST "$compiler.$testname\n";
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
