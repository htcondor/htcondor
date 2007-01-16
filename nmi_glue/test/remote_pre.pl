#!/usr/bin/env perl

######################################################################
# $Id: remote_pre.pl,v 1.6 2007-01-16 19:20:09 bt Exp $
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

	if( ! -d "$BaseDir/results" ) {
		# If there's no results, and we can't even make the directory, we
		# might as well die, since there's nothing worth saving...
		mkdir( "$BaseDir/results", 0777 ) || die "Can't mkdir($BaseDir/results): $!\n";
	}

	mkdir( "$BaseDir/local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
	mkdir( "$BaseDir/condor", 0777 ) || die "Can't mkdir $BaseDir/condor: $!\n";

	my $reltar = "$BaseDir/$version/release.tar";

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
	
} else {
	# windows personal condor setup

	mkdir( "local", 0777 ) || die "Can't mkdir $BaseDir/local: $!\n";
	mkdir( "local/spool", 0777 ) || die "Can't mkdir $BaseDir/local/spool: $!\n";
	mkdir( "local/execute", 0777 ) || die "Can't mkdir $BaseDir/local/execute: $!\n";
	mkdir( "local/log", 0777 ) || die "Can't mkdir $BaseDir/local/log: $!\n";

	# public contains bin, lib, etc... ;) 
	system("mv public condor");

	$Win32BaseDir = $ENV{WIN32_BASE_DIR} || die "WIN32_BASE_DIR not in environment!\n";

	# setup the user job wrapper
	safe_copy("$SrcDir/condor_scripts/exe_switch.pl", "$BaseDir/condor/bin/exe_switch.pl") ||
		die "couldn't copy exe_swtich.pl";
	open( WRAPPER, ">$BaseDir/condor/bin/exe_switch.bat" ) || die "Can't open new job wrapper: $!\n";
	print WRAPPER "\@c:\\perl\\bin\\perl.exe $Win32BaseDir/condor/bin/exe_switch.pl %*\n";
	close(WRAPPER);

	# create config file with todd's awk script
	$awkscript = "$BaseDir/src/condor_examples/convert_config_to_win32.awk";
	$genericconfig = "$BaseDir/src/condor_examples/condor_config.generic";
	$configcmd = "awk -f $awkscript $genericconfig > condor/etc/condor_config.orig";
	system("$configcmd");

	# create local config file with todd's awk script
	$genericlocalconfig = "$BaseDir/src/condor_examples/condor_config.local.central.manager";
	$configcmd = "awk -f $awkscript $genericlocalconfig >local/condor_config.local";
	system("$configcmd");

	# change RELEASE_DIR and LOCAL_DIR
	print "Set RELEASE_DIR and LOCAL_DIR\n";
	open( OLDFIG, "<condor/etc/condor_config.orig" ) || die "Can't open base config file: $!\n";
	open( NEWFIG, ">condor/etc/condor_config" ) || die "Can't open new config file: $!\n";
	$line = "";

	while( <OLDFIG> ) {
    	chomp;
		$line = $_;
		if($line =~ /^RELEASE_DIR\s*=.*/) {
			print "Matching <<$line>>\n";
			print NEWFIG "RELEASE_DIR = $Win32BaseDir/condor\n";
		} elsif($line =~ /^LOCAL_DIR\s*=.*/) {
			print "Matching <<$line>>\n";
			print NEWFIG "LOCAL_DIR = $Win32BaseDir/local\n";
		} else {
			print NEWFIG "$line\n";
		}
	}
	close( OLDFIG );
	close( NEWFIG );
}


print "Modifying local config file\n";

rename( "$BaseDir/local/condor_config.local",
	"$BaseDir/local/condor_config.local.orig" )
    	|| die "Can't rename $BaseDir/local/condor_config.local: $!\n";

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
    print FIX "SCHEDD_ADDRESS_FILE = \$(LOG)/.scheduler_address\n";
  } else {
    print FIX;
  }
}

# ADD size for log files and debug level
# default settings are in condor_config, set here to override 
print FIX "ALL_DEBUG               = D_FULLDEBUG\n";

print FIX "MAX_COLLECTOR_LOG       = $logsize\n";
print FIX "COLLECTOR_DEBUG         = \n";

print FIX "MAX_KBDD_LOG            = $logsize\n";
print FIX "KBDD_DEBUG              = \n";

print FIX "MAX_NEGOTIATOR_LOG      = $logsize\n";
print FIX "NEGOTIATOR_DEBUG        = D_MATCH\n";
print FIX "MAX_NEGOTIATOR_MATCH_LOG = $logsize\n";

print FIX "MAX_SCHEDD_LOG          = $logsize\n";
print FIX "SCHEDD_DEBUG            = D_COMMAND\n";

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

# turn on soap for testing
print FIX "ENABLE_SOAP            	= TRUE\n";
print FIX "ALLOW_SOAP            	= *\n";
print FIX "QUEUE_ALL_USERS_TRUSTED 	= TRUE\n";

# condor_config.generic now contains a special value
# for HOSTALLOW_WRITE which causes it to EXCEPT on submit
# till set to some legal value. Old was most insecure..
print FIX "HOSTALLOW_WRITE 			= *\n";
print FIX "NUM_CPUS 			= 1\n";

# Allow a default heap size for java(addresses issues on x86_rhas_3)
# May address some of the other machines with Java turned off also
print FIX "JAVA_MAXHEAP_ARGUMENT = \n";

if( ($ENV{NMI_PLATFORM} =~ /hpux_11/) )
{
    # evil hack b/c our ARCH-detection code is stupid on HPUX, and our
    # HPUX11 build machine in NMI doesn't seem to have the files we're
    # looking for...
    print FIX "ARCH = HPPA2\n";
}

if( ($ENV{NMI_PLATFORM} =~ /ppc64_sles_9/) ) {
	# evil work around for bad JIT compiler
	print FIX "JAVA_EXTRA_ARGUMENTS = -Djava.compiler=NONE\n";
}

# Add a job wrapper for windows.... and a few other things which
# normally are done by condor_configure for a personal condor
if( ($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	my $mypath = $ENV{PATH};
	print FIX "USER_JOB_WRAPPER = $Win32BaseDir/condor/bin/exe_switch.bat\n";
	print FIX "CONDOR_HOST = \$(IP_ADDRESS)\n";
	print FIX "NEGOTIATOR_INTERVAL = 20\n";
	print FIX "START = TRUE\n";
	print FIX "PREEMPT = FALSE\n";
	print FIX "SUSPEND = FALSE\n";
	print FIX "KILL = FALSE\n";
	print FIX "WANT_SUSPEND = FALSE\n";
    print FIX "WANT_VACATE = FALSE\n";
	print FIX "COLLECTOR_NAME = Personal Condor for Tests\n";
	print FIX "ALL_DEBUG = D_FULLDEBUG\n";
	# insure path from framework is injected into the new pool
	print FIX "environment=\"PATH=\'$mypath\'\"\n";
	print FIX "SUBMIT_EXPRS=environment\n";
}

close ORIG;
close FIX; 

print "PERSONAL CONDOR installed!\n";


######################################################################
# setup the src tree so the test suite finds condor_compile and its
# other dependencies.  we do this by setting up a "release_dir" that
# just contains symlinks to everything in the pre-built release
# directory (what we unpacked into "$BaseDir/condor").
######################################################################

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
	# save the configure.cf command for platform refernces to defines(bt)
	safe_copy("config/configure.cf","$BaseDir/results/configure.cf");

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


######################################################################
# spawn the personal condor
######################################################################

chdir($BaseDir);
print "Starting condor_master, about to FORK in $BaseDir\n";

$master_pid = fork();
if( $master_pid == 0) {
	if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
  		exec("$BaseDir/condor/sbin/condor_master -f");
	} else {
		exec("$BaseDir/condor/bin/condor_master -f");
	}
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
$scheddadr = `$BaseDir/condor/bin/condor_config_val SCHEDD_ADDRESS_FILE`;
print "<<SCHEDD_ADDRESS_FILE is $scheddadr\n";
if(!$scheddadr =~/Not defined/) {
	print "We are waiting for the file to exist\n";
	# Where is the schedd address file? wait for it to exist
	$havescheddaddr = "no";
	while($havescheddaddr ne "yes") { 
		print "Looking for $scheddadr\n";
		if( -f $scheddadr ) {
			print "Found it!!!! \n";
			$havescheddaddr = "yes";
		} else {
			sleep 10;
		}
	}
} else {
	print "We are not waiting for the file to exist\n";
	print "SCHEDD_ADDRESS_FILE is not defined\n";
	sleep 45;
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

