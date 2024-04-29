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

######################################################################
# script to drive Condor build steps (user tasks)
######################################################################
use strict;
use warnings;
use Cwd;

use Getopt::Long;
use vars qw/ $opt_coverity_analysis /;
GetOptions(
            'coverity-analysis' => \$opt_coverity_analysis,
);

my $EXTERNALS_TASK        = "remote_task-externals";
my $BUILD_TASK            = "remote_task-build";
my $TAR_TASK              = "remote_task-create_tar";
my $TAR_TESTS_TASK        = "remote_task-create_tests_tar";
my $CHECK_TAR_TASK        = "remote_task-check_tar";
my $UNSTRIPPED_TASK       = "remote_task-create_unstripped_tar";
my $CHECK_UNSTRIPPED_TASK = "remote_task-check_unstripped_tar";
my $NATIVE_DEBUG_TASK     = "remote_task-create_native_unstripped";
my $NATIVE_TASK           = "remote_task-create_native";
my $CHECK_NATIVE_TASK     = "remote_task-check_native";
my $BUILD_TESTS_TASK      = "remote_task-build_tests";
my $RUN_UNIT_TESTS        = "remote_task-run_unit_tests";
my $COVERITY_ANALYSIS     = "remote_task-coverity_analysis";
my $EXTRACT_TARBALLS_TASK = "remote_task-extract_tarballs";

# autoflush our STDOUT
$| = 1;

my $boos = 1; # build out of source
if ($ENV{NMI_PLATFORM} =~ /AlmaLinux|AmazonLinux|CentOS|Fedora|openSUSE|Rocky|Debian|Ubuntu/) {
	$boos = 0; # No need to shuffle stuff around streamlined builds
}
if ($boos) {
    # rewrite the directory structures so we can do out of source builds.
	# we start in dir_nnn/userdir, which is the git clone
	# we want to move most of the content of that to dir_nnn/sources
	# leaving dir_nnn/userdir to hold the build products
	# Because of some stupididity in the tests, we also have to duplicate parts of the source tree into userdir
	chdir '..';
	my $nmi_dir = cwd;
	print "Converting to out-of-source builds by moving most of $nmi_dir/userdir to $nmi_dir/sources\n";

	# we create scripts to do the moving of files because we want to tar some stuff
	# and because perl's move doesn't work for directories on all platforms....

	mkdir 'sources';
	open(FH, '>', 'swap_userdir.cmd') or print "Cant open swap_userdir.cmd for writing: $!\n";
	if ($ENV{NMI_PLATFORM} =~ /_win/i) {
		print FH 'mkdir sources\msconfig' . "\n";
		print FH 'for %%I in (userdir\msconfig\*) do copy %%I sources\msconfig\%%~nxI' . "\n";
		print FH 'move userdir\msconfig\WiX sources\msconfig\WiX' . "\n";
		print FH '"sources\msconfig\tar.exe" -czf swap_userdir.tgz userdir/BUILD-ID userdir/GIT-SHA userdir/nmi_tools userdir/src/condor_examples userdir/src/condor_tests' . "\n";
		print FH 'del userdir\condor-*.tgz' . "\n";
		print FH 'for %%I in (userdir\*) do move %%I sources' . "\n";
		print FH 'move userdir\src sources\src' . "\n";
		print FH 'move userdir\bindings sources\bindings' . "\n";
		print FH 'move userdir\docs sources\docs' . "\n";
		print FH 'move userdir\view sources\view' . "\n";
		print FH 'move userdir\build sources\build' . "\n";
		print FH 'move userdir\externals sources\externals' . "\n";
		print FH 'move userdir\nmi_tools sources\nmi_tools' . "\n";
		print FH 'move userdir\src sources\src' . "\n";
		print FH '"sources\msconfig\tar.exe" -xvf swap_userdir.tgz' . "\n";
		# try the necessary directories again, because sometimes we get AccessDenied the first time.
		print FH 'move userdir\docs sources\docs' . "\n";
		print FH 'move userdir\view sources\view' . "\n";
		print FH 'move userdir\build sources\build' . "\n";
		print FH 'move userdir\externals sources\externals' . "\n";
	} else {
		print FH '#!/bin/sh' . "\n";
		print FH 'tar czf swap_userdir.tgz userdir/BUILD-ID userdir/GIT-SHA userdir/nmi_tools userdir/src/condor_examples userdir/src/condor_tests' . "\n";
		print FH 'for file in userdir/*; do if [ -f "$file" ]; then mv "$file" sources; fi; done' . "\n";
		print FH 'mv userdir/src sources' . "\n";
		print FH 'mv userdir/bindings sources' . "\n";
		print FH 'mv userdir/docs sources' . "\n";
		print FH 'mv userdir/view sources' . "\n";
		print FH 'mv userdir/build sources' . "\n";
		print FH 'mv userdir/externals sources' . "\n";
		print FH 'mv userdir/msconfig sources' . "\n";
		print FH 'mv userdir/nmi_tools sources' . "\n";
		print FH 'tar xvf swap_userdir.tgz' . "\n";
		print FH 'mv sources/condor-*.tgz userdir' . "\n";
	}
	close(FH);
	chmod(0777,'swap_userdir.cmd');
	system ("$nmi_dir/swap_userdir.cmd");
	chdir 'userdir';
}


my $tasklist_file = "tasklist.nmi";

######################################################################
# Detecting platform and generating task list
######################################################################
print "Generating tasklist...\n";

open(TASKLIST, '>', $tasklist_file) or die "Can't open $tasklist_file for writing: $!\n";

if ($ENV{NMI_PLATFORM} =~ /_win/i) {
    #Windows
    print TASKLIST "$EXTERNALS_TASK 4h\n";
    print TASKLIST "$BUILD_TASK 4h\n";
    print TASKLIST "$BUILD_TESTS_TASK 4h\n";
    print TASKLIST "$NATIVE_TASK 4h\n";
    print TASKLIST "$TAR_TASK 4h\n";
    print TASKLIST "$TAR_TESTS_TASK 4h\n";
    print TASKLIST "$RUN_UNIT_TESTS 4h\n";
}    
elsif ($ENV{NMI_PLATFORM} =~ /AlmaLinux|AmazonLinux|CentOS|Fedora|openSUSE|Rocky|Debian|Ubuntu/) {
    print TASKLIST "$NATIVE_TASK 4h\n";
    print TASKLIST "$CHECK_NATIVE_TASK 4h\n";
    print TASKLIST "$EXTRACT_TARBALLS_TASK 4h\n";
    print TASKLIST "$CHECK_TAR_TASK 4h\n";
}
else {
    # Everything else
    print TASKLIST "$EXTERNALS_TASK 4h\n";
    print TASKLIST "$BUILD_TASK 4h\n";
    print TASKLIST "$BUILD_TESTS_TASK 4h\n";
    print TASKLIST "$UNSTRIPPED_TASK 4h\n";
    print TASKLIST "$CHECK_UNSTRIPPED_TASK 4h\n";
    if ($ENV{NMI_PLATFORM} =~ /(Debian8|Ubuntu14)/) {
        print TASKLIST "$NATIVE_DEBUG_TASK 4h\n";
    }
    print TASKLIST "$NATIVE_TASK 4h\n";
    print TASKLIST "$CHECK_NATIVE_TASK 4h\n";
    print TASKLIST "$TAR_TASK 4h\n";
    print TASKLIST "$TAR_TESTS_TASK 4h\n";
    print TASKLIST "$CHECK_TAR_TASK 4h\n";
    print TASKLIST "$RUN_UNIT_TESTS 4h\n";
}

if (defined($opt_coverity_analysis)) {
    print TASKLIST "$COVERITY_ANALYSIS 4h\n";
}

close (TASKLIST);

######################################################################
# Print content of tasklist
######################################################################
print "Generated tasklist:\n";
open(TASKFILE, '<', $tasklist_file) or die "Can't open $tasklist_file for reading: $!\n";
while(<TASKFILE>) {
    print "\t$_";
}
close (TASKFILE);

print "Initial directory content:\n";
open(FH, '>', 'probe.cmd') or print "Cant open probe.cmd for writing: $!\n";
if ($ENV{NMI_PLATFORM} =~ /_win/i) {
	print FH 'dir' . "\n";
	print FH 'dir ..' . "\n";
	print FH 'dir ..\sources' . "\n";
	close(FH);
} else {
	print FH '#!/bin/sh' . "\n";
	print FH 'pwd && ls -l' . "\n";
	print FH 'ls -l ..' . "\n";
	if ($boos) {
		print FH 'ls -l ../sources' . "\n";
	}
	close(FH);
	chmod(0777,'probe.cmd');
}
system("probe.cmd");
exit 0;
