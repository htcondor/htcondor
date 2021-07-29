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


######################################################################
# script to set up for Condor testsuite run
######################################################################

use strict;
use warnings;
use Cwd;
use Env; 
use File::Copy;
use File::Basename;

BEGIN {
	my $dir = dirname($0);
	unshift @INC, $dir;
}

use TestGlue;
TestGlue::setup_test_environment();

# Don't buffer output.
$|=1;

my $iswindows = TestGlue::is_windows();
my $iscygwin  = TestGlue::is_cygwin_perl();

#if($iswindows == 1) {
#	if($iscygwin == 1) {
#		TestGlue::out("Entering remote_pre using cygwin perl");
#	} else {
#		TestGlue::out("Entering remote_pre using active state perl");
#	}
#}

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";

print "My BASE_DIR=$BaseDir and I can see:\n";

if($iswindows == 1) {
	system("dir");
} else {
	system("ls");
}

my $logsize = "50000000"; # size for logs of personal Condor
my $force_cygwin = 0; # force a switch to cygwin perl before running batch test.

# Hard-coded filename, defined in test_platform_pre
my $tarball_file = "CONDOR-TARBALL-NAME";

if( -z "tasklist.nmi" ) {
    # our tasklist is empty, so don't do any real work
    out("No tasks in tasklist.nmi, nothing to do");
    exit 0;
}


######################################################################
# untar pre-built tarball
######################################################################

my $release_tarball;
my $version;
if( TestGlue::is_windows() ) {
	my $targetdir = "$BaseDir" . "\\condor";
	my $msiname = "";
	my $zipname = "";
	# get as close to an msi install as possible.
	# msi name is what?
	my $foundmsi = 0;
	my $foundzip = 0;
	print "condor to be stashed here: $targetdir\n";
	print "Looking for MSI, ZIP or release_dir\n";

	opendir DS, "." or die "Can not open directory: $1\n";
    foreach my $subfile (readdir DS) {
		next if $subfile =~ /^\.\.?$/;
		if($subfile =~ /^(.*?\.msi)$/) {
			print "MSI $subfile\n";
			$msiname = $subfile;
			$foundmsi = 1;
		}elsif($subfile =~ /^(.*?\.zip)$/) {
			print "ZIP $subfile\n";
			if ($subfile =~ /condor/i) { # must actually be the condor zip file.
				$zipname = $subfile;
				$foundzip = 1;
			}
		} else {
			print "$subfile: not msi or zip\n";
		}
		if(($foundmsi == 1) && ($foundzip == 1)) {
			last
		}
	}
	close(DS);

	# first find name of the msi
	# now extract msi into ./condor

	if ($foundmsi) {
		print "About to extract: $msiname\n";
		my $command = "msiexec /qn /a $msiname TARGETDIR=$targetdir";
		print "about to execute $command\n";
		system($command);
		if ($?) {
			print "\textract of $msiname failed with error $? will try the zip file instead.\n";
			print STDERR "Extract of MSI $msiname failed with error $? : $!\n";
			$foundmsi = 0; # try using the zip file
		} else {
			print "done extractiing $msiname\n";
			($version) = $msiname =~ /^(.*)\.[^.]+$/;
		}
	}
	unlink($msiname) or print "could not remove $msiname :$!\n";

	if ($foundzip && ! $foundmsi) {
		print "About to extract $zipname\n";
		my $command = "unzip $zipname -d $targetdir";
		print "about to execute $command\n";
		system($command) or print "unzip reported error $?: $!\n";
		if ($?) {
			print "\tunzip of $zipname failed with error $? will try the release_dir instead.\n";
			print STDERR "Extract of ZIP $zipname failed with error $? : $!\n";
			$foundzip = 0; # try using release_dir
		} else {
			print "done extractiing $zipname\n";
			($version) = $zipname =~ /^(.*)\.[^.]+$/;
			print "directory listing of $targetdir:\n";
			print `dir $targetdir`;
		}
	}
	unlink($zipname) or print "could not remove $zipname :$!\n";

	if ( ! $foundmsi && ! $foundzip) {
		print "Could not unpack either MSI or ZIP, attempting to rename release_dir to condor\n";
		rename("release_dir", "condor") or print "rename failed. things will probably fail from here on...\n";
		print "directory listing of $targetdir:\n";
		print `dir $targetdir`;
		$version = "unknown";
	}

	# lets get our base config in place
	my $genericconfig = "$targetdir" . "\\etc\\condor_config.base";
	my $mainconfig = "$targetdir" . "\\condor_config";
	my $localonfig = "$targetdir" . "\\condor_config.local";
	my $localdir = $targetdir;
	my $resleasedir = $targetdir;
	my $localdirset = 0;

	# might as well set this now
	$ENV{condor_config} = $mainconfig;

	# stub local config file
	open(LCF,">$localonfig")  or die "Can not create local config file:$!\n";
	print LCF "# Starting local config file\n";
	close(LCF);

	#craft initial config file
	my $line = "";
	open(ECF,"<$genericconfig") or die "failed to open config template:$!\n";
	open(CF,">$mainconfig") or die "failed to create base config fileL$!\n";
	while(<ECF>) {
		TestGlue::fullchomp($_);
		$line = $_;
		if($line =~  /^\s*RELEASE_DIR.*$/) {
			print CF "RELEASE_DIR = $resleasedir\n";
		} elsif($line =~  /^\s*#LOCAL_DIR = \$\(RELEASE_DIR\).*$/) {
			$localdirset = 1;
			print CF "LOCAL_DIR = $localdir\n";
		} else {
			print CF "$line\n";
		}
	}

	#
	# The changes we make to construct a personal Condor can only be
	# superseded on a test-by-test basis.  We therefore moved some of
	# them here so that the testconfigappend file can supersede them
	# globally.  This is particularly important for changes, like
	# COLLECTOR_HOST (included in 'use role : personal'), which presently
	# depend on which protocol is enabled (if the CONDOR_HOST is an IPv6
	# literal, we can't simply append ":<port-number>" to it).
	#
	print CF "use role : personal\n";
	print CF "use security : host_based\n";
	# Compensate for an HTCondor configuration bug where the above two
	# lines don't actually produce a working personal Condor.
	print CF 'CONDOR_HOST = $(FULL_HOSTNAME)' . "\n";

	if($localdirset == 0) {
		print CF "LOCAL_DIR = $localdir\n";
	}

	close(CF);
	close(ECF);

} else {
    out("Finding release tarball");
    open(TARBALL_FILE, '<', $tarball_file ) || die "Can't open $tarball_file: $!\n";
    while(<TARBALL_FILE>) {
        chomp;
        $release_tarball = $_;
    }
    
    out("Release tarball is '$release_tarball'");
    
    if( ! $release_tarball ) {
        die "$tarball_file does not contain a filename!\n";
    }
    if( ! -f $release_tarball ) {
        die "$release_tarball (from $tarball_file) does not exist!\n";
    }
    
    out("Release tarball file exists:");
    TestGlue::dir_listing($release_tarball);

    out("Untarring $release_tarball ...");
    system("tar -xzvf $release_tarball" ) && die "Can't untar $release_tarball: $!\n";
    
    ($version) = $release_tarball =~ /^(.*)\.[^.]+\.[^.]+$/;
    out("VERSION string is $version from $release_tarball");
}

######################################################################
# setup the personal condor
######################################################################

print "Condor version: $version\n";

# I'm not 100% certain wtf this actually does. 
if( not TestGlue::is_windows() ) {
    mkdir( "$BaseDir/local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
    system("mv $BaseDir/$version $BaseDir/condor" );

	if ( `uname` =~ "Linux" || `uname` =~ "Darwin" ) {
		# On cross-platform testing, the binaries in condor_tests may need
		# to find the extra system libraries we include. Let them find them
		# when looking under $ORIGIN/../lib/condor.
		system( "ln -s condor/lib lib" );
		system( "ln -s condor/lib64 lib64" );
	}
    
	if(-f "condor/condor_install") {
		system("ls condor");
		chdir("condor");
		system("chmod 755 condor_install");
		system("./condor_install -make-personal-condor");
		system("ls");
		chdir("condor");
	}

    # Remove leftovers from extracting built binaries.
    print "Removing $version tar file and extraction\n";
    unlink(<$version*>);

	# NOTE at this point we have condor_install generated
	# configuration file and paths in condor/condor.sh
	# needing to be harvested later so before we start
	# tweeking personal condors we have our condor_paths
	# and CONDOR_CONFIG. 

    #
    # With 'use security : host_based' now remove from the default HTCondor
    # config (HTCONDOR-301), we need to update or replace condor_install
    # (HTCONDOR-52).  Until we do, fix it here by hand.  We'll assume for
    # now that appending it will do.
    #
	my $mainConfigFile = $ENV{ CONDOR_CONFIG };
	open( MAIN, ">> $mainConfigFile" ) or die "Failed to open original config file: $mainConfigFile :$!\n";
	print MAIN "\nuse security : host_based\n";
	close( MAIN );
}

#Look for condor_tests/testconfigappend which we want at the end
#of the main config file
print "CONFIGURING PERSONAL CONDOR\n";
print "TESTS set to: $ENV{TESTS}\n";
print "CONDOR_CONFIG set to: $ENV{CONDOR_CONFIG}\n";

my $testConfigAppendFile = "testconfigappend";
my @dirs = ( $ENV{BASE_DIR}, $ENV{TESTS} );
foreach my $dir (@dirs) {
	my $fullPath = File::Spec->catfile( $dir, $testConfigAppendFile );
	if( -f $fullPath ) {
		$testConfigAppendFile = $fullPath;
		last;
	}
}

if( -f $testConfigAppendFile ) {
	print "Triggered to modify master config with testconfigappend file!\n";

	my $mainConfigFile = $ENV{ CONDOR_CONFIG };
	my $newConfigFile = $mainConfigFile . "new";
	my $oldConfigFile = $mainConfigFile . "old";

	open( MAIN, "< $mainConfigFile" ) or die "Failed to open original config files: $mainConfigFile :$!\n";
	open( NEW,  "> $newConfigFile" ) or die "Failed to open/create new config: $newConfigFile :$!\n";
	open( APP,  "< $testConfigAppendFile" ) or die "failed to open additions: $testConfigAppendFile :$!\n";

	while (<MAIN>) {
		print NEW "$_";
	}

	# force stuff?
	# print NEW "\nALL_DEBUG = D_SUB_SECOND D_CAT\n";

	while (<APP>) {
		print NEW "$_";
	}

	close( MAIN );
	close( NEW );
	close( APP );

	rename( $mainConfigFile, $oldConfigFile );
	rename( $newConfigFile, $mainConfigFile );

	print "Displaying new main config file:\n";
	open (MAIN, "< $mainConfigFile" ) or die "Failed to open original config files: $mainConfigFile :$!\n";
	while( <MAIN> ) {
		print $_;
	}
	close( MAIN );
} else {
	print "No config append requested, forcing ALL_DEBUG = D_SUB_SECOND D_CAT\n";
	my $mainConfigFile = $ENV{ CONDOR_CONFIG };
	open( MAIN,  ">> $mainConfigFile" ) or die "Failed to append to: $mainConfigFile :$!\n";
	print MAIN "\nALL_DEBUG = D_SUB_SECOND D_CAT\n";
	close ( MAIN );
}






