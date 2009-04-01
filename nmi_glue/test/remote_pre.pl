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

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $SrcDir = $ENV{SRC_DIR} || die "SRC_DIR not in environment!\n";

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
if( ! $release_tarball ) {
    die "$tarball_file does not contain a filename!\n";
}
if( ! -f $release_tarball ) {
    die "$release_tarball (from $tarball_file) does not exist!\n";
}
my $configure;
my $reltar;
open( UNTAR, "tar -zxvf $release_tarball |" ) ||
     die "Can't untar $release_tarball: $!\n";
print "Untarring $release_tarball ...\n";
while( <UNTAR> ) {
  if( /.*\/condor_configure/ ) {
    $configure = "$BaseDir/$_";
     chomp( $configure );
  }
  if( /.*\/release.tar/ ) {
    $reltar = "$BaseDir/$_";
    chomp( $reltar );
  }
  print;
}
close UNTAR;


######################################################################
# move prebuilt test programs into the source tree to their normal
# location. "src/testbin_dir"
######################################################################

$testdir = "$BaseDir/testbin/testbin_dir";
if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	system("mv $testdir $SrcDir");
	if( $? ) {
    	die "Problem moving pre-built test programs: $?\n";
	}
}

######################################################################
# setup the personal condor
######################################################################

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	# New improved way to find the version for unix releases
	$release_tarball =~ /condor-(\d+)\.(\d+)\.(\d+)-.*/; 
	$version = "condor-$1.$2.$3";
	print "VERSION string is $version\n";
} else {
	my $vers_file = "CONDOR-VERSION";

	print "Finding version of Condor\n";
	open( VERS, "$vers_file" ) || die "Can't open $vers_file: $!\n";
	while( <VERS> ) {
		chomp;
		$version = $_;
	}
	$version = "condor-" . $version;
	close( VERS );
	if( ! $version ) {
		die "Can't find Condor version in $vers_file!\n";
	}
}

print "Condor version: $version\n";

print "SETTING UP PERSONAL CONDOR\n";

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	# we use condor configure under unix
	my $configure = "$BaseDir/$version/condor_configure";

	mkdir( "$BaseDir/local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
	mkdir( "$BaseDir/condor", 0777 ) || die "Can't mkdir $BaseDir/condor: $!\n";

	my $release = "$BaseDir/$version";

	print "RUNNING condor_configure\n";

	$configure_cmd="$configure --make-personal-condor --local-dir=$BaseDir/local --install=$release --install-dir=$BaseDir/condor --verbose";
	open( CONF, "$configure_cmd|" ) || 
    	die "Can't open $configure as a pipe: $!\n";
	while( <CONF> ) {
    	print;
	}
	close( CONF );
	if( $? ) {
    	die "Problem installing Personal Condor: condor_configured returned $?\n";
	}
	print "condor_configure completed successfully\n";
	
} else {
	# windows personal condor setup

	mkdir( "local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
	mkdir( "local/spool", 0777 ) || die "Can't mkdir $BaseDir/local/spool: $!\n";
	mkdir( "local/execute", 0777 ) || die "Can't mkdir $BaseDir/local/execute: $!\n";
	mkdir( "local/log", 0777 ) || die "Can't mkdir $BaseDir/local/log: $!\n";

	# public contains bin, lib, etc... ;) 
	system("mv public/release condor");

	$Win32BaseDir = $ENV{WIN32_BASE_DIR} || die "WIN32_BASE_DIR not in environment!\n";

}

######################################################################
# Remove leftovers from extracting built binaries.
######################################################################

print "Removing $version tar file and extraction\n";
system("rm -rf $version*");

######################################################################
# setup the src tree so the test suite finds condor_compile and its
# other dependencies.  we do this by setting up a "release_dir" that
# just contains symlinks to everything in the pre-built release
# directory (what we unpacked into "$BaseDir/condor").
######################################################################

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	# save the configure.cf command for platform refernces to defines(bt)
	$configuresaveloc = "$SrcDir/condor_tests/configure_out";
	mkdir(  $configuresaveloc, 0777 ) || die "Can't mkdir($configuresaveloc): $!\n";
	
	safe_copy("config/configure.cf","$configuresaveloc");
	safe_copy("config/externals.cf","$configuresaveloc");
	safe_copy("config/config.sh","$configuresaveloc");
	safe_copy("$SrcDir/config.h","$configuresaveloc");
	safe_copy("$SrcDir/configure.ac","$configuresaveloc");
	print "************************* configure.cf says.. ***************************\n";
	system("cat config/configure.cf");
	print "************************* configure.cf DONE.. ***************************\n";
	print "************************* config.h says.. ***************************\n";
	system("cat $SrcDir/config.h");
	print "************************* config.h DONE.. ***************************\n";

	chdir( "$SrcDir" ) || die "Can't chdir($SrcDir): $!\n";
	mkdir( "$SrcDir/release_dir", 0777 );  # don't die, it might already exist...
	-d "$SrcDir/release_dir" || die "$SrcDir/release_dir does not exist!\n";
	chdir( "$SrcDir/release_dir" ) || die "Can't chdir($SrcDir/release_dir): $!\n";
	opendir( DIR, "$BaseDir/condor" ) ||
	die "can't opendir $BaseDir/condor for release_dir symlinks: $!\n";
	@files = readdir(DIR);
	closedir DIR;
	foreach $file ( @files ) {
		if( $file =~ /\.(\.)?/ ) {
			next;
		}
		symlink( "$BaseDir/condor/$file", "$file" ) ||
			die "Can't symlink($BaseDir/condor/$file): $!\n";
	}
}

# this is a little weird, but so long as we've still got the OWO test
# suites to support with our build system, Imake is going to have the
# path to condor_compile hard-coded to find it in condor_scripts.  so,
# we'll leave this cruft here to manually remove the old copy and add
# a symlink to the one in our pre-built tarball.  whenever we remove
# the OWO, condor_test_suite_C.V5 and friends, we can rip this out,
# and change the definition of the "CONDOR_COMPILE" macro in
# Imake.rules to point somewhere else (if we want).

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	chdir( "$SrcDir/condor_scripts" ) || 
	die "Can't chdir($SrcDir/condor_scripts): $!\n";
	unlink( "condor_compile" ) || die "Can't unlink(condor_compile): $!\n";
	symlink( "$BaseDir/condor/bin/condor_compile", "condor_compile" ) ||
	die "Can't symlink($BaseDir/condor/lib/condor_compile): $!\n";
}


my $OldPath = $ENV{PATH} || die "PATH not in environment!\n";
my $NewPath = "$BaseDir/condor/sbin:" . "$BaseDir/condor/bin:" . $OldPath;
$ENV{PATH} = $NewPath;

# -p means  just set up the personal condor for the test run
# move into the condor_tests directory first

chdir( "$SrcDir/condor_tests" ) ||
    die "Can't chdir($SrcDir/condor_tests for personal condor setup): $!\n";

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
    system( "make batch_test.pl" );
    if( $? >> 8 ) {
        c_die("Can't build batch_test.pl\n");
    }
    system( "make CondorTest.pm" );
    if( $? >> 8 ) {
        c_die("Can't build CondorTest.pm\n");
    }
    system( "make Condor.pm" );
    if( $? >> 8 ) {
        c_die("Can't build Condor.pm\n");
    }
    system( "make CondorPersonal.pm" );
    if( $? >> 8 ) {
        c_die("Can't build CondorPersonal.pm\n");
    }
    system( "make CondorPubLogdirs.pm" );
    if( $? >> 8 ) {
        c_die("Can't build CondorPubLogdirs.pm\n");
    }
	print "About to run batch_test.pl -p\n";

	system("perl $SrcDir/condor_scripts/batch_test.pl -p");
	$batchteststatus = $?;

	# figure out here if the setup passed or failed.
	if( $batchteststatus != 0 ) {
    	exit 2;
	}
} else {
	# do not do a pre-setup yet in remote_pre till fixed
    #my $scriptdir = $SrcDir . "/condor_scripts";
    #copy_file("$scriptdir/batch_test.pl", "batch_test.pl");
    #copy_file("$scriptdir/Condor.pm", "Condor.pm");
    #copy_file("$scriptdir/CondorTest.pm", "CondorTest.pm");
    #copy_file("$scriptdir/CondorPersonal.pm", "CondorPersonal.pm");
}

sub copy_file {
    my( $src, $dest ) = @_;
    copy($src, $dest);
    if( $? >> 8 ) {
        print "Can't copy $src to $dest: $!\n";
    } else {
        print "Copied $src to $dest\n";
    }
}

sub safe_copy {
    my( $src, $dest ) = @_;
	copy($src, $dest);
	if( $? >> 8 ) {
		print "Can't copy $src to $dest: $!\n";
		return 0;
    } else {
		print "Copied $src to $dest\n";
		return 1;
    }
}

