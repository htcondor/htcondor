#!/usr/bin/env perl

######################################################################
# $Id: remote_declare.pl,v 1.1.4.3 2006-03-14 19:16:14 gquinn Exp $
# generate list of all tests to run
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $SrcDir = $ENV{SRC_DIR} || die "SRC_DIR not in environment!\n";
my $TaskFile = "$BaseDir/tasklist.nmi";
my $windowstests = "Windows_list";

# Figure out what testclasses we should declare based on our
# command-line arguments.  If none are given, we declare the testclass
# "all", which is *all* the tests in the test suite.
my @classlist = @ARGV;
if( ! @classlist ) {
    push( @classlist, "all" );
}

print "****************************************************\n";
print "**** Prepairing to declare tests for these classes:\n";
foreach $class (@classlist) {
    print "****   $class\n";
}

# Make sure we can write to the tasklist file, and have the filehandle
# open for use throughout the rest of the script.
open( TASKFILE, ">$TaskFile" ) || die "Can't open $TaskFile: $!\n"; 


######################################################################
# run configure on the source tree
######################################################################

# On windows we simply have a list to fetch in!
if( !($ENV{NMI_PLATFORM} =~ /winnt/) )
{
	chdir( $SrcDir ) || die "Can't chdir($SrcDir): $!\n";
	print "****************************************************\n";
	print "**** running CONFIGURE ...\n"; 
	print "****************************************************\n";
	open( TESTCONFIG, "./configure --without-externals 2>&1 |") ||
    	die "Can't open configure as a pipe: $!\n";
	while ( <TESTCONFIG> ) {
    	print $_;
	}
	close (TESTCONFIG);
	$configstat = $?;
	print "CONFIGURE returned a code of $configstat\n"; 
	($configstat == 0) || die "CONFIGURE failed, aborting testsuite run.\n";  
}


######################################################################
# generate compiler_list and each Makefile we'll need
######################################################################

# On windows we simply have a list to fetch in!
if( !($ENV{NMI_PLATFORM} =~ /winnt/) )
{

	print "****************************************************\n";
	print "**** Creating Makefiles\n"; 
	print "****************************************************\n";

	$testdir = "condor_tests";

	chdir( $SrcDir ) || die "Can't chdir($SrcDir): $!\n";
	print "Creating $testdir/Makefile\n";
	doMake( "$testdir/Makefile" );

	chdir( $testdir ) || die "Can't chdir($testdir): $!\n";
	# First, generate the list of all compiler-specific subdirectories. 
	doMake( "compiler_list" );

	my @compilers = ();
	open( COMPILERS, "compiler_list" ) || die "cannot open compiler_list: $!\n";
	while( <COMPILERS> ) {
    	chomp;
    	push @compilers, $_;
	}
	close( COMPILERS );
	print "Found compilers: " . join(' ', @compilers) . "\n";
	foreach $cmplr (@compilers) {
    print "Creating $testdir/$cmplr/Makefile\n";
    	doMake( "$cmplr/Makefile" );
	}

	doMake( "list_testclass" );
	foreach $cmplr (@compilers) {
    	$cmplr_dir = "$SrcDir/$testdir/$cmplr";
    	chdir( $cmplr_dir ) || die "cannot chdir($cmplr_dir): $!\n"; 
    	doMake( "list_testclass" );
	}
}
else
{
    print "****************************************************\n";
    print "**** Locating to test directory to build Windows test list\n";
    print "****************************************************\n";

    $testdir = "condor_tests";

    chdir( $SrcDir ) || die "Can't chdir($SrcDir): $!\n";
    chdir( $testdir ) || die "Can't chdir($testdir): $!\n";
}

######################################################################
# For each testclass, generate the list of tests that match it
######################################################################

my %tasklist;
my $total_tests;

# On windows we simply have a list to fetch in!
if( !($ENV{NMI_PLATFORM} =~ /winnt/) )
{
	foreach $class (@classlist) {
    	print "****************************************************\n";
    	print "**** Finding tests for class: \"$class\"\n";
    	print "****************************************************\n";
    	$total_tests = 0;    
    	$total_tests += findTests( $class, "top" );
    	foreach $cmplr (@compilers) {
			$total_tests += findTests( $class, $cmplr );
    	}
	}
}
else
{
    # eat the file Windows_list into tasklist hash
    print "****************************************************\n";
    print "**** Finding tests for file \"Windows_list\"\n";
    print "****************************************************\n";
    open( WINDOWSTESTS, "<$windowstests" ) || die "Can't open $windowstests: $!\n";
    $class = $windowstests;
    $total_tests = 0;
    $testnm = "";
    while(<WINDOWSTESTS>)
    {
        chomp();
        $testnm = $_;
        $total_tests += 1;
        $tasklist{$testnm} = 1;
    }
}

if( $total_tests == 1) {
	$word = "test";
} else {
	$word = "tests";
}
print "-- Found $total_tests $word for \"$class\" in all " .
	"directories\n";

print "****************************************************\n";
print "**** Writing out tests to tasklist.nmi\n";
print "****************************************************\n";
$unique_tests = 0;
foreach $task (sort keys %tasklist ) {
    print TASKFILE $task . "\n";
    $unique_tests++;
}
close( TASKFILE );
print "Wrote $unique_tests unique tests\n";
exit(0);

sub findTests () {
    my( $classname, $dir_arg ) = @_;
    my $ext, $dir;
    my $total = 0;

    if( $dir_arg eq "top" ) {
	$ext = "";
	$dir = $testdir;
    } else {
	$ext = ".$dir_arg";
	$dir = "$testdir/$dir_arg";
    }
    print "-- Searching \"$dir\" for \"$classname\"\n";
    chdir( "$SrcDir/$dir" ) || die "Can't chdir($SrcDir/$dir): $!\n";

    $list_target = "list_" . $classname;
    doMake( $list_target );

    open( LIST, $list_target ) || die "cannot open $list_target: $!\n";
    while( <LIST> ) {
	print;
	chomp;
	$taskname = $_ . $ext;
	$total++;
	$tasklist{$taskname} = 1;
    }
    if( $total == 1 ) {
	$word = "test";
    } else {
	$word = "tests";
    }
    print "-- Found $total $word in \"$dir\" for \"$classname\"\n\n";
    return $total;
}


sub doMake () {
    my( $target ) = @_;
    my @make_out;
    open( MAKE, "make $target 2>&1 |" ) || 
	die "\nCan't run make $target\n";
    @make_out = <MAKE>;
    close( MAKE );
    $makestatus = $?;
    if( $makestatus != 0 ) {
	print "\n";
	print @make_out;
	die("\"make $target\" failed!\n");
    }
}

