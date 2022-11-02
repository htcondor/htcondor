#!/usr/bin/env perl

use strict;
use warnings;

use Cwd;
use File::Temp;
use File::Basename;

# This script require a CMakeCache.txt file in its IWD; it's intended
# to be run from your out-of-source build directory.  If you'd like, you
# check for the existence of that file before creating the destination
# directory.

my $originalDirectory = cwd();
my $destinationDirectory = "${originalDirectory}/rpms";
if( -e $destinationDirectory ) {
	if( ! -d $destinationDirectory ) {
		print( "Destination directory ${destinationDirectory} is not a directory.\n" );
		exit( 1 );
	}
} else {
	system( "mkdir ${destinationDirectory}" );
}

my $rawSD = `grep CONDOR_SOURCE_DIR CMakeCache.txt`;
chomp( $rawSD );
my( $dummy, $sourceDirectory ) = split( '=', $rawSD );
print( "Using source directory '$sourceDirectory'.\n" );

# Prepare a temporary directory for RPM construction.
my $workingDirectory = File::Temp::tempdir( CLEANUP => 1 );
# Don't delete the temporary directory.  Only for debugging.
# my $workingDirectory = File::Temp::tempdir();
print( "Working in '${workingDirectory}'...\n" );

chdir( $workingDirectory );
foreach my $dir qw( SOURCES BUILD BUILDROOT RPMS SPECS SRPMS ) {
	mkdir( $dir );
}
system( "cp -p ${sourceDirectory}/build/packaging/rpm/* SOURCES" );

my $condorVersion = qx#awk -F\\" '/^set\\\(VERSION /{print \$2}' ${sourceDirectory}/CMakeLists.txt#;
chomp( $condorVersion );
print( "Found condor version '${condorVersion}'.\n" );

my $condorBuildID = `date +%Y%m%d%H%M%S`;
chomp( $condorBuildID );
print( "Found condor build ID '${condorBuildID}'.\n" );

updateSpecDefine( "git_build", "0" );
updateSpecDefine( "tarball_version", $condorVersion );
updateSpecDefine( "condor_build_id", $condorBuildID );
updateSpecDefine( "condor_base_release", "0.${condorBuildID}" );


chdir( ${sourceDirectory} ); chdir( ".." );
print( "Tarring up sources...\n" );
my $baseName = basename( $sourceDirectory );
system( "tar --exclude .git --transform 's/^${baseName}/condor-${condorVersion}/' -z -c -f ${workingDirectory}/SOURCES/condor-${condorVersion}.tar.gz ${baseName}" );
chdir( $workingDirectory );


print( "Starting RPM build...\n" );
$ENV{ VERBOSE } = 1;
system( qq#rpmbuild -v -ba --define="_topdir ${workingDirectory}" SOURCES/condor.spec# );

print( "Moving results to destination...\n" );
system( "mv \$(find *RPMS -name \*.rpm) ${destinationDirectory}" );

END {
	chdir( $originalDirectory );
}

exit( 0 );

sub updateSpecDefine {
	my( $attribute, $value ) = @_;
	print( "Redefining '${attribute}' to '${value}'...\n" );

	system( qq!perl -p -i -e's#^%define ${attribute} .*#%define ${attribute} ${value}#' ${workingDirectory}/SOURCES/condor.spec! );
}
