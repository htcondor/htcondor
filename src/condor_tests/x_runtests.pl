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

use Cwd;
use POSIX ":sys_wait_h";

######################################################################
# x_runtests.pl
# generate list of all tests to run
######################################################################

my $BaseDir = getcwd();
my $SrcDir = $BaseDir ;
my $TaskFile = "$BaseDir/tasklist.nmi";

# file which contains the list of tests to run on Windows
my $WinTestList = "$SrcDir/Windows_list";

# set up to recover from tests which hang
$SIG{ALRM} = sub { die "timeout" };

print "Starting tests now: ";
print scalar localtime() . "\n";

my @classlist;
my $timeout = 0;

while( $_ = shift( @ARGV ) ) {
  SWITCH: {
        if( /-c.*/ ) {
                push(@classlist, shift(@ARGV));
                next SWITCH;
        }
        if( /-t.*/ ) {
                $timeout= shift(@ARGV);
                next SWITCH;
        }
  }
}

# Figure out what testclasses we should declare based on our
# command-line arguments.  If none are given, we declare the testclass
# "all", which is *all* the tests in the test suite.
if( ! @classlist ) {
    push( @classlist, "quick" );
}

if($timeout == 0) {
	$timeout = 1200;
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

######################################################################
# generate compiler_list and each Makefile we'll need
######################################################################

if( !($ENV{NMI_PLATFORM} =~ /_win/i) )
{
	print "****************************************************\n";
	print "**** Creating \n"; 
	print "****************************************************\n";

	$testdir = ".";

	chdir( $SrcDir ) || die "Can't chdir($SrcDir): $!\n";

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

if( !($ENV{NMI_PLATFORM} =~ /_win/i) ) {
	foreach $class (@classlist) {
    	print "****************************************************\n";
    	print "**** Finding tests for class: \"$class\"\n";
    	print "****************************************************\n";
    	$total_tests = 0;    
    	$total_tests += findandrunTests( $class, "top" );
    	foreach $cmplr (@compilers) {
			$total_tests += findandrunTests( $class, $cmplr );
    	}
	}
} else {
    # eat the file Windows_list into tasklist hash
    print "****************************************************\n";
    print "**** Finding tests for file \"$WinTestList\"\n";
    print "****************************************************\n";
    open( WINDOWSTESTS, "<$WinTestList" ) || die "Can't open $WinTestList: $!\n";
    $class = $WinTestList;
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
print scalar localtime() . "\n";

exit(0);

sub findandrunTests () {
    my( $classname, $dir_arg ) = @_;
    my ($ext, $dir);
    my $total = 0;
	my $pid;

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
		#print;
		chomp;
		$taskname = $_ . $ext;
		$total++;
		$tasklist{$taskname} = 1;

		eval {
			alarm($timeout);
			defined($pid = fork) || die "Could not fork!\n";
			unless($pid) {
				#child here
				system("$SrcDir/batch_test.pl -d . -t $taskname -q");
				#print "my pid is $pid\n";
				#print "$taskname\n";
				exit(0);
			}
			# parent here
			#print "parent pid is $pid\n";
			$resp = 0;
			while(($resp = waitpid($pid,&WNOHANG)) < 1) {
				#print "waiting on child sleep\n";
				sleep 15;
			}
			alarm(0);
		};

		if($@) {
			if($@ =~ /timeout/) {
				print "$taskname 			Timeout\n";
				kill 9, (0 - $pid);
			} else {
				alarm(0);
			}
		}

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

