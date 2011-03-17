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

use strict;
use warnings;
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

my $release_tarball;
my $version;
if( $ENV{NMI_PLATFORM} =~ /winnt/) {
    # on Windows, condor is in a zip file, not a tarball
    print "Finding release zip file\n";
    my ($release_zipfile) = glob("condor-*.zip");
    
    print "Release zip file is $release_zipfile\n";
    
    if( ! $release_zipfile ) {
        die "Could not find a condor release zip file!\n";
    }
    
    if ( ! mkdir("condor") ) {
        die "Could not make the condor folder\n";
    }
    
    print "Unzipping $release_zipfile ...\n";
    system("unzip $release_zipfile -d condor") && die "Can't unzip $release_zipfile !\n";
    print "Unzipped $release_zipfile ...\n";
    
    print "fixing execute bits ...\n";
    system("chmod a+x condor/bin/*");
    
    #debug code...
    #system("ls");
    #system("ls -l condor");
    
    $version = substr($release_zipfile, 0, -4);
    print "VERSION string is $version from $release_zipfile\n";
}
else {
    print "Finding release tarball\n";
    open(TARBALL_FILE, '<', $tarball_file ) || die "Can't open $tarball_file: $!\n";
    while(<TARBALL_FILE>) {
        chomp;
        $release_tarball = $_;
    }
    
    print "Release tarball is '$release_tarball'\n";
    
    if( ! $release_tarball ) {
        die "$tarball_file does not contain a filename!\n";
    }
    if( ! -f $release_tarball ) {
        die "$release_tarball (from $tarball_file) does not exist!\n";
    }
    
    print "Release tarball file exists:\n";
    system("ls -l $release_tarball");

    print "Untarring $release_tarball ...\n";
    system("tar -xzvf $release_tarball" ) && die "Can't untar $release_tarball: $!\n";
    print "Untarred $release_tarball ...\n";
    
    ($version) = $release_tarball =~ /^(.*)\.[^.]+\.[^.]+$/;
    print "VERSION string is $version from $release_tarball\n";
}

######################################################################
# setup the personal condor
######################################################################

print "Condor version: $version\n";

print "SETTING UP PERSONAL CONDOR\n";

# I'm not 100% certain wtf this actually does. 
if($ENV{NMI_PLATFORM} !~ /winnt/) {
    mkdir( "$BaseDir/local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
    system("mv $BaseDir/$version $BaseDir/condor" );
    
    # Remove leftovers from extracting built binaries.
    print "Removing $version tar file and extraction\n";
    system("rm -rf $version*");
    
    # Add condor to the path and set a condor_config variable
    my $OldPath = $ENV{PATH} || die "PATH not in environment!\n";
    my $NewPath = "$BaseDir/condor/sbin:" . "$BaseDir/condor/bin:" . $OldPath;
    $ENV{PATH} = $NewPath;
    $ENV{CONDOR_CONFIG} = "$BaseDir/condor/condor_config";
}
else {
    # windows personal condor setup
    mkdir( "local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
    mkdir( "local/spool", 0777 ) || die "Can't mkdir $BaseDir/local/spool: $!\n";
    mkdir( "local/execute", 0777 ) || die "Can't mkdir $BaseDir/local/execute: $!\n";
    mkdir( "local/log", 0777 ) || die "Can't mkdir $BaseDir/local/log: $!\n";
    
    # Remove leftovers from extracting built binaries.
    print "Removing $version.zip file\n";
    system("rm -rf $version.zip");

    my $Win32BaseDir = $ENV{WIN32_BASE_DIR} || die "WIN32_BASE_DIR not in environment!\n";
    
    #print "current dir\n";
    #system("ls -l");
    #print "$BaseDir/condor_tests\n";
    #system("ls -l $BaseDir/condor_tests");
    #print "$BaseDir/condor/bin\n";
    #system("ls -l $BaseDir/condor/bin");
    
    # Add condor to the path
    my $OldPath = $ENV{PATH} || die "PATH not in environment!\n";
    print "PATH=$OldPath\n";
    system ("which condor_master.exe");
    print "adding condor to the path\n";
    my $NewPath = "$BaseDir/condor/bin:" . $OldPath;
    $ENV{PATH} = $NewPath;
    print "PATH=$ENV{PATH}\n";
}


# -p means  just set up the personal condor for the test run
# move into the condor_tests directory first

chdir( "$BaseDir/condor_tests" ) ||
    die "Can't chdir($BaseDir/condor_tests for personal condor setup): $!\n";

print "About to run batch_test.pl --debug -p\n";
#system("env");

system("perl $BaseDir/condor_tests/batch_test.pl --debug -p");
my $batchteststatus = $?;

# figure out here if the setup passed or failed.
if( $batchteststatus != 0 ) {
    exit 2;
}
