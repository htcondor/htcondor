#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

use Getopt::Long;
use vars qw/ @opt_testclasses $opt_help /;

parseOptions();

######################################################################
# generate list of all tests to run
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $SrcDir = $ENV{SRC_DIR} || die "SRC_DIR not in environment!\n";
my $TaskFile = "$BaseDir/tasklist.nmi";
my $UserdirTaskFile = "$BaseDir/../tasklist.nmi";

my %CustomTimeouts;
my $TimeoutFile = "$SrcDir/condor_tests/TimeoutChanges";
# Do we have a file with non-default timeouts for some tests?
if( -f "$TimeoutFile") {
	open(TIMEOUTS,"<$TimeoutFile") || die "Failed to open $TimeoutFile: $!\n";
	my $line;
	while(<TIMEOUTS>) {
		chomp($_);
		$line = $_;
		if($line =~ /^\s*([\-\w]*)\s+(\d*)\s*$/) {
			print "Custom Timeout: $1:$2\n";
			$CustomTimeouts{"$1"} = $2;
		}
	}
	close(TIMEOUTS);
}

my %RuncountChanges;
my $RuncountFile = "$SrcDir/condor_tests/RuncountChanges";
# Do we have a file with non-default runtimes for some tests?
if( -f "$RuncountFile") {
    open(RUNCOUNT,"<$RuncountFile") || die "Failed to open $RuncountFile: $!\n";
    my $line;
    while(<RUNCOUNT>) {
        chomp($_);
        $line = $_;
        if($line =~ /^\s*([\w\-]+)\s+(\d*)\s*$/) {
            print "Custom Runcount: $1:$2\n";
            $RuncountChanges{"$1"} = $2;
        }
    }
    close(RUNCOUNT);
}
# file which contains the list of tests to run on Windows
my $WinTestList = "$SrcDir/condor_tests/Windows_list";
my $ShortWinTestList = "$SrcDir/condor_tests/Windows_shortlist";

# Figure out what testclasses we should declare based on our
# command-line arguments.  If none are given, we declare the testclass
# "all", which is *all* the tests in the test suite.
my @classlist = @opt_testclasses;
if( ! @classlist ) {
    push( @classlist, "all" );
}

# The rest of argv, after the -- are any additional configure arguments.
my $configure_args = join(' ', @ARGV);

print "****************************************************\n";
print "**** Preparing to declare tests for these classes:\n";
foreach $class (@classlist) {
    print "****   $class\n";
}

# Make sure we can write to the tasklist file, and have the filehandle
# open for use throughout the rest of the script.
open( TASKFILE, ">$TaskFile" ) || die "Can't open $TaskFile: $!\n"; 
open( USERTASKFILE, ">$UserdirTaskFile" ) || die "Can't open $UserdirTaskFile: $!\n"; 


######################################################################
# run configure on the source tree
######################################################################

if( !($ENV{NMI_PLATFORM} =~ /winnt/) )
{
	chdir( $SrcDir ) || die "Can't chdir($SrcDir): $!\n";
	print "****************************************************\n";
	print "**** running CONFIGURE ...\n"; 
	print "****************************************************\n";
	print "./configure $configure_args\n";
	open( TESTCONFIG, "./configure $configure_args 2>&1 |") ||
    	die "Can't open configure as a pipe: $!\n";
	while ( <TESTCONFIG> ) {
    	print $_;
	}
	close (TESTCONFIG);
	$configstat = $?;
	print "CONFIGURE returned a code of $configstat\n"; 

	if($configstat != 0) {
		print "Let's try without gcc version check and disabling the full " .
			"port!\n";

		print "****************************************************\n";
		print "**** running CONFIGURE (again)...\n"; 
		print "****************************************************\n";
		$extra_try_args =  " --disable-gcc-version_check --disable-full-port ";
		print "./configure $extra_try_args $configure_args\n";
		open( TESTCONFIG, "./configure $extra_try_args $configure_args 2>&1 |") ||
    		die "Can't open configure as a pipe: $!\n";
		while ( <TESTCONFIG> ) {
    		print $_;
		}
		close (TESTCONFIG);
		$configstat = $?;
		print "CONFIGURE returned a code of $configstat\n"; 
	}

	($configstat == 0) || die "CONFIGURE failed, aborting testsuite run.\n";  
}

######################################################################
# generate compiler_list and each Makefile we'll need
######################################################################

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

	@compilers = ();
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

######################################################################
# For each testclass, generate the list of tests that match it
######################################################################

my %tasklist;
my $total_tests;

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
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
} else {
    # eat the file Windows_list into tasklist hash
	foreach $class (@classlist) {
    	print "****************************************************\n";
    	print "**** Finding Windows tests \"$class\"\n";
    	print "****************************************************\n";
		if($class eq "quick") {
    		open( WINDOWSTESTS, "<$WinTestList" ) || die "Can't open $WinTestList: $!\n";
		} elsif($class eq "short") {
    		open( WINDOWSTESTS, "<$ShortWinTestList" ) || die "Can't open $ShortWinTestList: $!\n";
		} else {
			# if things got confused just run the hourly tests
    		open( WINDOWSTESTS, "<$ShortWinTestList" ) || die "Can't open $ShortWinTestList: $!\n";
		}
    	$total_tests = 0;
    	$testnm = "";
    	while(<WINDOWSTESTS>) {
        	chomp();
        	$testnm = $_;
        	if( $testnm =~ /^\s*#.*/) {
            	# skip the comment
        	} else {
            	$total_tests += 1;
            	$tasklist{$testnm} = 1;
        	}
    	}
	}
}

if( $total_tests == 1) {
	$word = "test";
} else {
	$word = "tests";
}
print "-- Found $total_tests $word in all " .
	"directories\n";

print "****************************************************\n";
print "**** Writing out tests to tasklist.nmi\n";
print "****************************************************\n";
my $unique_tests = 0;
my $repeat_test;
foreach $task (sort keys %tasklist ) {
	$tempt = $CustomTimeouts{"$task"};
    $tempr = $RuncountChanges{"$task"};
    if( exists $RuncountChanges{"$task"} ) {
        $repeat_test = $tempr;
    } else {
        $repeat_test = 1;
    }

    if( exists $CustomTimeouts{"$task"} ) {
        foreach(1..$repeat_test) {
            print TASKFILE $task . "-" . $_ . " " . $CustomTimeouts{"$task"} . "\n";
            print USERTASKFILE $task . "-" . $_ .  " " . $CustomTimeouts{"$task"} . "\n";
        }
        print "CustomTimeout:$task $tempt\n";
    } else {
        foreach(1..$repeat_test) {
            print TASKFILE $task . "-" . $_ . "\n";
            print USERTASKFILE $task . "-" . $_ . "\n";
        }
    }
    $unique_tests++;
}
close( TASKFILE );
close( USERTASKFILE );
print "Wrote $unique_tests unique tests\n";
exit(0);

sub findTests () {
    my( $classname, $dir_arg ) = @_;
    my ($ext, $dir);
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

sub usage
{
print <<EOF;
--help          This help.
--test-class    Which test class to run, comma separated or multiple ocurrance.
EOF
	
	exit 0;
}

# We use -- to delineate the boundary between args to this script and args to
# the configure in this script.
sub parseOptions
{
	my $rc;

	print "Script called with ARGV: " . join(' ', @ARGV) . "\n";

	$rc = GetOptions(
		'test-class=s'		=> \@opt_testclasses,
		'help'				=> \$opt_help,
	);

	if( !$rc ) {
		usage();
	}

	if (defined($opt_help)) {
		usage();
	}

	# allow comma separated list in addition to multiple occurrances.
	@opt_testclasses = split(/,/, join(',', @opt_testclasses));

	if (!defined(@opt_testclasses)) {
		die "Please supply a test class!\n";
	}
}




