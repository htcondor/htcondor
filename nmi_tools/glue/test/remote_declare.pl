#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

use strict;
use warnings;
use Getopt::Long;
use vars qw/ @opt_testclasses /;
use File::Basename;
use File::Spec;
use File::Copy;

my $dir = dirname($0);
unshift @INC, $dir;
require "TestGlue.pm";

my $iswindows = TestGlue::is_windows();
my $iscygwin  = TestGlue::is_cygwin_perl();

TestGlue::print_debug_header();
TestGlue::setup_test_environment();

parseOptions();

my $appendFile = parseAppendFile();

######################################################################
# generate list of all tests to run
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $TaskFile = File::Spec->catfile($BaseDir, "tasklist.nmi");
my $UserdirTaskFile = File::Spec->catfile($BaseDir, "..", "tasklist.nmi");
my $testdir = "condor_tests";

######################################################################
# Handle testoverride file if one exists. this is not the normal case.
######################################################################
my $TestOverride = File::Spec->catfile($BaseDir, $testdir, "testoverride");
if ($iswindows) { system("dir /b /s"); } else { system("ls -R . "); }
print "Checking for test override file $TestOverride\n";
if (-f "$TestOverride") {
	print "****************************************************\n";
	print "**** Found test override file\n****   $TestOverride\n";
	print "****************************************************\n";
	#copy("$TestOverride", \*STDOUT);
	print "Open and list out:$TestOverride\n";
	open(TO,"<$TestOverride") or die "Failed to open:$TestOverride :$!\n";
	while (<TO>) {
		print "$_";
	}
	close(TO);
	print "****************************************************\n";
	print "**** Copying to $TaskFile, $UserdirTaskFile\n";
	print "**** and ignoring normal test lists/arguments\n";
	print "****************************************************\n";
	copy("$TestOverride", "$TaskFile");
	copy("$TestOverride", "$UserdirTaskFile");
	exit(0);
}

# Look for a specific file that contains overrides for the default timeouts for some tests
my %CustomTimeouts = load_custom_timeout_file();
my %RuncountChanges = load_custom_runcount_file();

# the skip file tells us to skip certain tests on this platform.
my %SkipList = load_platform_skip_list();

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
foreach my $class (@classlist) {
    print "****   $class\n";
}

# Make sure we can write to the tasklist file, and have the filehandle
# open for use throughout the rest of the script.
open(TASKFILE, '>', $TaskFile ) || die "Can't open $TaskFile for writing: $!\n"; 
open(USERTASKFILE, '>', $UserdirTaskFile ) || die "Can't open $UserdirTaskFile for writing: $!\n"; 


######################################################################
# For each testclass, generate the list of tests that match it
######################################################################

my %tasklist;

foreach my $class (@classlist) {
    print "****************************************************\n";
    print "**** Finding tests for class: '$class'\n";
    print "****************************************************\n";
    my $tests = findTests( $class, "top" );
    %tasklist = (%tasklist, %$tests);
}

my $total_tests = scalar(keys %tasklist);
print "-- Found $total_tests test(s) in all directories\n";

my $skipped_count = 0;
my $wrote_count = 0;

print "****************************************************\n";
print "**** Writing out tests to tasklist.nmi\n";
print "****************************************************\n";
foreach my $task (sort prio_sort keys %tasklist) {
    my $skip = defined($SkipList{$task}) ? 1 : 0;
    if ($skip) {
        $skipped_count += 1;
        next;
    }

    my $test_count = defined($RuncountChanges{$task}) ? $RuncountChanges{$task} : 1;

    my $prefix = "JOB ";
    my @serial_test_platforms = qw/x86_winnt_5.1 x86_64_winnt_6.1/;
    if(grep $_ eq $ENV{NMI_PLATFORM}, @serial_test_platforms) {
        $prefix = "";
    }

	print "\t$prefix$task\n";

    if(defined($CustomTimeouts{"$task"})) {
        print "CustomTimeout:$task $CustomTimeouts{$task}\n";
        foreach(1..$test_count) {
            print TASKFILE "$prefix$task-$_ $CustomTimeouts{$task}\n";
            print USERTASKFILE "$prefix$task-$_ $CustomTimeouts{$task}\n";
        }
    }
    else {
        foreach(1..$test_count) {
            print TASKFILE "$prefix$task-$_\n";
            print USERTASKFILE "$prefix$task-$_\n";
        }
    }
    $wrote_count += 1;
}
close( TASKFILE );
close( USERTASKFILE );
print "Wrote " . $wrote_count . " unique tests, skipped " . $skipped_count . ".\n";

exit(0);

# sort tests by the order in which we want them to run.
# using the hash value as the primary sort key (reversed because we want higher prio to go first)
# the task name (i.e the key) as will be the secondary sort key
sub prio_sort {
   my $ret = $tasklist{$b} <=> $tasklist{$a};
   if ( ! $ret) { $ret = $a cmp $b; }
   return $ret;
}

sub findTests {
    my( $classname, $dir_arg ) = @_;
    my ($ext, $dir);

    if( $dir_arg eq "top" ) {
        $ext = "";
        $dir = $testdir;
    }
    else {
        $ext = ".$dir_arg";
        $dir = "$testdir/$dir_arg";
    }
    print "-- Searching directory '$dir' for tests with class '$classname'\n";
    chdir( "$BaseDir/$dir" ) || die "Can't chdir($BaseDir/$dir): $!\n";

    my $list_target = "list_$classname";
    open(LIST, '<', $list_target) || die "cannot open $list_target: $!\n";
    my %tasklist = map { chomp; "$_$ext" => 1 } <LIST>;
    close(LIST);

    # set (rough) priorities for some of the tests.
    foreach (keys %tasklist) {
	# these few are slow, start them first to get them out of the long tail
	if ($_ =~ /test_late_material/) { $tasklist{$_} += 20; }
	if ($_ =~ /log_rotation/) { $tasklist{$_} += 20; }
	if ($_ =~ /job_dagman_splice-N/) { $tasklist{$_} += 20; }
	if ($_ =~ /job_dagman_splice-O/) { $tasklist{$_} += 20; }
	if ($_ =~ /job_basic_preempt/) { $tasklist{$_} += 20; }

        if ($_ =~ /basic/) { $tasklist{$_} += 7; }
        if ($_ =~ /^cmd_/) { $tasklist{$_} += 2; }
        #if ($_ =~ /_core_/) { $tasklist{$_} += 2; }
        if ($_ =~ /concurrency/) { $tasklist{$_} += 10; } # these are very slow tests.
    }

    print join("\n", sort keys %tasklist) . "\n";

    my $total = scalar(keys %tasklist);
    print "-- Found $total test(s) in directory '$dir' for class '$classname'\n\n";
    return \%tasklist;
}

sub usage {
    print <<EOF;
--help          This help.
--test-class    Which test class to run, comma separated or multiple occurrence.
EOF
	
    exit 1;
}

# We use -- to delineate the boundary between args to this script and args to
# the configure in this script.
sub parseOptions {
    print "Script called with ARGV: " . join(' ', @ARGV) . "\n";

    my $rc = GetOptions('test-class=s' => \@opt_testclasses,
                        'help'         => \&usage,
                        );

    if( !$rc ) {
        usage();
    }

    # allow comma separated list in addition to multiple occurrences.
    @opt_testclasses = split(/,/, join(',', @opt_testclasses));

    #if (!defined(@opt_testclasses)) {
        #die "Please supply a test class!\n";
    #}
}


sub load_custom_timeout_file {
    my %timeouts = ();
    my $TimeoutFile = File::Spec->catfile($BaseDir, $testdir, "TimeoutChanges");
    if( -f $TimeoutFile) {
        print "Found a custom timeout file at '$TimeoutFile'.  Loading it...\n";
        open(TIMEOUTS, '<', $TimeoutFile) || die "Failed to open $TimeoutFile for reading: $!\n";
        while(<TIMEOUTS>) {
            if(/^\s*([\-\w]*)\s+(\d*)\s*$/) {
                print "\tCustom Timeout: $1:$2\n";
                $timeouts{$1} = $2;
            }
        }
        close(TIMEOUTS);
    }
    else {
        print "INFO: No custom timeout file found.\n";
    }

    return %timeouts;
}


sub load_custom_runcount_file {
    my %runcounts = ();
    my $RuncountFile = File::Spec->catfile($BaseDir, $testdir, "RuncountChanges");
    if( -f $RuncountFile) {
        print "Found a custom runcount file at '$RuncountFile'.  Loading it...\n";
        open(RUNCOUNT, '<', $RuncountFile) || die "Failed to open $RuncountFile for reading: $!\n";
        while(<RUNCOUNT>) {
            if(/^\s*([\w\-]+)\s+(\d*)\s*$/) {
                print "\tCustom Runcount: $1:$2\n";
                $runcounts{$1} = $2;
            }
        }
        close(RUNCOUNT);
    }
    else {
        print "INFO: No custom runcount file found.\n";
    }

    return %runcounts;
}

sub load_platform_skip_list {
    my %skips = ();

    my $filename = "$ENV{NMI_PLATFORM}_SkipList";
    my $SkipFile = File::Spec->catfile($BaseDir, $testdir, "$filename");

    if( -f $SkipFile) {
        print "Found a platform skip file at '$SkipFile'.  Loading it...\n";
        open(SKIPS, '<', $SkipFile) || die "Failed to open $SkipFile for reading: $!\n";
        while(<SKIPS>) {
            if(/^\s*([\-\w]*)\s*$/) {
                print "\tSkip: $1\n";
                $skips{$1} = 1;
            }
        }
        close(SKIPS);
    }

    # In addition to a platform specific skip file, Windows gets a generic skip file as well.
    if (TestGlue::is_windows()) {
       if ( ! -f $SkipFile) { 
          $filename = "Windows_SkipList";
          $SkipFile = File::Spec->catfile($BaseDir, $testdir, "$filename");
          if( -f $SkipFile) {
             print "Found a Windows skip file at '$SkipFile'.  Loading it...\n";
             open(SKIPS, '<', $SkipFile) || die "Failed to open $SkipFile for reading: $!\n";
             while(<SKIPS>) {
                if(/^\s*([\-\w]*)\s*$/) {
                    print "\tSkip: $1\n";
                    $skips{$1} = 1;
                }
             }
             close(SKIPS);
          }
       }
    }

    return %skips;
}

#
# Handle not declaring IPv4-only tests.
#

sub parseAppendFile {
	my $appendFile = ();

	my $appendConfigFile = "testconfigappend";
	my @dirs = ( $ENV{BASE_DIR}, "condor_tests" );
	foreach my $dir (@dirs) {
		my $fullPath = File::Spec->catfile( $dir, $appendConfigFile );
		if( -f $fullPath ) {
			$appendConfigFile = $fullPath;
			last;
		}
	}

	if( -f $appendConfigFile ) {
		open( PAF, "< ${appendConfigFile}" ) or die( "Unable to open '$appendConfigFile', aborting.\n" );

		while( my $line = <PAF> ) {
			TestGlue::fullchomp( $line );
			if( $line =~ m/^\s*([^=\s]+)\s*=\s*([^\s]*)\s*$/ ) {
				my( $attr, $value ) = ( $1, $2 );
				print( "Found config to append: '$attr' = '$value'.\n" );
				$appendFile->{ $attr } = $value;
			}
		}

		close( PAF );
	} else {
		print( "'$appendConfigFile' does not exist, not parsing it.\n" );
	}

	return $appendFile;
}
