#!/usr/bin/env perl

######################################################################
# $Id: remote_pre.pl,v 1.1.4.6 2005-04-01 01:22:18 wright Exp $
# script to set up for Condor testsuite run
######################################################################

use Cwd;
use Env; 

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
# setup the personal condor
######################################################################

# New improved way to find the version
$release_tarball =~ /condor-(\d+)\.(\d+)\.(\d+)-.*/; 
$version = "condor-$1.$2.$3";
print "VERSION string is $version\n";

my $configure = "$BaseDir/$version/condor_configure";
my $reltar = "$BaseDir/$version/release.tar";

print "SETTING UP PERSONAL CONDOR\n";
mkdir( "$BaseDir/local" ) || die "Can't mkdir $BaseDir/local: $!\n";
mkdir( "$BaseDir/condor" ) || die "Can't mkdir $BaseDir/condor: $!\n";

print "RUNNING condor_configure\n";

$configure_cmd="$configure --make-personal-condor --local-dir=$BaseDir/local --install=$reltar --install-dir=$BaseDir/condor --verbose";
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


print "Modifying local config file\n";

rename( "$BaseDir/local/condor_config.local",
	"$BaseDir/local/condor_config.local.orig" )
    || die "Can't rename condor_config.local: $!\n";

# make sure ports for Personal Condor are valid, we'll use address
# files and port = 0 for dynamic ports...
open( ORIG, "<$BaseDir/local/condor_config.local.orig" ) ||
    die "Can't open $BaseDir/local/condor_config.local.orig: $!\n";
open( FIX, ">$BaseDir/local/condor_config.local" ) ||
    die "Can't open $BaseDir/local/condor_config.local: $!\n";
while( <ORIG> ) {
  if( /CONDOR_HOST.*/ ) {
    print FIX;
    print FIX "COLLECTOR_HOST = \$(CONDOR_HOST):0\n";
    print FIX "NEGOTIATOR_HOST = \n";
    print FIX "COLLECTOR_ADDRESS_FILE = \$(LOG)/.collector_address\n";
    print FIX "NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address\n";
  } else {
    print FIX;
  }
}

# ADD size for log files and debug level
# default settings are in condor_config, set here to override 
print FIX "ALL_DEBUG               = ";

print FIX "MAX_COLLECTOR_LOG       = $logsize\n";
print FIX "COLLECTOR_DEBUG         = ";

print FIX "MAX_KBDD_LOG            = $logsize\n";
print FIX "KBDD_DEBUG              = ";

print FIX "MAX_NEGOTIATOR_LOG      = $logsize\n";
print FIX "NEGOTIATOR_DEBUG        = D_MATCH\n";
print FIX "MAX_NEGOTIATOR_MATCH_LOG = $logsize\n";

print FIX "MAX_SCHEDD_LOG          = $logsize\n";
print FIX "SCHEDD_DEBUG            = D_COMMAND";

print FIX "MAX_SHADOW_LOG          = $logsize\n";
print FIX "SHADOW_DEBUG            = D_FULLDEBUG\n";

print FIX "MAX_STARTD_LOG          = $logsize\n";
print FIX "STARTD_DEBUG            = D_COMMAND\n";

print FIX "MAX_STARTER_LOG         = $logsize\n";
print FIX "STARTER_DEBUG           = D_NODATE\n";

print FIX "MAX_MASTER_LOG          = $logsize\n";
print FIX "MASTER_DEBUG            = D_COMMAND\n";

# Add a shorter check time for periodic policy issues
print FIX "PERIODIC_EXPR_INTERVAL = 15\n";

close ORIG;
close FIX; 

print "PERSONAL CONDOR installed!\n";


######################################################################
# setup the src tree so the test suite finds condor_compile and its
# other dependencies.  we do this by setting up a "release_dir" that
# just contains symlinks to everything in the pre-built release
# directory (what we unpacked into "$BaseDir/condor").
######################################################################

chdir( "$SrcDir" ) || die "Can't chdir($SrcDir): $!\n";
mkdir( "$SrcDir/release_dir" );  # don't die, it might already exist...
-d "$SrcDir/release_dir" || die "$SrcDir/release_dir does not exist!\n";
chdir( "$SrcDir/release_dir" )
    || die "Can't chdir($SrcDir/release_dir): $!\n";
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

# this is a little weird, but so long as we've still got the OWO test
# suites to support with our build system, Imake is going to have the
# path to condor_compile hard-coded to find it in condor_scripts.  so,
# we'll leave this cruft here to manually remove the old copy and add
# a symlink to the one in our pre-built tarball.  whenever we remove
# the OWO, condor_test_suite_C.V5 and friends, we can rip this out,
# and change the definition of the "CONDOR_COMPILE" macro in
# Imake.rules to point somewhere else (if we want).
chdir( "$SrcDir/condor_scripts" ) || 
    die "Can't chdir($SrcDir/condor_scripts): $!\n";
unlink( "condor_compile" ) || die "Can't unlink(condor_compile): $!\n";
symlink( "$BaseDir/condor/bin/condor_compile", "condor_compile" ) ||
    die "Can't symlink($BaseDir/condor/lib/condor_compile): $!\n";


######################################################################
# spawn the personal condor
######################################################################

chdir($BaseDir);
print "Starting condor_master, about to FORK in $BaseDir\n";

$master_pid = fork();
if( $master_pid == 0) {
  exec("$BaseDir/condor/sbin/condor_master -f");
  print "MASTER EXEC FAILED!!!!!\n";
  exit 1;
}

# save the master pid for later use
print "Master PID is $master_pid\n";
open( PIDFILE, ">$BaseDir/condor_master_pid") || 
    die "cannot open $BaseDir/condor_master_pid: $!\n";
print PIDFILE "$master_pid";
close PIDFILE;   

# Give the master time to start before jobs are submitted.
print "Sleeping for 30 seconds to give the master time to start.\n";
sleep 30;
