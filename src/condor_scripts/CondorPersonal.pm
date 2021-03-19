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


# CondorPersonal.pm - a Perl API to Condor for Personal Condors
#
# Designed to allow a flexible way to have tests and other jobs
# run in conjunction with other Condor perl modules and control
# the environment in which they run
#
# 1-6-05 Bill Taylor - vastly updated  Bill Taylor 10-2-14
#

#################################################################
#
#	A Personal Condor will be constructed in a subdirectory
#		based on current PID and the version string passed in
#		such that bin, sbin, log, condor_config and the rest
#		live in:
#
#		PID/PIDversion/sbin
#		PID/PIDversion/bin
#		PID/PIDversion/log
#		PID/PIDversion/execute
#		PID/PIDversion/condor_config
#		...



package CondorPersonal;

use strict;
use warnings;
use Carp;
use Cwd;
use POSIX qw/sys_wait_h strftime/;
use Socket;
use Sys::Hostname;

use CondorUtils;
use CondorTest;


my $iswindows = CondorUtils::is_windows();
my $iscygwin = CondorUtils::is_cygwin_perl();
my $iswindowsnativeperl = CondorUtils::is_windows_native_perl();
my $wininstalldir = "";
my $installdir = "";

my $masterconfig = ""; #one built by batch_test and moving to test glue
my $currentlocation;

sub Initialize
{
	# we want to initialize by whatever we have in $ENNV(CONDOR_CONFIG}
	# not something created in the test glue.
	my %control = @_;
	my $intheglue = 0;
	if( exists $control{test_glue}) {
		$intheglue = 1;
	}

	my $newconfig = deriveMasterConfig();

	debug("Effective config now $newconfig\n", 2);

	$currentlocation = getcwd();
	if(CondorUtils::is_windows() == 1) {
		if(is_windows_native_perl()) {
			$currentlocation =~ s/\//\\/g;
			$masterconfig = "$currentlocation" . "\\$newconfig";
		} else {
			# never have cygwin till test runs, so glue would not be set
			#print "RAW curentlocation:-$currentlocation-\n";
			my $tmp = `cygpath -m $currentlocation`;
			fullchomp($tmp);
			#print "tmp after cygpath:$tmp\n";
			$tmp =~ s/\//\\/g;
			$currentlocation = $tmp;
			$masterconfig = "$currentlocation" . "\\$newconfig";
		}
	} else {
		$masterconfig = "$currentlocation" . "/$newconfig";
	}
}

sub deriveMasterConfig {
	# since we still are in the impact of the initial condor
	# get all of the actual settings
	my @outres = ();
	my $derivedconfig = "derived_condor_config";
	if(-f "$derivedconfig") {
		return($derivedconfig);
	} else {
		# we need gererate the effective current config and
		# start from there. There are all the possible config files
		# plus changed vs default values.
		my $res = CondorTest::runCondorTool("condor_config_val -writeconfig:file $derivedconfig",\@outres,2,{emit_output=>0,expect_result=>\&ANY});
		if($res != 1) {
			die "Error while getting the effective current configuration\n";
		}
		open(DR,"<derived_condor_config") or die "Failed to create derived_condor_config: $!\n";
		my $line = "";
		while(<DR>) {
			$line = $_;
			fullchomp($line);
			print "$line\n";
		}
	}
	return($derivedconfig);
}

BEGIN {
}

my %windows_semaphores = ();
my %framework_timers = ();

#################################################################
#
#	Parameters used within parameter config file......
#
#	Parameter		Use								Default						Variable stored in
#	----------------------------------------------------------------------------------------------
#	condortemplate	Core config file					condor_config_template		$personal_template
#	condorlocalsrc	Name for condor local config src 								$personal_local_src
#   daemonwait      Wait for startd/schedd to be seen	true						$personal_startup_wait
#   localpostsrc    New end of local config file                                    $personal_local_post_src
#   append_condor_config    Text to append to end of local config file
#   secprepostsrc	New security settings 											$personal_sec_prepost_src
#	condordaemon	daemon list to start				contents of config template $personal_daemons
#	condorconfig	Name for condor config file			condor_config				$personal_config
#	condordomain	Name for domain						local						$condordomain
#	condorlocal		Name for condor local config 		condor_config.local			$personal_local
#	condor			"install" or path to tarball 		nightlies							$condordistribution
#	collector	 	Used to define COLLECTOR_HOST									$collectorhost
#	nameschedd	 	Used to define SCHEDD_NAME			cat(name and collector)		$scheddname
#	condorhost	 	Used to define CONDOR_HOST										$condorhost
#	ports			Select dynamic or normal ports		dynamic						$portchanges	
#	slots			sets NUM_CPUS NUM_SLOTS			none
#   universe		parallel configuration of schedd	none						$personal_universe
#
#   Notes added 10/3/14 bt
#
#   The use of param files and tests calling other scripts is over. What we have now 
#   are test additional knobs which are added to the end of a very basic local file.
#   All platforms go with the initial CONDOR_CONFIG in the environment which simply
#   put has a valid RELEASE_DIR.
#
#	Notes from January 2014 bill taylor
#	Clumsy and unreliable IsRunningYet causes test to fail and failures to be long in time
#	for a series of tests as platforms etc vary. We are moving away from IsRunningYet
#	address file based to daemon information available with condor_who -daemon -log xxxxxxxx
#
#	So we can collect whodata within our presonal condor instance
#
#	Se we first want a test setable trigger to start using new start up evaluation
#	and get one test working this way.
#
#	The new way will still block. It will have a time tolerance not a try tolerance.
#	There must be functions to determine our current direction based state and a way
#	to call this state evaluation repeatably. 
# 
# 	There is a large comment relative to our data storage class in CondorTest.pm above this
# 	pacakge: package WhoDataInstance
#
#   We actually do not have test framework call backs. But we added a basic state change loop
#   which will check for and call callbacks while we are looping to change state. NewIsRunningYet
#   replaces IsRunningYet and KillDaemons replaces KillDaemonPids and uses NewIsDownYet(new).
#   Both NewIs functions call the same state transition code which collects and examines condor_who
#   data stored in the instance of the personal condor

my %daemon_logs =
(
	"COLLECTOR" => "CollectorLog",
	"NEGOTIATOR" => "NegotiatorLog",
	"MASTER" => "MasterLog",
	"STARTD" => "StartLog",
	"SCHEDD" => "SchedLog",
	"collector" => "CollectorLog",
	"negotiator" => "NegotiatorLog",
	"master" => "MasterLog",
	"startd" => "StartLog",
	"schedd" => "SchedLog",
);

########################################
##
## 7/28/14 bt
##
## Our normal test pass has the LOCKDIR to deep
## to use a folder for shared ports handles
## So the flag below relocates it to /tmp
## Other switch turns on shared port for
## every personal condor spun up
##

my $MOVESOCKETDIR = 0;
my $USESHARERPORT = 0;
##
##
########################################

my $RunningTimeStamp = 0;

my $topleveldir = getcwd();
   $topleveldir =~ s/condor_tests$/test-runs/;
my $home = $topleveldir;
my $localdir;
my $condorlocaldir;
my $pid = $$;
my $version = ""; # remote, middle, ....... for naming schedd "schedd . pid . version"
my $mastername = ""; # master_$verison
my $DEBUGLEVEL = 1; # nothing higher shows up
my $debuglevel = 4; # take all the ones we don't want to see
					# and allowed easy changing and remove hard
					# coded value
my @debugcollection = ();
my $isnightly = IsThisNightly($topleveldir);
my $wrap_test;

#################################################################
#
# Debug messages get time stamped. These will start showing up
# at DEBUGLEVEL = 3 with some rather verbous at 4.
# 
# For a single test which uses this module simply
# CondorPersonal::DebugOn();
# CondorPersonal::DebugLevel(3);
# .... some time later .....
# CondorPersonal::DebugLevel(2);
#
# There is no reason not to have debug always on the the level
# pretty completely controls it. All DebugOff calls
# have been removed.
#
# This is a similar debug setup as the rest of the test
# suite but I did not want to require the other condor
# modules for this one.
#
#################################################################

my %personal_condor_params;
my %personal_config_changes;
my $personal_config = "condor_config";
my $personal_template = "condor_config_template";
my $personal_daemons = "";
my $personal_local = "condor_config.local";
my $personal_local_src = "";
my $personal_local_post_src = "";
my $personal_sec_prepost_src = "";
my $personal_universe = "";
my $personal_startup_wait = "true";
my $personalmaster;
my $portchanges = "dynamic";
my $collector_port = "0";
my $personal_config_file = "";
my $condordomain = "";
my $procdaddress = "";

#################################################################
#
# Main interface StartCondor
#condor
# Calls functions to parse parameters, install binaries, tune the config file
#	and start the personal condor. Passes back config file location and port
#	number<config_file_location:collector_port>.
#

sub StartCondor
{
	my $mpid = "";
	my $arraysz = scalar(@_);
	my $testname = shift || die "Missing test name\n";
	my $paramfile = shift || die "Missing parameter file!\n";
	$version = shift || die "Missing parameter version!\n";
	my $nowait = shift;


	my $config_and_port = "";
	my $winpath = "";

	if(!(-f $paramfile)) {
		die "StartCondor: param file $paramfile does not exist!!\n";
	}

	CondorPersonal::ParsePersonalCondorParams($paramfile);

	if(defined $nowait) {
		#print "StartCondor: no wait option\n";
		$personal_condor_params{"no_wait"} = "TRUE";
	}

	# Insert the positional arguments into the new-style named-argument
	# hash and call the version of this function which handles it.
	$personal_condor_params{"test_name"} = $testname;
	$personal_condor_params{"condor_name"} = $version;
	$personal_condor_params{"fresh_local"} = "TRUE";

	return StartCondorWithParams(%personal_condor_params);
}

############################################
## StartCondorWithParams
##
## Starts up a personal condor that is configured as specified in
## the named arguments to this function.  If you are using the
## CondorTest framework, do not call this function directly.
## Call CondorTest::StartCondorWithParams().
##
## Required Arguments:
##  condor_name      - a descriptive name, used when generating directory names
##
## Optional Arguments:
##  test_name            - name of the test that is using this personal condor
##  append_condor_config - lines to be added to the (local) configuration file
##  daemon_list          - list of condor daemons to run
##
##
############################################
sub StartCondorWithParams
{
	%personal_condor_params = @_;

	if(is_windows()) {
		$ENV{LOCAL_DIR} = undef;
	}

	Initialize(%personal_condor_params);
	# Make sure at the least we have an initial Config folder to seed future
	# personal condors. Test via environment variable CONDOR_CONFIG.
	#
	my $configvalid = DoInitialConfigCheck(); 
	if($configvalid == 1) {
		die "We expected a configured HTCondor in our environment\n";
	}

	my $condor_name = $personal_condor_params{"condor_name"};

	my $testname = $personal_condor_params{"test_name"} || die "Missing test_name\n";
	$version = $personal_condor_params{"condor_name"} || die "Missing condor_name!\n";

	my $mpid = $personal_condor_params{"owner_pid"} || $pid;
	$mpid = "pdir$mpid";
	my $config_and_port = "";
	my $winpath = "";
	my $now = time();

	if(exists $personal_condor_params{"runs_dir"}) {
		$topleveldir = $personal_condor_params{"runs_dir"};
		debug( "USING $topleveldir as topleveldir for test runs\n",$debuglevel);
	}

	if(exists $personal_condor_params{"test_glue"}) {
		system("mkdir -p $topleveldir/condor_tests/$testname.saveme/$mpid$version");
		$topleveldir = "$topleveldir/condor_tests/$testname.saveme/$mpid$version";
		#system("mkdir -p $topleveldir/condor_tests/$testname.saveme/$mpid/$mpid$version");
		#$topleveldir = "$topleveldir/condor_tests/$testname.saveme/$mpid/$mpid$version";
	} else {
		if(is_windows() && is_windows_native_perl()) {
			CreateDir("-p $topleveldir\\$testname.saveme\\$mpid$version");
			$topleveldir = "$topleveldir\\$testname.saveme\\$mpid$version";
			#CreateDir("-p $topleveldir\\$testname.saveme\\$mpid\\$mpid$version");
			#$topleveldir = "$topleveldir\\$testname.saveme\\$mpid\\$mpid$version";
		} elsif(is_windows() && is_cygwin_perl()) {
			CreateDir("-p $topleveldir/$testname.saveme/$mpid$version");
			my $tmp1 = "$topleveldir/$testname.saveme/$mpid$version";
			#CreateDir("-p $topleveldir/$testname.saveme/$mpid/$mpid$version");
			#my $tmp1 = "$topleveldir/$testname.saveme/$mpid/$mpid$version";
			$topleveldir = `cygpath -m $tmp1`;
			CondorUtils::fullchomp($topleveldir);
		} else {
			CreateDir("-p $topleveldir/$testname.saveme/$mpid$version");
			$topleveldir = "$topleveldir/$testname.saveme/$mpid$version";
			#CreateDir("-p $topleveldir/$testname.saveme/$mpid/$mpid$version");
			#$topleveldir = "$topleveldir/$testname.saveme/$mpid/$mpid$version";
		}
	}

	$procdaddress = $mpid . $now . $version;


	if(exists $personal_condor_params{"personaldir"}) {
		$topleveldir = $personal_condor_params{"personaldir"};
		debug( "SETTING $topleveldir as topleveldir\n",$debuglevel);
		CreateDir("-p $topleveldir");
	}

	# if we are wrapping tests, publish log location
	$wrap_test = $ENV{WRAP_TESTS};
	if(defined  $wrap_test) {
		my $logdir = $topleveldir . "/log";
		#CondorPubLogdirs::PublishLogDir($testname,$logdir);
	}

	if(is_windows() && is_windows_native_perl()){
		$personal_config_file = $topleveldir ."\\condor_config";
	} elsif(is_windows() && is_cygwin_perl()){
		$personal_config_file = $topleveldir ."/condor_config";
	} else {
		$personal_config_file = $topleveldir ."/condor_config";
	}

	$ENV{CONDOR_CONFIG} = $masterconfig;

	# we need the condor instance early for state determination
	#print "Personal: StartCondorWithParams: Creating condor instance for: $personal_config_file\n";
    my $new_condor = CondorTest::CreateAndStoreCondorInstance( $version, $personal_config_file, 0, 0 );

	$localdir = CondorPersonal::InstallPersonalCondor();
	#

	if($localdir eq "")
	{
		return("Failed to do needed Condor Install\n");
	}

	if( CondorUtils::is_windows() == 1 ){
		if(is_windows_native_perl()) {
			$localdir =~ s/\//\\/g;
			$condorlocaldir = $localdir;
		} else {
			$winpath = `cygpath -m $localdir`;
			CondorUtils::fullchomp($winpath);
			$condorlocaldir = $winpath;
		}
		if( exists $personal_condor_params{catch_startup_tune}) {
			CondorPersonal::TunePersonalCondor($condorlocaldir, $mpid, $personal_condor_params{catch_startup_tune});
		} else {
			CondorPersonal::TunePersonalCondor($condorlocaldir, $mpid);
		}
	} else {
		if( exists $personal_condor_params{catch_startup_tune}) {
			CondorPersonal::TunePersonalCondor($localdir, $mpid,$personal_condor_params{catch_startup_tune});
		} else {
			CondorPersonal::TunePersonalCondor($localdir, $mpid);
		}
	}

	$ENV{CONDOR_CONFIG} = $personal_config_file;

	if(exists $personal_condor_params{"do_not_start"}) {
		$topleveldir = $home;
		return("do_not_start");
	}

	$collector_port = CondorPersonal::StartPersonalCondor();

	# reset topleveldir to $home so all configs go at same level
	$topleveldir = $home;

	debug( "collector port is $collector_port\n",$debuglevel);

	if( CondorUtils::is_windows() == 1 ){
		if(is_windows_native_perl()) {
			$personal_config_file =~ s/\//\\/g;
			$config_and_port = $personal_config_file . "+" . $collector_port ;
		} else {
			$winpath = `cygpath -m $personal_config_file`;
			CondorUtils::fullchomp($winpath);
			$config_and_port = $winpath . "+" . $collector_port ;
		}
	} else {
		$config_and_port = $personal_config_file . "+" . $collector_port ;
	}

	#CondorPersonal::Reset();
	debug( "StartCondor config_and_port is --$config_and_port--\n",$debuglevel);
	debug( "Personal Condor Started\n",$debuglevel);
	return( $config_and_port );
}

sub StartCondorWithParamsStart
{
	my $winpath = "";
	my $config_and_port = "";
	if(is_windows()) {
		$ENV{LOCAL_DIR} = undef;
	}

	$collector_port = CondorPersonal::StartPersonalCondor();

	debug( "collector port is $collector_port\n",$debuglevel);

	if( CondorUtils::is_windows() == 1 ){
		if(is_windows_native_perl()) {
			$_ = $personal_config_file;
			s/\//\\/g;
			$winpath = $_;
		} else {
			$winpath = `cygpath -m $personal_config_file`;
			CondorUtils::fullchomp($winpath);
		}
		$config_and_port = $winpath . "+" . $collector_port ;
	} else {
		$config_and_port = $personal_config_file . "+" . $collector_port ;
	}

	CondorPersonal::Reset();
	debug( "StartCondor config_and_port is --$config_and_port--\n",$debuglevel);
	debug( "Personal Condor Started\n",$debuglevel);
	return( $config_and_port );
}

sub debug {
    my $string = shift;
    my $level = shift;
	my $time = timestamp();
	if(!(defined $level)) { $level = 0; }
	if ($level <= $DEBUGLEVEL) {
		my $msg = "$time $string";
		print $msg;
		push @debugcollection, $msg;
	} else {
		my $msg = "$time (CP$level) $string";
		push @debugcollection, $msg;
	}
}

sub debug_flush {
	print "\nDEBUG_FLUSH:\n";
	my $logdir = `condor_config_val log`;
	fullchomp($logdir);
	print "\nLOG=$logdir and contains:\n";
	List("ls -lh $logdir");

	# what daemons does condor_who see running/exited?
	print "\ncondor_who -verb says:\n";
	system("condor_who -verb");

	# what is in our config files?
	print "\ncondor_config_val -writeconfig:file says:\n";
	system("condor_config_val -writeconfig:file -");

	print "\n------------Saved debug output is----------------\n";
	foreach my $line (@debugcollection) {
		print "$line";
	}

}

sub DebugLevel
{
	my $newlevel = shift;
	my $oldlevel = $DEBUGLEVEL;
	$DEBUGLEVEL = $newlevel;
	return($oldlevel);
}

sub timestamp {
	return strftime("%H:%M:%S", localtime);
}

sub Reset
{
	debug( "CondorPersonal RESET\n",$debuglevel);
	%personal_condor_params = ();
	%personal_config_changes = ();
	$personal_config = "condor_config";
	$personal_template = "condor_config_template";
	$personal_daemons = "";
	$personal_local = "condor_config.local";
	$personal_local_src = "";
	$personal_local_post_src = "";
	$personal_sec_prepost_src = "";
	$personal_universe = "";
	$personal_startup_wait = "true";

	$RunningTimeStamp = 0;

	$topleveldir = getcwd();
	$home = $topleveldir;
	$portchanges = "dynamic";
	$collector_port = "0";
	$personal_config_file = "";
	$condordomain = "";
	$procdaddress = "";
}

#################################################################
#
# ParsePersonalCondorParams
#
#	Parses parameter file in typical condor form of NAME = VALUE
#	and stores results into a hash for lookup later.
#

sub ParsePersonalCondorParams
{
    my $submit_file = shift || die "missing submit file argument";
    my $line = 0;

    if( ! open( SUBMIT_FILE, $submit_file ) )
    {
		die "error opening \"$submit_file\": $!\n";
		return 0;
    }
    
    debug( "reading submit file...\n" ,4);
	my $variable;
	my $value;

    while( <SUBMIT_FILE> )
    {
		CondorUtils::fullchomp($_);
		$line++;

		# skip comments & blank lines
		next if /^#/ || /^\s*$/;

		# if this line is a variable assignment...
		if( /^(\w+)\s*\=\s*(.*)$/ ) {
	    	$variable = lc $1;
	    	$value = $2;

	    	# if line ends with a continuation ('\')...
	    	while( $value =~ /\\\s*$/ ) {
				# remove the continuation
				$value =~ s/\\\s*$//;

				# read the next line and append it
				<SUBMIT_FILE> || last;
				$value .= $_;
	    	}

	    	# compress whitespace and remove trailing newline for readability
	    	$value =~ s/\s+/ /g;
			CondorUtils::fullchomp($value);
		
			# Do proper environment substitution
	    	if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ ) {
				my $envlookup = $ENV{$2};
	    		debug( "Found $envlookup in environment \n",4);
				$value = $1.$envlookup.$3;
	    	}

	    	debug( "(CondorPersonal.pm) $variable = $value\n" ,$debuglevel);
	    	#print "(CondorPersonal.pm) $variable = $value\n";

	    	# save the variable/value pair
	    	$personal_condor_params{$variable} = $value;
		} else {
	#	    debug( "line $line of $submit_file not a variable assignment... " .
	#		   "skipping\n" );
		}
    }
	close(SUBMIT_FILE);
    return 1;
}

##################################################################
#
# Run condor_config_val using the specified configuration file.
#

sub CondorConfigVal
{
    my $config_file = shift;
    my $param_name = shift;
	my $returnarrayref = shift;
	my @otherarray = ();
	my $result = "";

    my $oldconfig = $ENV{CONDOR_CONFIG};
    $ENV{CONDOR_CONFIG} = $config_file;
	#print "CondorConfigVal called with this fig:$config_file;\n";

    #my $result = `condor_config_val $param_name`;

	if (defined $returnarrayref) {
		my $res = CondorTest::runCondorTool("condor_config_val $param_name",$returnarrayref,2,{emit_output=>0,expect_result=>\&ANY});
	} else {
		my $res = CondorTest::runCondorTool("condor_config_val $param_name",\@otherarray,2,{emit_output=>0});
		my $firstline = $otherarray[0];
    	fullchomp $firstline;
		$result = $firstline;
	}

    $ENV{CONDOR_CONFIG} = $oldconfig;
    return $result;
}

#################################################################
#
# InstallPersonalCondor
#
#	We either find binaries in the environment or we install
#		a particular tar ball.
#

sub InstallPersonalCondor
{
	# this used to be used globally but now passwed
	# in from StartCondorWithParams
	#%personal_condor_params = @_;
	my %control = %personal_condor_params;

	my $master;
	my $collector;
	my $submit;
	my $iswindows = CondorUtils::is_windows() ;
	my $condorq = "";
	my $sbinloc = "";
	my $configline = "";
	my @configfiles;
	my $condordistribution;
	my $tmpconfig = $ENV{CONDOR_CONFIG};
	my $configdir = "";

	if($iswindows) {
		$condorq = Which("condor_q.exe");
	} else {
		$condorq = Which("condor_q");
	}

	if($tmpconfig =~ /^(.*\/)\w+$/) {
		$configdir = $1;
		#print "InstallPersonalCondor: CONFIG DIR:$configdir\n";

	}
	my $binloc = "";

	$condordistribution = $control{"condor"} || "nightlies";
	debug( "Install this condor --$condordistribution--\n",$debuglevel);
	if( $condordistribution eq "nightlies" ) {
		# test if this is really the environment we are in or
		# switch it to install mode.
		 if(! -f "../../condor/sbin/condor_master") {
		 	$condordistribution = "install";
		 }
	}

	if( $condordistribution eq "install" ) {
	if($iswindows == 1) {
		#print "condor distribution = install\n";
	}
		# where is the hosting condor_config file? The one assumed to be based
		# on a setup with condor_configure.

		my @config = ();
		debug("InstallPersonalCondor getting ccv -config\n",$debuglevel);
		CondorTest::runCondorTool("condor_config_val -config",\@config,2,{emit_output=>0});
		debug("InstallPersonalCondor BACK FROM ccv -config\n",$debuglevel);
		open(CONFIG,"condor_config_val -config 2>&1 | ") || die "Can not find config file: $!\n";
		while(<CONFIG>)
		{
			next if ($_ =~ /figuration source/);
			CondorUtils::fullchomp($_);
			$configline = $_;
			push @configfiles, $configline;
		}
		close(CONFIG);
		$personal_condor_params{"condortemplate"} = shift @config;
		fullchomp($personal_condor_params{"condortemplate"});

		#print " ****** Condortemplate set to <$personal_condor_params{condortemplate}>\n";

		if(exists $personal_condor_params{fresh_local}) {
		} else {
			# Always start with a freshly constructed local config file
			# so we know what we get  bt 5/13
			#$personal_condor_params{"condorlocalsrc"} = shift @configfiles;
		}

		debug("condor_q: $condorq\n",$debuglevel);
		debug("topleveldir: $topleveldir\n",$debuglevel);


		if($iswindows == 1) {
			# maybe we have a dos path
			if(is_windows_native_perl()) {
				if($condorq =~ /[A-Za-z]:/) {
					$_ = $condorq;
					s/\//\\/g;
					$condorq = $_;
					#print "condor_q now:$condorq\n";
					if($condorq =~ /^([A-Za-z]:\\.*?)\\bin\\(.*)$/) {
						#print "setting binloc:$1 \n";
						$binloc = $1;
						$sbinloc = $1;
					}
				}
			} else {
				my $tmp = `cygpath -m $condorq`;
				fullchomp($tmp);
				#print "InstallPersonalCondor:condorq:$tmp\n";
				$condorq = $tmp;
				if($condorq =~ /[A-Za-z]:/) {
					if($condorq =~ /^([A-Za-z]:\/.*?)\/bin\/(.*)$/) {
						#print "setting binloc:$1 \n";
						$binloc = $1;
						$sbinloc = $1;
					}
				}
			}
		}

		if($binloc eq "") {
			if( $condorq =~ /^(\/.*\/)(\w+)\s*$/ ) {
				debug( "Root path $1 and base $2\n",$debuglevel);
				$binloc = $1;	# we'll get our binaries here.
			} elsif(-f "../release_dir/bin/condor_status") {
				#print "Bummer which condor_q failed\n";
				#print "Using ../release_dir/bin(s)\n";
				$binloc = "../release_dir/bin"; # we'll get our binaries here.
			}
			else
			{
				#print "which condor_q responded: $condorq! CondorPersonal Failing now\n";
				debug_flush();
				die "Can not seem to find a Condor install!\n";
			}
		}
		

		if($sbinloc eq "") {
			if( $binloc =~ /^(\/.*\/)s*bin\/\s*$/ )
			{
				debug( "Root path to sbin is $1\n",$debuglevel);
				$sbinloc = $1;	# we'll get our binaries here. # local_dir is here
			}
			else
			{
				debug_flush();
				die "Can not seem to locate Condor release binaries\n";
			}
		}

		debug( "Sandbox started rooted here: $topleveldir\n",$debuglevel);

		#print "Sandbox started rooted here: $topleveldir\n";
		if(is_windows_native_perl()) {
			#print "before making local dirs\n";
			#CondorUtils::dir_listing("$topleveldir");
			my $cwd = getcwd();
			chdir ("$topleveldir");
			CreateDir("execute spool log log\\tmp");
			chdir ("$cwd");
			#print "after making local dirs\n";
			#CondorUtils::dir_listing("$topleveldir");
		} else {
			system("cd $topleveldir && mkdir -p execute spool log log/tmp");
		}
	} elsif( $condordistribution eq "nightlies" ) {
	if($iswindows == 1) {
	}
		# we want a mechanism by which to find the condor binaries
		# we are testing. But we know where they are relative to us
		# ../../condor/bin etc
		# That is simply the nightly test setup.... for now at least
		# where is the hosting condor_config file? The one assumed to be based
		# on a setup with condor_configure.

		debug(" Nightlies - find environment config files\n",$debuglevel);
		my @config = ();
		CondorTest::runCondorTool("condor_config_val -config",\@config,2,{emit_output=>0});
		$personal_condor_params{"condortemplate"} = shift @config;
		$personal_condor_params{"condorlocalsrc"} = shift @config;
		fullchomp($personal_condor_params{"condortemplate"});
		fullchomp($personal_condor_params{"condorlocalsrc"});

		#print " ****** Case nightlies leading to <$personal_condor_params{condortemplate}> and $personal_condor_params{condorlocalsrc}\n";
		debug( "My path to condor_q is $condorq and topleveldir is $topleveldir\n",$debuglevel);

		if( $condorq =~ /^(\/.*\/)(\w+)\s*$/ )
		{
			debug( "Root path $1 and base $2\n",$debuglevel);
			$binloc = $1;	# we'll get our binaries here.
		}
		else
		{
			#print "which condor_q responded: $condorq! CondorPersonal Failing now\n";
			debug_flush();
			die "Can not seem to find a Condor install!\n";
		}
		

		if( $binloc =~ /^(\/.*)\/bin\/\s*$/ )
		{
			debug( "Root path to sbin is $1\n",$debuglevel);
			$sbinloc = $1;	# we'll get our binaries here. # local_dir is here
		}
		else
		{
			debug_flush();
			die "Can not seem to locate Condor release binaries\n";
		}

		debug( "My path to condor_q is $binloc and topleveldir is $topleveldir\n",$debuglevel);

		debug( "Sandbox started rooted here: $topleveldir\n",$debuglevel);

		if(is_windows_native_perl()) {
			my $cwd = getcwd();
			system("chdir $topleveldir");
			CreateDir("execute spool log log/tmp");
			system("chdir $cwd");
		} else {
			system("cd $topleveldir && mkdir -p execute spool log log/tmp");
		}
	} elsif( -e $condordistribution ) {
		if($iswindows == 1) {
		}
		# in this option we ought to run condor_configure
		# to get a current config files but we'll do this
		# after getting the current condor_config from
		# the environment we are in as it is supposed to
		# have been generated this way in the nightly tests
		# run in the NWO.

		my $res = chdir "$topleveldir";
		if(!$res) {
			die "chdir $topleveldir failed: $!\n";
			exit(1);
		}
		system("cd $topleveldir && mkdir -p execute spool log");
		system("tar -xf $home/$condordistribution");
		$sbinloc = $topleveldir; # local_dir is here
		chdir "$home";
	} else {
		debug_flush();
		die "Undiscernable install directive! (condor = $condordistribution)\n";
	}

	debug( "InstallPersonalCondor returning $sbinloc for LOCAL_DIR setting\n",$debuglevel);

	return($sbinloc);
}

sub FetchParams
{
	return(%personal_condor_params);
}

#################################################################
#
# TunePersonalCondor
#
# 	Most changes go into the condor_config.local file but
#		some changes are done to the condor_config template.
#
#		RELEASE_DIR, LOCAL_DIR and LOCAL_CONFIG_FILE are 
#		adjusted from the main template file and other 
#		changes are in the condor_config.local file.
#

sub TunePersonalCondor
{
	my %control = %personal_condor_params;
	my $myhost = CondorTest::getFqdnHost();
	my @domainparts = split /\./, $myhost;
	my $condorhost = "";
	my $collectorhost = "";
	my $localdir = shift;
	my $mpid = shift;
	my $scheddname;
	my $startdname;
	my $minimalconfig = 0;
	my $returnarrayref = shift;
	my $iswindows = CondorUtils::is_windows();


	if(!(defined $mpid)) {
		$mpid = $$;
		$mpid = "pdir$mpid";
	}

my $socketdir = "";

	if($MOVESOCKETDIR == 1) {


		# The tests get pretty long paths to LOCK_DIR making unix sockets exceed
		# the max character length. So in remote_pre we create a folder to hold 
		# the test run's socket folder. /tmp/tds$pid. We place this name we will
		# need to configure each personal with in condor_tests/SOCKETDIR and we will
		# configure with our own pid. Remote_post removes this top level directory.

		if( CondorUtils::is_windows() == 0 ){
			# windows does not have a path length limit
			if(!(-f "SOCKETDIR")) {
				print "Creating SOCKETDIR?\n";
				my $privatetmploc = "/tmp/tds$$";
				print "tmp loc:$privatetmploc\n";
				$socketdir = "SOCKETDIR";
				system("mkdir $privatetmploc;ls /tmp");
				open(SD,">$socketdir") or print "Failed to create:$socketdir:$!\n";
				print SD "$privatetmploc\n";
				close(SD);
			} else {
				open(SD,"<SOCKETDIR") or print "Failed to open:SOCKETDIR:$!\n";
				$socketdir = (<SD>);
				chomp($socketdir);
				print "Fetch master SOCKETDIR:$socketdir\n";
				$socketdir = "$socketdir" . "/$$";
				print "This tests socketdir:$socketdir\n";
			}
		}
	}

	#print " ****** TunePersonalCondor with localdir set to <$localdir>\n";

	debug( "TunePersonalCondor setting LOCAL_DIR to $localdir\n",$debuglevel);

	debug( "My basic name is $myhost\n",$debuglevel);


	# was a special condor host called out?
	if( exists $control{"condorhost"} )
	{
		$condorhost = $control{"condorhost"};
	}

	# was a special condor collector called out?
	if( exists $control{"collector"} )
	{
		$collectorhost = $control{"collector"};
	}

	# was a special domain called out?
	if( exists $control{"condordomain"} )
	{
		$condordomain = $control{"condordomain"};
	}

	if( $condordomain ne "" ) {
		$condorhost = $myhost . "." . $condordomain;
	} else {
		$condorhost = $myhost;
	}

	debug( "Fully qualified domain name is ************************ $condorhost ********************\n",$debuglevel);

	# was a special template called out?
	if( exists $control{"condortemplate"} )
	{
		$personal_template = $control{"condortemplate"};
	}

	# was a special config file called out?
	if( exists $control{"condorconfig"} )
	{
		$personal_config = $control{"condorconfig"};
	} else {
		$personal_config = "condor_config";

		# store this default in the personal condor params so
		# other parts of the code can rely on it.
		$personal_condor_params{"condorconfig"} = $personal_config;
	}

	# was a special daemon list called out?
	if( exists $control{"daemon_list"} )
	{
		#print "New daemon list called out <$control{daemon_list}>\n";
		$personal_daemons = $control{"daemon_list"};
	}

	# was a special local config file name called out?
	if( exists $control{"condorlocal"} )
	{
		$personal_local = $control{"condorlocal"};
	} else {
		$personal_local = "condor_config.local";
	}

	# was a special local config file src called out?
	if( exists $control{"condorlocalsrc"} )
	{
		$personal_local_src = $control{"condorlocalsrc"};
	}

	# was a special local config file post src called out?
	if( exists $control{"secprepostsrc"} )
	{
		$personal_sec_prepost_src = $control{"secprepostsrc"};
	}

	# was a special local config file post src called out?
	if( exists $control{"localpostsrc"} )
	{
		$personal_local_post_src = $control{"localpostsrc"};
	}

	# is this for a specific universe like parallel?
	if( exists $control{"universe"} )
	{
		$personal_universe = $control{"universe"};
		debug( "HMMMMMMMMMMM universe request is $personal_universe\n",$debuglevel);
	}


	 debug( "Proto file is --$personal_template--\n",3);

	$personalmaster = "$topleveldir/sbin/condor_master";

	#filter fig file storing entries we set so we can test
	#for completeness when we are done
	my $mytoppath = "";
	if( CondorUtils::is_windows() == 1 ){
		if(is_windows_native_perl()) {
			$_ = $topleveldir;
			s/\//\\/g; # convert to reverse slants
			#s/\\/\\\\/g;
			$mytoppath = $_;
		} else {
			$mytoppath = `cygpath -m $topleveldir`;
		}
		CondorUtils::fullchomp($mytoppath);
	} else {
		$mytoppath =  $topleveldir;
	}


debug( "HMMMMMMMMMMM personal local is $personal_local , mytoppath is $mytoppath",$debuglevel);

	my $line;

	open(TEMPLATE,"<$personal_template")  || die "Can not open template: $personal_template: $!\n";
	debug( "want to open new config file as $topleveldir/$personal_config\n",$debuglevel);
	open(NEW,">$topleveldir/$personal_config") || die "Can not open new config file: $topleveldir/$personal_config: $!\n";

	# There is an interesting side effect of reading and changing the condor_config
	# template and then at the end, add the new LOCAL_DIR entree. That is when the constructed 
	# file is inspected it looks like the LOCAL_DIR came from the environment. 
	# So we are going to parse for a comment only line and only drop it out if it is NOT
	# the one followed by "# from <Environment>". When that is the line that follows, we will drop
	# out the new LOCAL_DIR, then a blank line and THEN those two saved lines.

	my $lastline = "";
	my $thisline = "";
	while(<TEMPLATE>)
	{
		CondorUtils::fullchomp($_);
		$line = $_;
		if( $line =~ /^LOCAL_DIR\s*=.*/ )
		# this is now beeing added to Config/condor_config so should propograte
		{
			 debug( "-----------$line-----------\n",4);
			$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $mytoppath\n";
			print NEW "LOCAL_DIR = $mytoppath\n";
		} elsif( $line =~ /^LOCAL_CONFIG_FILE\s*=.*/ ) {
			debug( "-----------$line-----------\n",4);
			if($iswindows) {
				if(is_windows_native_perl()) {
					#print "My toppath:$mytoppath\n";
					$personal_config_changes{"LOCAL_CONFIG_FILE"} = "LOCAL_CONFIG_FILE = $mytoppath\\$personal_local\n";
					print NEW "LOCAL_CONFIG_FILE = $mytoppath\\$personal_local\n";
				} else {
					$personal_config_changes{"LOCAL_CONFIG_FILE"} = "LOCAL_CONFIG_FILE = $mytoppath/$personal_local\n";
					print NEW "LOCAL_CONFIG_FILE = $mytoppath/$personal_local\n";
				}
			} else {
				$personal_config_changes{"LOCAL_CONFIG_FILE"} = "LOCAL_CONFIG_FILE = $mytoppath/$personal_local\n";
				print NEW "LOCAL_CONFIG_FILE = $mytoppath/$personal_local\n";
			}
		} elsif( $line =~ /^#\s*$/ ) {
			#print "save $line could be comment before environment label\n";
			$lastline = $line;
		} elsif( $line =~ /^#.*?Environment.*$/ ) {
			$thisline = $line;
			#print "TunePersonalCondor: setting LOCAL_DIR=$mytoppath\n"; 
			print NEW "LOCAL_DIR = $mytoppath\n";
			print NEW "\n";
			print NEW "$lastline\n";
			print NEW "$thisline\n";
		} elsif( $line =~ /^#.*$/ ) {
			#print "Not environment label, drop both lines\n";
			print NEW "$lastline\n";
			print NEW "$thisline\n";
		} elsif( $line =~ /^LOCAL_CONFIG_DIR\s*=.*/ ) {
			# eat this entry
		} else {
			print NEW "$line\n";
		}
	}
	close(TEMPLATE);
	close(NEW);

	open(NEW,">$topleveldir/$personal_local")  || die "Can not open template: $!\n";

	if($minimalconfig == 0) {
		if( ! exists $personal_config_changes{"CONDOR_HOST"} )
		{
			$personal_config_changes{"CONDOR_HOST"} = "CONDOR_HOST = $condorhost\n";
		}


		if( exists $control{"ports"} )
		{
			debug( "Port Changes being Processed!!!!!!!!!!!!!!!!!!!!\n",$debuglevel);
			$portchanges = $control{"ports"};
			debug( "portchanges set to $portchanges\n",$debuglevel);
		}

		debug( "opening to write: $topleveldir/$personal_local\n",$debuglevel);

		if($personal_daemons ne "")
		{
			# Allow the collector to run on the default and expected port as the main
			# condor install on this system.
			print NEW "# Adding requested daemons\n";
			print NEW "DAEMON_LIST = $personal_daemons\n";
		} else {
			print NEW "DAEMON_LIST = MASTER STARTD SCHEDD COLLECTOR NEGOTIATOR\n";
		}

				if(is_windows_native_perl()) {
					print NEW "NEGOTIATOR_ADDRESS_FILE = \$(LOG)\\.negotiator_address\n";
	    			print NEW "SCHEDD_ADDRESS_FILE = \$(LOG)\\.schedd_address\n";
				} else {
					print NEW "NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address\n";
	    			print NEW "SCHEDD_ADDRESS_FILE = \$(LOG)/.schedd_address\n";
				}

				print NEW "SCHEDD_INTERVAL = 5\n";
				print NEW "UPDATE_INTERVAL = 5\n";
				print NEW "NEGOTIATOR_INTERVAL = 5\n";
				print NEW "CONDOR_ADMIN = \n";
				print NEW "CONDOR_JOB_POLL_INTERVAL = 5\n";
				print NEW "PERIODIC_EXPR_TIMESLICE = .99\n";
				print NEW "JOB_START_DELAY = 0\n";
				print NEW "DAGMAN_USER_LOG_SCAN_INTERVAL = 1\n";
				print NEW "LOCK = \$(LOG)\n";
				if($iswindows == 1) {
				#print NEW "PROCD_LOG = \$(LOG)/ProcLog\n";
					print NEW "# Adding procd pipe for windows\n";
					print NEW "PROCD_ADDRESS = \\\\.\\pipe\\$procdaddress\n";
				}

		my $jvm = "";
		my $java_libdir = "";
		my $exec_result;
		my $javabinary = "";

		# now we consider configuration requests

		if( exists $control{"slots"} )
		{
			my $myslots = $control{"slots"};
			debug( "Slots wanted! Number = $myslots\n",$debuglevel);
			print NEW "# Adding slot request from param file\n";
			print NEW "NUM_CPUS = $myslots\n";
			print NEW "SLOTS = $myslots\n";
			print NEW "# Done Adding slot request from param file\n";
		}

		if($personal_local_src ne "")
		{
			print NEW "# Requested local config: $personal_local_src\n";
			#print "******************** Must seed condor_config.local <<$personal_local_src>> ************************\n";

	debug( "opening to read: $personal_local_src\n",$debuglevel);

			open(LOCSRC,"<$personal_local_src") || die "Can not open local config template: $!\n";
			while(<LOCSRC>)
			{
				CondorUtils::fullchomp($_);
				$line = $_;
				print NEW "$line\n";
			}
			# now make sure we have the local dir we want after the generic .local file is seeded in
#				$line = $personal_config_changes{"LOCAL_DIR"};
#				print NEW "$line\n";
#				# and a lock directory we like
			close(LOCSRC);
		}

		if($personal_sec_prepost_src ne "")
		{
			debug( "Adding to local config file from $personal_sec_prepost_src\n",$debuglevel);
			open(SECURITY,"<$personal_sec_prepost_src")  || die "Can not do local config additions: $personal_sec_prepost_src: $!\n";
			print NEW "# Adding changes requested from $personal_sec_prepost_src\n";
			while(<SECURITY>)
			{
				print NEW "$_";
			}
			close(SECURITY);
			print NEW "# Done Adding changes requested from $personal_sec_prepost_src\n";
		}


		if($personal_local_post_src ne "")
		{
			debug("Adding to local config file from $personal_local_post_src\n",$debuglevel);
			open(POST,"<$personal_local_post_src")  || die "Can not do local config additions: $personal_local_post_src:$!\n";
			print NEW "# Adding changes requested from $personal_local_post_src\n";
			while(<POST>)
			{
				print NEW "$_";
			}
			close(POST);
			print NEW "# Done Adding changes requested from $personal_local_post_src\n";
		}

		if( exists $control{append_condor_config} ) {
		    print NEW "# Appending from 'append_condor_config'\n";
		    print NEW "$control{append_condor_config}\n";
		    print NEW "# Done appending from 'append_condor_config'\n";
		}

		# assume an array reference
		if( exists $control{append_condor_config_plus} ) {
		    print NEW "# Appending from 'append_condor_config_plus'\n";
			my $arrayref = $control{append_condor_config_plus};
			foreach my $line (@{$arrayref}) {
		    	print NEW "$line\n";
			}
		    print NEW "# Done appending from 'append_condor_config_plus'\n";
		}

	}

	#lets always overrule existing A__DEBUG with one that adds to it D_CMD
	print NEW "ALL_DEBUG = \$(ALL_DEBUG) D_CMD:1\n";
	# we are testing. dramatically reduce MaxVacateTime
	print NEW "JOB_MAX_VACATE_TIME = 15\n";

	close(NEW);
	if (defined $returnarrayref) {
		PostTunePersonalCondor($personal_config_file,$returnarrayref);
	} else {
		PostTunePersonalCondor($personal_config_file);
	}
}


#################################################################
#
#   PostTunePersonalCondor() is called after TunePersonalCondor.
#   It assumes that the configuration file is all set up and
#   ready to use.

sub PostTunePersonalCondor
{
    my $config_file = shift;
	my $outputarrayref = shift;
	debug("PostTunePersonalCondor: getting DAEMON_LIST from $config_file\n",2);

    my $configured_daemon_list;
	if (defined $outputarrayref) {
    	$configured_daemon_list = CondorConfigVal($config_file,"daemon_list", $outputarrayref);
	} else {
    	$configured_daemon_list = CondorConfigVal($config_file,"daemon_list");
	}
}

#################################################################
#
#	StartPersonalCondor will start a personal condor which has
#	been set up. If the ports are dynamic, it will look up the
#   address and return the port number.
#

sub StartPersonalCondor
{
	my %control = %personal_condor_params;

	my $personalmaster = "";
	
	# If we start a personal Condor as root (for testing the VM universe),
	# we need to change the permissions/ownership on the directories we
	# made so that the master (which runs as condor) can use them.
	if( ! CondorUtils::is_windows() && ( $> == 0 )) {
		my $testName = $control{ 'test_name' };
		system( "chown condor.condor $home/${testName}.saveme >& /dev/null" );
		system( "chown -R condor.condor $home/${testName}.saveme/pdir$pid >& /dev/null" );
	}

	my $configfile = $control{"condorconfig"};
	#my $fullconfig = "$topleveldir/$configfile";
	
	my $fullconfig = "$ENV{CONDOR_CONFIG}";

	#print "StartPersonalCondor CONDOR_CONFIG=$fullconfig\n";

	debug( "Want $configfile for config file\n",$debuglevel);
	my $figpath = "";

	if( CondorUtils::is_windows() == 1 ){
		if(is_windows_native_perl()) {
			$personalmaster = "start $localdir" . "\\bin\\condor_master.exe -f";
		} else {
			$figpath = `cygpath -m $fullconfig`;
			CondorUtils::fullchomp($figpath);
			$fullconfig = $figpath;
			# note: on windows all binaaries in bin!
			my $tmp = `cygpath -m $localdir`;
			CondorUtils::fullchomp($tmp);
			$personalmaster = $tmp . "/bin/condor_master.exe -f &";
		}
	} else {
		$personalmaster = $localdir . "sbin/condor_master -f &";
	}

	# We may not want to wait for certain daemons to talk
	# to each other on startup.

	if( exists $control{"daemonwait"} ) {
		my $waitparam = $control{"daemonwait"};
		if($waitparam eq "false") {
			$personal_startup_wait = "false";
		}
	}


	# set up to use the existing generated configfile
	my $condorstate = 0;
	$ENV{CONDOR_CONFIG} = $fullconfig;
	my $condor_instance = CondorTest::GetPersonalCondorWithConfig($fullconfig);
	if($condor_instance != 0) { 
		$condorstate = $condor_instance->GetCondorAlive();
	} else {
		$condorstate = 0;
	}

	my $fig = $ENV{CONDOR_CONFIG};
	debug( "Condor_config from environment is --$fig--\n",$debuglevel);

	# At the momment we only restart/start a personal we just configured 
	# or reconfigured

	if( $condorstate == 0 ) {
		#  not running with this config so treat it like a start case
		debug("Condor state is off\n",$debuglevel);
		debug( "start up the personal condor!--$personalmaster--\n",$debuglevel);
		# when open3 is used it sits and waits forever
		if( exists $control{catch_startup_start}) {
			print "catch_startup_start seen. Calling runCondorTool\n";
			runCondorTool("$personalmaster",$control{catch_startup_start},2,{emit_output=>1});
		} else {
			my $res = system("$personalmaster");
			if($res != 0) {
				print "Failed system call starting master\n";
			}
		}
	} else {
		debug_flush();
		die "Bad state for a new personal condor configuration! running :-(\n";
	}

	# is test opting into new condor personal status yet?
	my $res = 1;
	sleep(1);
	if(exists $control{"no_wait"}) {
		#print "use no methods here to be sure daemons are up.\n";
	} else {
		#print "NO_WAIT not set???????\n";
		my $condor_name = $personal_condor_params{"condor_name"};
		$res = NewIsRunningYet($fullconfig, $condor_name);
	}

	if($res == 0) {
		debug_flush();
		die "Can not continue because condor is not running!!!!\n";
	}

	# if this was a dynamic port startup, return the port which
	# the collector is listening on...

	if( $portchanges eq "dynamic" )
	{
		debug("Looking for collector port!\n",$debuglevel);
		return( FindCollectorPort() );
	}
	else
	{
		debug("NOT Looking for collector port!\n",$debuglevel);
		return("0");
	}
}

sub ProcessStateWanted
{
	my $condor_config = shift;
	#print "ProcessStateWanted: $condor_config\n";
	my $condor_instance = CondorTest::GetPersonalCondorWithConfig($condor_config);
	my $direction = $condor_instance->GetCondorDirection(); # up or down
	#print "in ProcessStateWanted going:$direction\n";
	if($condor_instance == 0) {
		return("?");
	}
	# lets look for best case first
	my $scheddseen = "";
	if($direction eq "up") {
		my $alldaemons = $condor_instance->HasAllLiveDaemons();
		if($alldaemons eq "yes") {
			$scheddseen = $condor_instance->CollectorSeesSchedd();
			if($scheddseen eq "yes") {
				return("up");
			}
			return("alldaemonsup");
		}
	} else {
		my $nodaemons = $condor_instance->HasNoLiveDaemons();
		if($nodaemons eq "yes") {
			return("down");
		}
	}

	my $master = $condor_instance->HasLiveMaster();
	if($master == 0) {
		return("down");
	}

	return("hasmaster");
}

sub StateChange
{
	my $desiredstate = shift || croak "No desired state passed in\n";
	my $config = shift;
	my $timelimit = shift;
	my $masterlimit = shift;
	my $RunningTimeStamp = time;
	my $finaltime = 0;
	my $state = "";
	my $now = 0;

	# we'll use this to set alive field in instance correctly
	# or upon error to drop out condor_who data
	my $condor_config = $ENV{CONDOR_CONFIG};
	my $condor_instance = CondorTest::GetPersonalCondorWithConfig($condor_config);
	my $node_name = $condor_instance->GetCondorName() || "Condor";
	my $daemonlist = $condor_instance->GetDaemonList();

	debug("CondorPersonal Waiting $timelimit sec for <$node_name> to be $desiredstate. MasterTime:$masterlimit\n\tDAEMON_LIST = $daemonlist\n", 1);
	#print "\tCONDOR_CONFIG=$config\n";

	while($state ne $desiredstate) {
		#print "Waiting for state: $desiredstate current $state\n";
		my $amialive = $condor_instance->HasLiveMaster();
		$now = time;
		if(($amialive == 0) &&($desiredstate eq "up")) {
			if(($now - $RunningTimeStamp) >= $masterlimit) {
				print "StateChange: <$node_name> Master did not start in $masterlimit seconds, giving up.\n";
				$condor_instance->DisplayWhoDataInstances();
				return(0);
			}
		}
		if(($amialive == 1) &&($desiredstate eq "down")) {
			if(($now - $RunningTimeStamp) >= $masterlimit) {
				print "StateChange: <$node_name> Master did not exit in $masterlimit seconds. giving up.\n";
				$condor_instance->DisplayWhoDataInstances();
				return(0);
			}
		}
		if((($now - $RunningTimeStamp) >= $timelimit) && ($timelimit >= $masterlimit)) {
			print "StateChange: <$node_name>:$desiredstate not seen after $timelimit seconds. giving up\n";
			$condor_instance->DisplayWhoDataInstances();
			return(0);
		}
		#print "StateChange: again\n";
		#CollectWhoData($desiredstate);
		CollectWhoData();
		$state = ProcessStateWanted($config);
		#print "StateChange: now:$state\n";
		CheckNamedFWTimed();
		sleep 1;
	}
	if($desiredstate eq "up") {
		$condor_instance->SetCondorAlive(1);
	} elsif($desiredstate eq "down") {
		$condor_instance->SetCondorAlive(0);
	} else {
		die "Only up/down transitions expected to be requested\n";
	}
	#$condor_instance->DisplayWhoDataInstances();
	$now = time;
	$finaltime = ($now - $RunningTimeStamp);
	if($desiredstate eq "up") {
		debug("Condor <$node_name> is running. ($finaltime of $timelimit seconds)\n", 1);
	} elsif($desiredstate eq "down") {
		debug("Condor <$node_name> is off. ($finaltime of $timelimit seconds)\n", 1);
	}
	return(1);
}

sub NewIsRunningYet {
	my $config = shift;
	my $name = shift;
	#print "Checking running of: $name config:$config \n";
	# ON going up or down, first number total time to allow a full up state, but
	# master has to be alive by second number or bail
	my $res = StateChange("up", $config, 120, 30);
	return $res;
}

sub NewIsDownYet {
	my $config = shift;
	my $name = shift;
	if(defined $name) {
		#print "Checking running of: $name config:$config \n";
	} else {
		#print "This config does not have a condor instance condor_name:$config\n";
	}
	my $res = StateChange("down", $config, 120, 160);
	return $res;
}


#################################################################
#
# dual State thoughts which condor_who may detect
#
# turning on, Unknown
# turning on, Not Run Yet
# turning on, Coming up
# turning on, mater alive
# turning on, collector alive
# turning on, collector knows xxxxx
# turning on, all daemons
# going down, all daemons
# going down, collector knows XXXXX
# going down, collector alive
# going down, master alive
# down
# $self = {
   # textfile => shift,
   # placeholders => { }         #  { }, not ( )
# };
# ...


# $self->{placeholders}->{$key} = $value;
# delete $self->{placeholders}->{$key};
# @keys = keys %{$self->{placeholders}};
# foreach my ($k,$v) each %{$self->{placeholders}} { ... }
#
#

sub CollectWhoData
{
	my $desiredstate = shift;
	# experient to vary by going up vs down OFF now
	# and nothing is passed in
	my @whoarray;
	#print "CollectWhoData for this Condor:<$ENV{CONDOR_CONFIG}>\n";

	# Get condor instance for this config
	#print CondorUtils::TimeStr() . " CollectWhoData start\n";
	my $usequick = 1;
	my $condor = CondorTest::GetPersonalCondorWithConfig($ENV{CONDOR_CONFIG});

	#$condor->DisplayWhoDataInstances();
	# condor_who -quick is best before master is alive
	if($condor != 0) {
		if(defined $desiredstate) {
			if($desiredstate eq "down"){
				print "going down and using quick mode\n";
			} else {
				if($condor->HasLiveMaster() == 1) {
					$usequick = 0;
				}
			}
		} else {
			my $hasLive = $condor->HasLiveMaster();
			#print "HasLiveMaster says:$hasLive\n";
			if($condor->HasLiveMaster() == 1) {
				$usequick = 0;
			}
		}
	} else {
		die "CollectWhoData with no condor instance yet\n";
	}
    my $logdir = `condor_config_val log`;
	$_ = $logdir;
	s/\\/\//g;
	$logdir = $_;
    CondorUtils::fullchomp($logdir);

	if($usequick == 1) {
		#print "Using -quick\n";
		CondorTest::runCondorTool("condor_who -quick -daemon -log \"$logdir\"",\@whoarray,2,{emit_output=>0});
		foreach my $wholine (@whoarray) {
			CondorUtils::fullchomp($wholine);
			# print timestamp() .  ": raw whodataline: $wholine\n";
			if($wholine =~ /(\w*)\s+(.*?)\s+(.*?)\s+(.*?)/) {
				# print timestamp() .  ": Who data with 4 fields:$1,$2,$3,$4\n";
				#print "condor_who -quick fields. $1 daemon name $2 pid\n";
				#id this is the master is pid real?
				my $savepid = $2;
				my $processstring = "";
				if($1 eq "Master") {
					#print "Master found\n";
					if(CondorUtils::is_windows() == 1) {
				    	my @grift = `tasklist | grep $savepid`;
						foreach my $process (@grift) {
							#print "consider:$process saved pid: $savepid\n";
							if($process =~ /(.*?)\s+(\d+)\s+(\w+).*/) {
								$processstring = $1;
								if($2 eq $savepid) {
									#print "Pids equal:$processstring\n";
									# does this process have master in binary
									if($processstring =~ /condor_master/) {
										#print "Is master, thus master alive\n";
										CondorTest::LoadWhoData("Master","yes",$savepid,"","","","");
									}
								}
							}
						}
					} else {
						#print "Master record\n";
						if($savepid ne "no") {
							my @psdata = `ps $savepid`;
							my $pssize = @psdata;
							#print "ps data on $savepid: $psdata[1]\n";
							if($pssize >= 2) {
								if($psdata[1] =~ /condor_master/) {
									#Mark master alive
									#print "Marking Master Alive*************************************************************\n";
									#print "Before LoadWhoData:\n";
									CondorTest::LoadWhoData("Master","yes",$savepid,"","","","");
								}
							}
						}
					}
				} #else {
					#print "Not Master but $1\n";
					#next if $wholine =~ /^Daemon.*$/; # skip column headings
					#next if $wholine =~ /^\-\-\-\-\-\-.*$/; # skip dashes
					#CondorTest::LoadWhoData($1,$2,"","","","","");
				#}
			}
		}
	} else {
		CondorTest::runCondorTool("condor_who -daemon -log \"$logdir\"",\@whoarray,2,{emit_output=>0});

		foreach my $wholine (@whoarray) {
			CondorUtils::fullchomp($wholine);
			next if $wholine =~ /^Daemon.*$/; # skip column headings
			next if $wholine =~ /^\-\-\-\-\-\-.*$/; # skip dashes
			# print timestamp()  . ": rawhodataline: $wholine\n";
			if($wholine =~ /(.*?)\s+(.*?)\s+(.*?)\s+(.*?)\s+(.*?)\s+<(.*)>\s+(.*)/) {
				# print timestamp() . ": Who data with 7 fields:$1,$2,$3,$4,$5,$6,$7\n";
				# print "Parse:$wholine\n";
				# print "Before LoadWhoData: $1,$2,$3,$4,$5,$6,$7\n";
				# this next call assumes we are interested in currently configed personal condor
				# which means a lookup for condor instance for each daemon
				CondorTest::LoadWhoData($1,$2,$3,$4,$5,$6,$7);
			} elsif($wholine =~ /(.*?)\s+(.*?)\s+(.*?)\s+(.*?)\s+(.*?).*/) {
				# print timestamp() . ": Who data with 5 fields:$1,$2,$3,$4,$5\n";
				# print "Before LoadWhoData: $1,$2,$3,$4,$5\n";
				CondorTest::LoadWhoData($1,$2,$3,$4,$5,"","");
			} else {
				#print "CollectWhoData: Parse Error: $wholine\n";
			}
		}
	}
	#print CondorUtils::TimeStr() . " CollectWhoData done\n";
}

sub KillDaemons
{
	my $desiredconfig = shift;
	my $oldconfig = $ENV{CONDOR_CONFIG};
	$ENV{CONDOR_CONFIG} = $desiredconfig;
	my $condor_name = $personal_condor_params{"condor_name"};
	my $condor_instance = CondorTest::GetPersonalCondorWithConfig($desiredconfig);
	my $alive = $condor_instance->GetCondorAlive();
	if($alive == 0) {
		# nothing to do since it is not marked as ever coming up
		return(1);
	}

	CondorTest::runToolNTimes("condor_off -master -fast",1,0,{expect_result=>\&ANY,emit_output=>0});

	my $res = NewIsDownYet($desiredconfig, $condor_name);

	# reset config to whatever it was.
	$ENV{CONDOR_CONFIG} = $oldconfig;
	return($res);
}

#################################################################
#
# KillDaemonPids
#
#	Find the log directory via the config file passed in. Then
#	open the PIDS fill and kill every pid in it for a sure kill.
#
#################################################################

sub KillDaemonPids
{
	my $desiredconfig = shift;
	my $oldconfig = $ENV{CONDOR_CONFIG};
	#print "Using new kill method\n";
	KillDaemons($desiredconfig);
	#print "Exit KillDaemonPids after calling KillDaemons\n";
	return(0);
}

#################################################################
#
# FindCollectorPort
#
# 	Looks for collector_address_file via condor_config_val and tries
#		to parse port number out of the file.
#

sub FindCollectorAddress
{
	my $collector_address_file = `condor_config_val collector_address_file`;
	my $line;
	CondorUtils::fullchomp($collector_address_file);

	debug( "Looking for collector port in file ---$collector_address_file---\n",$debuglevel);

	if($collector_address_file eq "") {
		debug( "No collector address file defined! Can not find port\n",$debuglevel);
		return("0");
	}

	if( ! -e "$collector_address_file") {
		debug( "No collector address file exists! Can not find port\n",$debuglevel);
		return("0");
	}

	open(COLLECTORADDR,"<$collector_address_file")  || die "Can not open collector address file: $!\n";
	while(<COLLECTORADDR>) {
		CondorUtils::fullchomp($_);
		$line = $_;
		if( $line =~ /^\s*(<[^>]+>)\s*$/ ) {
			debug( "Collector address is $1\n",$debuglevel);
			return($1);
		} else {
			debug( "$line\n",$debuglevel);
		}
	}
	close(COLLECTORADDR);
	debug( "No collector address found in collector address file!\n",$debuglevel);
	return("");
}

sub FindCollectorPort
{
    my $addr = FindCollectorAddress();
    if( $addr =~ /^(\d+\.\d+\.\d+\.\d+):(\d+)$/ ) {
	debug( "Collector ip $1 and port $2\n",$debuglevel);
	return($2);
    } else {
	debug( "Failed to extract port from collector address: $addr\n",$debuglevel);
    }
    return("0");
}

#################################################################
#
# SaveMeSetup
#
# Make the saveme directory for a test, Create the pid based
# location for the current test within this saveme directory
# and then create a symbolic link to this pid directory. By doing this
# when the personal condor setup go to make a pid directory to
# run in, it ends up running within the saveme directory.
# This saveme directory allows more data to be returned during the
# nightly testing.
#
# If all is good the current pid is returned but if there 
# is an error 0 is returned.
#
#################################################################

sub SaveMeSetup
{
	my $testname = shift;
	print "Into SaveMeSetup for:$testname\n";
	my $mypid = $$;
	my $res = 1;
	my $mysaveme = $testname . ".saveme";
	$res = CreateDir("-p $mysaveme");
	if($res != 0) {
		print "SaveMeSetup: Could not create \"saveme\" directory for test\n";
		return(0);
	}
	my $mypiddir = $mysaveme . "/pdir" . $mypid;
	# there should be no matching directory here
	# unless we are getting pid recycling. Start fresh.
	$res = system("rm -rf $mypiddir");
	if($res != 0) {
		print "SaveMeSetup: Could not remove prior pid directory in savemedir \n";
		return(0);
	}
	$res = system("mkdir $mypiddir");
	if($res != 0) {
		print "SaveMeSetup: Could not create pid directory in \"saveme\" directory\n";
		return(0);
	}
	# make a symbolic link for personal condor module to use
	# if we get pid recycling, blow the symbolic link 
	# This might not be a symbolic link, so use -r to be sure
	#$res = verbose_system("rm -fr $mypid");
	#if($res != 0) {
		#print "SaveMeSetup: Could not remove prior pid directory\n";
		#return(0);
	#}
	#$res = verbose_system("ln -s $mypiddir $mypid");
	#if($res != 0) {
		#print "SaveMeSetup: Could not link to pid dir in  \"saveme\" directory\n";
		#return(0);
	#}
	return($mypid);
}

sub PersonalSystem 
{
	my $args = shift @_;
	my $dumpLogs = $ENV{DUMP_CONDOR_LOGS};
	my $mypid = $$;
	$mypid = "pdir$mypid";
	
	if(defined $dumpLogs) {
		print "Dump Condor Logs if things go south\n";
		print "Pid dir is  $mypid\n";
		system("pwd");
	}

	my $hashref = system($args);

	my $rc = ${$hashref}{exitcode};

	if(defined $dumpLogs) {
		print "Dumping Condor Logs\n";
		my $savedir = getcwd();
		chdir("$mypid");
		system("ls");
		PersonalDumpLogs($mypid);
		chdir("$savedir");
		system("pwd");
	}

	return $rc;
}

sub PersonalDumpLogs
{
	my $piddir = shift;
	local *PD;
	#print "PersonalDumpLogs for $piddir\n";
	#system("pwd");
	#system("ls -la");
	opendir PD, "." || die "failed to open . : $!\n";
	#print "Open worked.... listing follows.....\n";
	foreach my $file (readdir PD)
	{
		#print "Consider: $file\n";
		next if $file =~ /^\.\.?$/; # skip . and ..
		if(-f $file ) {
			#print "F:$file\n";
		} elsif( -d $file ) {
			#print "D:$file\n";
			my $logdir = $file . "/log";
			PersonalDumpCondorLogs($logdir);
		}
	}
	close(PD);
}

sub PersonalDumpCondorLogs
{
	my $logdir = shift;
	local *LD;
	#print "PersonalDumpLogs for $logdir\n";
	my $now = getcwd();
	chdir("$logdir");
	#system("pwd");
	#system("ls -la");
	print "\n\n******************* DUMP $logdir ******************\n\n";
	opendir LD, "." || die "failed to open . : $!\n";
	#print "Open worked.... listing follows.....\n";
	foreach my $file (readdir LD)
	{
		#print "Consider: $file\n";
		next if $file =~ /^\.\.?$/; # skip . and ..
		if(-f $file ) {
			print "\n\n******************* DUMP $file ******************\n\n";
			open(FF,"<$file") || die "Can not open logfile: $file: $!\n";
			while(<FF>){
				print "$_";
			}
			close(FF);
		} elsif( -d $file ) {
			#print "D:$file\n";
		}
	}
	close(LD);
	chdir("$now");
}

sub DisplayPartialLocalConfig
{
	my $configloc = shift;
	my $logdir = `condor_config_val log`;
	$_ = $logdir;
	s/\\/\//g;
	$logdir = $_;
	my $fullpathtolocalconfig = "";
	my $line = "";
	fullchomp($logdir);
	if($logdir =~ /(.*\/)log/) {
		#print "Config File Location <$1>\n";
		$fullpathtolocalconfig = $1 . $personal_local;
		print "\nlocal config file: $fullpathtolocalconfig\n";
		if( -f $fullpathtolocalconfig) {
			print "\nDumping Adjustments to: $personal_local\n\n";
			my $startdumping = 0;
			open(LC,"<$fullpathtolocalconfig") or die "Can not open $fullpathtolocalconfig: $!\n";
			while(<LC>) {
				fullchomp($_);
				$line = $_;
				if($line =~ /# Requested.*/) {
					print "$line\n";
				} elsif($line =~ /# Adding.*/) {
					if($startdumping == 0) {
						$startdumping = 1;
					}
					print "$line\n";
				} else {
					if($startdumping == 1) {
						print "$line\n";
					}
				}
			}
			close(LC);
			print "\nDONE Dumping Adjustments to: $personal_local\n\n";
		}
	}
}

sub IsThisNightly
{
	my $mylocation = shift;

	debug("IsThisNightly passed: $mylocation\n",$debuglevel);
	if($mylocation =~ /^.*(\/execute\/).*$/) {
		return(1);
	} else {
		return(0);
	}
}

sub CheckNamedFWTimed
{
	foreach my $key (sort keys %framework_timers) {
	}
}

sub RegisterFWTimed 
{
	my $name = shift || croak "Missing fw callback name\n";
	my $timedcallback = shift || croak "missing callback argument\n";
	my $timeddelta = shift || croak "missing delta argument\n";
	$framework_timers{$name}{name} = $name;
	$framework_timers{$name}{timer_time} = time;
	$framework_timers{$name}{timedcallback} = $timedcallback;
	$framework_timers{$name}{timeddelta} = $timeddelta;
}

sub RemoveFWTimed
{
	my $namedcallback = shift || croak "Missing name of named callback to delete\n";
	delete $framework_timers{$namedcallback};
}

my $minimalistConfig = "

";

my $WinminimalistConfigextra = "
";



sub DoInitialConfigCheck
{
	if(exists $ENV{CONDOR_CONFIG}) {
		my $config = $ENV{CONDOR_CONFIG};
		if( -f "$config") {
			#print "Our initial main config file:$config\n";
			return(0);
		} else {
			print "CONDOR_CONFIG defined but missing:$config\n";
			return(1);
		}
	} else {
		print "CONDOR_CONFIG not set\n";
		return(1);
	}
}
1;
