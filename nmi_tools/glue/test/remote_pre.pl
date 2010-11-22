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
# script to set up for Condor testsuite run
######################################################################

use Cwd;
use Env; 
use File::Copy;

# Don't buffer output.
$|=1;

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $logsize = "50000000"; # size for logs of personal Condor

# Hard-coded filename, defined in test_platform_pre
my $tarball_file = "CONDOR-TARBALL-NAME";

if( -z "tasklist.nmi" ) {
    # our tasklist is empty, so don't do any real work
    print "No tasks in tasklist.nmi, nothing to do\n";
    exit 0;
}


######################################################################
# untar pre-built tarball
######################################################################

print "Finding release tarball\n";
open( TARBALL_FILE, "$tarball_file" ) || 
    die "Can't open $tarball_file: $!\n";
my $release_tarball;
while( <TARBALL_FILE> ) {
    chomp;
    $release_tarball = $_;
}

print "Release tarball is $release_tarball\n";

if( ! $release_tarball ) {
    die "$tarball_file does not contain a filename!\n";
}
if( ! -f $release_tarball ) {
    die "$release_tarball (from $tarball_file) does not exist!\n";
}

print "Release tarball file exists\n";

print "Untarring $release_tarball ...\n";
system("tar -xzvf $release_tarball" ) && die "Can't untar $release_tarball: $!\n";
print "Untarred $release_tarball ...\n";

######################################################################
# setup the personal condor
######################################################################

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	($basename,$ext_gz) = $release_tarball =~ /^(.*)(\.[^.]*)$/;
	($version,$ext_tar) = $basename =~ /^(.*)(\.[^.]*)$/;
	print "VERSION string is $version\n";
} else {
	die "Your tarball does not match condor-X-Y-Z!\n";
}

print "Condor version: $version\n";

print "SETTING UP PERSONAL CONDOR\n";

# I'm not 100% certain wtf this actually does. 
if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {

	mkdir( "$BaseDir/local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";

} else {
	# windows personal condor setup

	mkdir( "local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
	mkdir( "local/spool", 0777 ) || die "Can't mkdir $BaseDir/local/spool: $!\n";
	mkdir( "local/execute", 0777 ) || die "Can't mkdir $BaseDir/local/execute: $!\n";
	mkdir( "local/log", 0777 ) || die "Can't mkdir $BaseDir/local/log: $!\n";

	# public contains bin, lib, etc... ;)
	# system("mv public condor");

	$Win32BaseDir = $ENV{WIN32_BASE_DIR} || die "WIN32_BASE_DIR not in environment!\n";

}

system("mv $BaseDir/$version $BaseDir/condor" );

######################################################################
# Remove leftovers from extracting built binaries.
######################################################################

print "Removing $version tar file and extraction\n";
system("rm -rf $version*");

my $OldPath = $ENV{PATH} || die "PATH not in environment!\n";
my $NewPath = "$BaseDir/condor/sbin:" . "$BaseDir/condor/bin:" . $OldPath;
$ENV{PATH} = $NewPath;

# -p means  just set up the personal condor for the test run
# move into the condor_tests directory first

chdir( "$BaseDir/condor_tests" ) ||
    die "Can't chdir($BaseDir/condor_tests for personal condor setup): $!\n";

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {

	print "About to run batch_test.pl --debug -p\n";
	#system("env");

	system("perl $BaseDir/condor_tests/batch_test.pl --debug -p");
	$batchteststatus = $?;

	# figure out here if the setup passed or failed.
	if( $batchteststatus != 0 ) {
    	exit 2;
	}
} else {
	system("set");
	# do not do a pre-setup yet in remote_pre till fixed
    #my $scriptdir = $SrcDir . "/condor_scripts";
    #copy_file("$scriptdir/batch_test.pl", "batch_test.pl");
    #copy_file("$scriptdir/Condor.pm", "Condor.pm");
    #copy_file("$scriptdir/CondorTest.pm", "CondorTest.pm");
    #copy_file("$scriptdir/CondorPersonal.pm", "CondorPersonal.pm");
}

# sub copy_file {
#     my( $src, $dest ) = @_;
#     copy($src, $dest);
#     if( $? >> 8 ) {
#         print "Can't copy $src to $dest: $!\n";
#     } else {
#         print "Copied $src to $dest\n";
#     }
# }
# 
# sub safe_copy {
#     my( $src, $dest ) = @_;
# 	copy($src, $dest);
# 	if( $? >> 8 ) {
# 		print "Can't copy $src to $dest: $!\n";
# 		return 0;
#     } else {
# 		print "Copied $src to $dest\n";
# 		return 1;
#     }
# }
# 
