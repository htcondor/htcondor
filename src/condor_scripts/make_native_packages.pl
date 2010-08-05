#!/usr/bin/env perl

##**************************************************************
##
## Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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

######################################################################
# This script finds the binary tarballs packaged up by the nightly
# build and makes native packages (RPM or Debian). It assumes that 
# "make public" has already finished successfully.  
# 
# On Redhat-compatible distribution "make_native_rpm.sh" will be invoked.
# On Debian-compatible distribution "make_native_deb.sh" will be invoked.
#
######################################################################

#Main package revision number
my $revision = 1;

my $make_rpm = "make_native_rpm.sh";
my $make_deb = "make_native_deb.sh";

# Deb | RPM detect via os name
my $type;

#Store OS name - used for naming RPM file
my $os="rhel";

#Store RPM build command (For RPM build only)
my $rpm_cmd;


#Detecting OS distribution 
if( -f "/etc/redhat-release")  {
	$type="RPM";

	#Guess that the fist number in this file is major version number
    my $version = `cat /etc/redhat-release`;

	#to support yellow dog linux
	if ( $version=~/Yellow/)  {
		$os="yd"; 
	}

	$version=~/.*?(\d).*/;
	$version=$1;
    $os=$os . $version;
	
	print "Building for Redhat $version\n"; 

	# The first argument must be the full path to the rpm binary we will be using.
	if ( !defined($ARGV[0]) || ! -e $ARGV[0] ) {
		die "No full path to rpm command specified!";
	}
	if (! -x $ARGV[0]) {
		die "rpm command is not executable!";
	}

	$rpm_cmd = $ARGV[0];
	print "rpm build commmand: $rpm_cmd\n";
	
} elsif ( -f "/etc/debian_version") {

	$type="DEB";

}else {
	print "************************************************************\n";
	print "Native packaging is not supported on this platform\n";
	print "************************************************************\n";
	exit 0;
}

# First, set up a few variables and make sure we can find the script
# we'll eventually need to call to do the actualy RPM creation.
chdir( ".." ) || die "Can't chdir(..): $!\n";
chomp(my $root = `pwd`);
print "root: $root\n";


# Make a temporary build directory in ../public for RPM generation.
chdir "public" || die "Can't chdir(public): $!\n";


# Now, figure out what tarballs we're going to be working on.
opendir PUBLIC, "." || die "Can't opendir(.): $!\n";
my $vers_dir = "";
foreach my $file ( readdir(PUBLIC) ) {
    if( $file =~ /v\d*\.\d*/ ) {
	$vers_dir = $file;
	last;
    }
}
closedir PUBLIC;
if( ! $vers_dir ) {
    die "Can't find directory of public tarballs to make native packages";
}
chdir $vers_dir;

my $binaries_dir="$root/public/$vers_dir";

#Create absolute path to script
$make_rpm = "$root/src/condor_scripts/" . $make_rpm;
$make_deb = "$root/src/condor_scripts/" . $make_deb;

if( ! -f $make_rpm ) {
    die "Can't find make_native_rpm.sh script!";
}
if( ! -f $make_deb ) {
    die "Can't find make_native_deb.sh script!";
}

#There should be only one tarfile that we need to make native package from.
my ($tarfile) = glob "condor-*dynamic.tar.gz";

if( ! -f $tarfile ) {
    die "Can't find tarfile to build from $vers_dir!";
}

#Create temporary area for building package
my $build_area_path = "$binaries_dir/native/package-root";
`rm -rf $binaries_dir/native`;
mkdir "$binaries_dir/native", 0755 or die "Cannnot create folder for storing packages";
mkdir $build_area_path, 0755 or die "Cannnot create folder for building packages";
chdir $build_area_path or die "Cannnot go into build area";

#Do work for each packaging type
if ($type eq "RPM") {

	print "************************************************************\n";
	print "Making RPM package from $tarfile \n";
	print "************************************************************\n";

	#Populate build area
	!system ("cp", "-r", "$root/src/condor_scripts/native/SPECS", ".") or die "Cannot copy SPEC file";
	mkdir "BUILD", 0755;
	mkdir "RPMS", 0755;
	mkdir "SOURCES", 0755;
	mkdir "SRPMS", 0755;

	#Copy tarfile into build area
	!system ("cp", "$binaries_dir/$tarfile", "$build_area_path/$tarfile") or die "Cannot copy tarfile";
	
	system ($make_rpm, $tarfile, $os, $rpm_cmd, $revision );

    #RPM file should be in native folder by now.
	chdir "$binaries_dir/native";
	my $rpm_file = glob "condor*.rpm";
	
	if ( !defined ($rpm_file)) {
		die "RPM package not found";
    } else {
		!system ("mv", "$rpm_file", "$binaries_dir") or die "Cannot move package to public folder";

		print "************************************************************\n";
		print "All done. Result package: $rpm_file \n";
		print "************************************************************\n";
	}

	############################################################################################

	print "************************************************************\n";
	print "Making drone RPM/SRPM \n";
	print "************************************************************\n";
	
	#Build area is already created by previous section	
	#SPEC will clean the build area by itself 

	chdir $build_area_path;

	#Copy input files
	!system ("cp", "$root/src/condor_examples/condor.boot.rpm", "SOURCES/") or die "Cannot copy condor.boot.rpm";
	!system ("cp", "$root/src/condor_examples/condor.sh", "SOURCES/") or die "Cannot copy condor.sh";
	!system ("cp", "$root/src/condor_examples/condor.csh", "SOURCES/") or die "Cannot copy condor.csh";
	!system ("cp", "$root/src/condor_examples/condor.sysconfig", "SOURCES/") or die "Cannot copy condor.sysconfig";
	!system ("cp", "$root/src/condor_examples/condor.sysconfig.drone.patch", "SOURCES/") or die "Cannot copy condor.sysconfig.drone.patch";
	!system ("cp", "$root/src/condor_examples/master_wrapper", "SOURCES/") or die "Cannot copy master_wrapper";
	# SPEC file is already copied in as part of last RPM build

	print "Updating SPEC file\n";
	chomp(my $rpm_date = `date +"%a %b %d %Y"`);

	#Replace VERSION and SOURCE file in original spec file
	$tarfile =~ /condor-(.*?)-.*/;
	my $condor_ver = $1;

	`perl -p -i -e "s|_VERSION_|$condor_ver|g; \
					s|_REVISION_|$revision|g; \
					s|_DATE_|$rpm_date|g;" $build_area_path/SPECS/condor_drone.spec`;


	print "Building RPM/SRPM\n";
	#-ba will build both source and binary rpm
	`$rpm_cmd --define="_topdir $build_area_path" -ba $build_area_path/SPECS/condor_drone.spec`;

	$rpm_file = glob "$build_area_path/RPMS/noarch/condor*";
	my $srpm_file = glob "$build_area_path/SRPMS/condor*";
	
	if (!defined ($rpm_file)) {
		die "Drone RPM package not found";
	}
	if (!defined ($srpm_file)) {
		die "Drone SRPM package not found";
	}
	
	#Construct os-specific rpm name
	$rpm_file =~ /.*(condor-drone.*?)(\.noarch.*)/;
	my $rpm_name = "$1.$os$2";

	#Construct os-specific rpm name
	$srpm_file =~ /.*(condor-drone.*?)(\.src.*)/;
	my $srpm_name = "$1.$os$2";

	print "Renamed  RPM: $rpm_name\n";
	print "Renamed SRPM: $srpm_name\n";
	!system ("mv", "$rpm_file", "$binaries_dir/$rpm_name") or die "Cannot move package to public folder";
	!system ("mv", "$srpm_file", "$binaries_dir/$srpm_name") or die "Cannot move source package to public folder";
   
	print "************************************************************\n";
	print "All done. RPM package: $rpm_name \n";
	print "         SRPM package: $srpm_name \n";
	print "************************************************************\n";

} else {

	print "************************************************************\n";
	print "Making Debian package from $tarfile \n";
	print "************************************************************\n";

	#Populate build area
	!system ("cp", "-r", "$root/src/condor_scripts/native/DEBIAN", ".") or die "Cannot copy Debian metadata files";

	#Copy tarfile into build area
	!system ("cp", "$binaries_dir/$tarfile", "$build_area_path/$tarfile") or die "Cannot copy tarfile";
	
	system ($make_deb, $tarfile , $revision);
	
	#Debian file should be in native folder by now.
	chdir "$binaries_dir/native";
	my $deb_file = glob "condor*.deb";	

	if ( !defined ($deb_file)) {
		die "Debian package not found";
    } else {
		!system ("mv", "$deb_file", "$binaries_dir") or die "Cannot move package to public folder";

		print "************************************************************\n";
		print "All done. Result package: $deb_file \n";
		print "************************************************************\n";

	}
	
}

#Remove build area
`rm -rf $binaries_dir/native`;

exit 0;

