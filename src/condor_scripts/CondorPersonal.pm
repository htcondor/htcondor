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
# 1-6-05 Bill Taylor
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
use Cwd;
use POSIX qw/sys_wait_h strftime/;
use Socket;
use Sys::Hostname;

use CondorUtils;
use CondorTest;

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
#   Notes added 12/14/05 bt
#
#   The current uses of this module only support condor = [ nightlies | install ]
#	The difference is that we "look" for the binaries using condor_config_val -config
#	if "install" is specified and we assume the "testing" environment for binary locations
#	if "nightlies" is called out. In either case we utilize the current condor configuration
#	file and the current local config file to ensure working with an up to date
#	base configuration file. These we copy into the new directory we are building
#	for the personal condor. We then "tune" them up....
#
#	currently "universe" is only used to add a bit to the config file for the parallel
#	universe.
#
#   So a base parameter file could be as little as: (I think)
#					condor = install
#					ports = dynamic
#					condordaemon = master, startd, schedd
#
#	Current uses to look at are *stork*.run tests, *_par.run tests and *condorc*.run tests.
#	This module is in a very early stage supporting current testing.
#

#	Notes from efforts 10/10/2007
#	This module continues to grow and complicated tests combining multiple
#	pools continue to be added for testing of anything to flocking, had, security modes
#	and a run through of all 16 protocol negotions. We have just finshed being much
#	more strict about when we decide the pool we are creating is actually up. This has created
#	more consistent results. The daemons(main 5 -Col, Neg, Mas, Strt, Schd) all are configured
#	with address files and we wait until those files exist if that daemon is configured.
#	We will adjust for daemons controlled by HAD. We also wait for the collector(if we have one)
#	to see the schedd or the startd if those daemons are configured. HOWEVER, I am adding a 
# 	bypass for this for the 16 variations of the authentication protocol negotiations.
#	If "daemonwait" is set to false, we will only wait for the address files to exist and not
#	require inter-daemon communication.

#	Notes from efforts 11/24/2008
#	The recent adding of tracking of core files and ERROR statements in daemon
#	logs shows (since the negotiation tests never shut down generating a ongoing
# 	stream of ERROR statements making other good tests fail) that we need a better
#	and surer KILL for personal condors. if condor_off -master fails then we 
# 	leave loos personal condors around which could cause other tests to fail.
#	We are going to collect the PIDs of the daemons for a personal condor just
#	after they start and before they have a chance to rotate and write a file PIDS
#	in the log directory we will kill later.

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


my $topleveldir = getcwd();
my $btdebug = 0;
my $home = $topleveldir;
my $localdir;
my $condorlocaldir;
my $pid = $$;
my $version = ""; # remote, middle, ....... for naming schedd "schedd . pid . version"
my $mastername = ""; # master_$verison
my $DEBUGLEVEL = 3; # nothing higher shows up
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
#
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
	my $config_and_port = "";
	my $winpath = "";

	if(!(-f $paramfile)) {
		die "StartCondor: param file $paramfile does not exist!!\n";
	}

	if($arraysz == 3) {
		$mpid = $pid; # assign process id
	} else {
		$mpid = shift; # assign process id
	}

	CondorPersonal::ParsePersonalCondorParams($paramfile);

	# Insert the positional arguments into the new-style named-argument
	# hash and call the version of this function which handles it.
	$personal_condor_params{"test_name"} = $testname;
	$personal_condor_params{"condor_name"} = $version;
	$personal_condor_params{"owner_pid"} = $mpid;
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
	if($btdebug == 1) {
		print "StartCondorWithParams: Hash <personal_condor_params > holds:\n";
		foreach my $key (sort keys %personal_condor_params) {
			print "StartCondorWithParams: $key $personal_condor_params{$key}\n";
		}
	}

	my $testname = $personal_condor_params{"test_name"} || die "Missing test_name\n";
	$version = $personal_condor_params{"condor_name"} || die "Missing condor_name!\n";
	my $mpid = $personal_condor_params{"owner_pid"} || $pid;
	my $config_and_port = "";
	my $winpath = "";

	if($btdebug == 1) {
		print "######################### StartCondorWithParams: toplevedir: $topleveldir ###########################\n";
	}
        $topleveldir = "$topleveldir/$testname.saveme/$mpid/$mpid$version";
	system("mkdir -p $topleveldir/$testname.saveme/$mpid/$mpid$version");

	$procdaddress = $mpid . $version;


	if(exists $personal_condor_params{"personaldir"}) {
		$topleveldir = $personal_condor_params{"personaldir"};
		debug( "SETTING $topleveldir as topleveldir\n",$debuglevel);
		system("mkdir -p $topleveldir");
	}

	# if we are wrapping tests, publish log location
	$wrap_test = $ENV{WRAP_TESTS};
	if(defined  $wrap_test) {
		my $logdir = $topleveldir . "/log";
		#CondorPubLogdirs::PublishLogDir($testname,$logdir);
	}

	$personal_config_file = $topleveldir ."/condor_config";
	#print " ****** StartCondorWithParams: our new config file is <$personal_config_file>\n";

	# what if we want to change what goes on here? Like really bare bones config
	# file for checking internal param table defaults and values.
	#print "StartCondorWithParams: Hash <personal_condor_params > <pre-InstallPersonalCondor>holds:\n";
	#foreach my $key (sort keys %personal_condor_params) {
		#print "StartCondorWithParams: $key $personal_condor_params{$key}\n";
	#}

	$localdir = CondorPersonal::InstallPersonalCondor();

	#print "StartCondorWithParams: Hash <personal_condor_params > <post-InstallPersonalCondor>holds:\n";
	#foreach my $key (sort keys %personal_condor_params) {
		#print "StartCondorWithParams: $key $personal_condor_params{$key}\n";
	#}

	if($localdir eq "")
	{
		return("Failed to do needed Condor Install\n");
	}

	if( CondorUtils::is_windows() == 1 ){
		$winpath = `cygpath -m $localdir`;
		CondorUtils::fullchomp($winpath);
		$condorlocaldir = $winpath;
		CondorPersonal::TunePersonalCondor($condorlocaldir, $mpid);
	} else {
		CondorPersonal::TunePersonalCondor($localdir, $mpid);
	}

	#print "StartCondorWithParams: Hash <personal_condor_params > <post-TunePersonalCondor>holds:\n";
	#foreach my $key (sort keys %personal_condor_params) {
		#print "StartCondorWithParams: $key $personal_condor_params{$key}\n";
	#}

	$collector_port = CondorPersonal::StartPersonalCondor();
	#print "StartCondorWithParams: Hash <personal_condor_params > <post-StartPersonalCondor>holds:\n";
	#foreach my $key (sort keys %personal_condor_params) {
		#print "StartCondorWithParams: $key $personal_condor_params{$key}\n";
	#}


	debug( "collector port is $collector_port\n",$debuglevel);

	if( CondorUtils::is_windows() == 1 ){
		$winpath = `cygpath -m $personal_config_file`;
		CondorUtils::fullchomp($winpath);
		if($btdebug == 1) {
			print "Windows conversion of personal config file to $winpath!!\n";
		}
		$config_and_port = $winpath . "+" . $collector_port ;
	} else {
		$config_and_port = $personal_config_file . "+" . $collector_port ;
	}

	debug( "StartCondor config_and_port is --$config_and_port--\n",$debuglevel);
	CondorPersonal::Reset();
	debug( "StartCondor config_and_port is --$config_and_port--\n",$debuglevel);
	debug( "Personal Condor Started\n",$debuglevel);
	system("date");
	if($btdebug == 1) {
		print  "Personal Condor Started\n";
		print scalar localtime() . "\n";
	}
	return( $config_and_port );
}

sub debug {
    my $string = shift;
    my $level = shift;
	my $time = `date`;
	fullchomp($time);
	push @debugcollection, "$time: CondorPersonal - $string";
        if(!(defined $level) or ($level <= $DEBUGLEVEL)) {
			if(defined $level) {
            	print( "", timestamp(), ": CondorPersonal(L=$level) $string" );
			} else {
            	print( "", timestamp(), ": CondorPersonal(L=?) $string" );
			}
        }
}

sub debug_flush {
	print "\nDEBUG_FLUSH:\n";
	my $logdir = `condor_config_val log`;
	$_ = $logdir;
	s/\\/\//g;
	$logdir = $_;
	fullchomp($logdir);
	print "\nLog directory: $logdir and contains:\n";
	system("ls -lh $logdir");
	print "\ncondor_who -verb says:\n";
	system("condor_who -verb");
	print "\nDebug collection starts now:\n";
	foreach my $line (@debugcollection) {
		print "$line\n";
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
# 	Parses parameter file in typical condor form of NAME = VALUE
#		and stores results into a hash for lookup later.
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
	#foreach my $key (%personal_condor_params) {
		#print "ParsePersonalCondorParams: param: $key , $personal_condor_params{$key}\n";
	#}
    return 1;
}

#################################################################
#
# WhichCondorConfig
#
# 	Analysis to decide if we are currently in the environment of this same personal
#		Condor. Not very likely anymore as the config files are all dependent on
#		the pid of the requesting shell so every request for a personal condor
#		should be unique.
#

sub WhichCondorConfig
{
	my $pathtoconfig = shift @_;
	my $line = "";
	my $badness = "";
	my $matchedconfig = "";

	open(CONFIG, "condor_config_val -config -master 2>&1 |") || die "condor_config_val: $!\n";
	while(<CONFIG>)
	{
		CondorUtils::fullchomp($_);
		$line = $_;
		debug ("--$line--\n",$debuglevel);

		if( $line =~ /^\s*($pathtoconfig)\s*$/ )
		{
			$matchedconfig = $1;
			debug ("Matched! $1\n",$debuglevel);
		}

		if( $line =~ /(Can't find address for this master)/ )
		{
			$badness = $1;
			debug( "Not currently running! $1\n",$debuglevel);
		}
	}
	close(CONFIG);
	
	# we want matched running or matched not running or lost returned

	if( $matchedconfig eq "" )
	{
		return("lost");
	} 
	else
	{
		if( $badness eq "" )
		{
			return("matched running");
		}
		else
		{
			return("matched not running");
		}
	}
}

##################################################################
#
# Run condor_config_val using the specified configuration file.
#

sub CondorConfigVal
{
    my $config_file = shift;
    my $param_name = shift;

    my $oldconfig = $ENV{CONDOR_CONFIG};
    $ENV{CONDOR_CONFIG} = $config_file;

    my $result = `condor_config_val $param_name`;

    fullchomp $result;
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

	my $schedd;
	my $master;
	my $collector;
	my $submit;
	my $startd;
	my $negotiator;
	my $condorq = Which("condor_q");
	my $sbinloc;
	my $configline = "";
	my @configfiles;
	my $condordistribution;

	my $binloc;

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
		# where is the hosting condor_config file? The one assumed to be based
		# on a setup with condor_configure.
		open(CONFIG,"condor_config_val -config | ") || die "Can not find config file: $!\n";
		while(<CONFIG>)
		{
			CondorUtils::fullchomp($_);
			$configline = $_;
			push @configfiles, $configline;
		}
		close(CONFIG);
		$personal_condor_params{"condortemplate"} = shift @configfiles;

		#print " ****** Condortemplate set to <$personal_condor_params{condortemplate}>\n";

		if(exists $personal_condor_params{fresh_local}) {
		} else {
			# Always start with a freshly constructed local config file
			# so we know what we get  bt 5/13
			#$personal_condor_params{"condorlocalsrc"} = shift @configfiles;
		}

		debug("condor_q: $condorq\n",$debuglevel);
        debug("topleveldir: $topleveldir",$debuglevel);

		if( $condorq =~ /^(\/.*\/)(\w+)\s*$/ ) {
			debug( "Root path $1 and base $2\n",$debuglevel);
			$binloc = $1;	# we'll get our binaries here.
		} elsif(-f "../release_dir/bin/condor_status") {
			print "Bummer which condor_q failed\n";
			print "Using ../release_dir/bin(s)\n";
			$binloc = "../release_dir/bin"; # we'll get our binaries here.
		}
		else
		{
			print "which condor_q responded: $condorq! CondorPersonal Failing now\n";
			debug_flush();
			die "Can not seem to find a Condor install!\n";
		}
		

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

		$schedd = $sbinloc . "sbin/". "condor_schedd";
		$master = $sbinloc . "sbin/". "condor_master";
		$collector = $sbinloc . "sbin/". "condor_collector";
		$submit = $binloc . "condor_submit";
		$startd = $sbinloc . "sbin/". "condor_startd";
		$negotiator = $sbinloc . "sbin/". "condor_negotiator";

		debug( "$schedd $master $collector $submit $startd $negotiator\n",$debuglevel);


		debug( "Sandbox started rooted here: $topleveldir\n",$debuglevel);

		system("cd $topleveldir && mkdir -p execute spool log log/tmp");
	} elsif( $condordistribution eq "nightlies" ) {
		# we want a mechanism by which to find the condor binaries
		# we are testing. But we know where they are relative to us
		# ../../condor/bin etc
		# That is simply the nightly test setup.... for now at least
		# where is the hosting condor_config file? The one assumed to be based
		# on a setup with condor_configure.

		debug(" Nightlies - find environment config files\n",$debuglevel);
		open(CONFIG,"condor_config_val -config | ") || die "Can not find config file: $!\n";
		while(<CONFIG>)
		{
			CondorUtils::fullchomp($_);
			$configline = $_;
			debug( "$_\n" ,$debuglevel);
			push @configfiles, $configline;
		}
		close(CONFIG);
		# yes this assumes we don't have multiple config files!
		$personal_condor_params{"condortemplate"} = shift @configfiles;
		$personal_condor_params{"condorlocalsrc"} = shift @configfiles;

		#print " ****** Case nightlies leading to <$personal_condor_params{condortemplate}> and $personal_condor_params{condorlocalsrc}\n";
		debug( "My path to condor_q is $condorq and topleveldir is $topleveldir\n",$debuglevel);

		if( $condorq =~ /^(\/.*\/)(\w+)\s*$/ )
		{
			debug( "Root path $1 and base $2\n",$debuglevel);
			$binloc = $1;	# we'll get our binaries here.
		}
		else
		{
			print "which condor_q responded: $condorq! CondorPersonal Failing now\n";
			debug_flush();
			die "Can not seem to find a Condor install!\n";
		}
		

		if( $binloc =~ /^(\/.*\/)bin\/\s*$/ )
		{
			debug( "Root path to sbin is $1\n",$debuglevel);
			$sbinloc = $1;	# we'll get our binaries here. # local_dir is here
		}
		else
		{
			debug_flush();
			die "Can not seem to locate Condor release binaries\n";
		}
		#$binloc = "../../condor/bin/";
		#$sbinloc = "../../condor/";

		debug( "My path to condor_q is $binloc and topleveldir is $topleveldir\n",$debuglevel);

		$schedd = $sbinloc . "sbin/" .  "condor_schedd";
		$master = $sbinloc . "sbin/" .  "condor_master";
		$collector = $sbinloc . "sbin/" .  "condor_collector";
		$submit = $binloc . "condor_submit";
		$startd = $sbinloc . "sbin/" .  "condor_startd";
		$negotiator = $sbinloc . "sbin/" .  "condor_negotiator";

		debug( "$schedd $master $collector $submit $startd $negotiator\n",$debuglevel);


		debug( "Sandbox started rooted here: $topleveldir\n",$debuglevel);

		system("cd $topleveldir && mkdir -p execute spool log log/tmp");
	} elsif( -e $condordistribution ) {
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
	my $mpid = "";
	my $localdir = shift;
	my $scheddname;
	my $startdname;
	my $minimalconfig = 0;

	if(scalar( @_ ) == 1) {
		$mpid = $pid; # assign process id
	} else {
		$mpid = shift; # assign process id
	}

	#print " ****** TunePersonalCondor with localdir set to <$localdir>\n";

	debug( "TunePersonalCondor setting LOCAL_DIR to $localdir\n",$debuglevel);
	#print "domain parts follow:";
	#foreach my $part (@domainparts)
	#{
		#print " $part";
	#}
	#print "\n";

	#$myhost = @domainparts[0];

	#foreach my $key (sort keys %control) {
		#print "TunePersonal:\n";
		#print "CONTROL: $key, $control{$key}\n";
	#}

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


	 debug( "Proto file is --$personal_template--\n",4);

	$personalmaster = "$topleveldir/sbin/condor_master";

	#filter fig file storing entries we set so we can test
	#for completeness when we are done
	my $mytoppath = "";
	if( CondorUtils::is_windows() == 1 ){
		$mytoppath = `cygpath -m $topleveldir`;
		CondorUtils::fullchomp($mytoppath);
	} else {
		$mytoppath =  $topleveldir;
	}


debug( "HMMMMMMMMMMM personal local is $personal_local , mytoppath is $mytoppath",$debuglevel);
if($btdebug == 1) {
	print "HMMMMMMMMMMM personal local is $personal_local , mytoppath is $mytoppath";
}

	my $line;
	#system("ls;pwd");
	#print "***************** opening $personal_template as config file template *****************\n";
	#print " ****** writing template to <$topleveldir/$personal_config> \n";
	if(exists $control{config_minimal}) {
		print " ****** writing template have request for config_minimal!!!!!\n";
		$minimalconfig += 1;
	}

	open(TEMPLATE,"<$personal_template")  || die "Can not open template: $personal_template: $!\n";
	debug( "want to open new config file as $topleveldir/$personal_config\n",$debuglevel);
	open(NEW,">$topleveldir/$personal_config") || die "Can not open new config file: $topleveldir/$personal_config: $!\n";
	print NEW "# Editing requested config: $personal_template\n";
	if($minimalconfig == 1) {
		# Have almost no config so w can examine internal value for knobs
		print NEW "DAEMON_LIST = MASTER, SCHEDD, STARTD, COLLECTOR, NEGOTIATOR\n";
		$personal_config_changes{"RELEASE_DIR"} = "RELEASE_DIR = $localdir\n";
		print NEW "RELEASE_DIR = $localdir\n";
		$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $mytoppath\n";
		print NEW "LOCAL_DIR = $mytoppath\n";
		$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $mytoppath\n";
		print NEW "LOCAL_CONFIG_FILE = $mytoppath/$personal_local\n";
		# set up some address files
		print NEW "COLLECTOR_HOST = \$(CONDOR_HOST):0\n";
    	print NEW "COLLECTOR_ADDRESS_FILE = \$(LOG)/.collector_address\n";
    	print NEW "NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address\n";
    	print NEW "MASTER_ADDRESS_FILE = \$(LOG)/.master_address\n";
    	print NEW "STARTD_ADDRESS_FILE = \$(LOG)/.startd_address\n";
    	print NEW "SCHEDD_ADDRESS_FILE = \$(LOG)/.schedd_address\n";
		# binaries
		print NEW "MASTER = \$(SBIN)/condor_master\n";
		print NEW "STARTD = \$(SBIN)/condor_startd\n";
		print NEW "SCHEDD = \$(SBIN)/condor_schedd\n";
		print NEW "KBDD = \$(SBIN)/condor_kbdd\n";
		print NEW "NEGOTIATOR = \$(SBIN)/condor_negotiator\n";
		print NEW "COLLECTOR = \$(SBIN)/condor_collector\n";
		print NEW "CKPT_SERVER = \$(SBIN)/condor_ckpt_server\n";
		# starter stuff
		print NEW "STARTER_LOCAL = \$(SBIN)/condor_starter\n";
		print NEW "STARTER = \$(SBIN)/condor_starter\n";
		print NEW "STARTER_LIST = STARTER\n";
		# misc
		print NEW "CONTINUE = TRUE\n";
		print NEW "PREEMPT =  FALSE\n";
		print NEW "SUSPEND =  FALSE\n";
		print NEW "WANT_SUSPEND =  FALSE\n";
		print NEW "WANT_VACATE =  FALSE\n";
		print NEW "START =  TRUE\n";
		print NEW "KILL =  FALSE\n";
	} else {
		# geneate basic config as variation on TestingPersonalCondor config
		while(<TEMPLATE>)
		{
			CondorUtils::fullchomp($_);
			$line = $_;
			if( $line =~ /^RELEASE_DIR\s*=.*/ )
			{
				 debug( "-----------$line-----------\n",4);
				$personal_config_changes{"RELEASE_DIR"} = "RELEASE_DIR = $localdir\n";
				print NEW "RELEASE_DIR = $localdir\n";
			}
			elsif( $line =~ /^LOCAL_DIR\s*=.*/ )
			{
				 debug( "-----------$line-----------\n",4);
				$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $mytoppath\n";
				print NEW "LOCAL_DIR = $mytoppath\n";
			}
			elsif( $line =~ /^LOCAL_CONFIG_FILE\s*=.*/ )
			{
				 debug( "-----------$line-----------\n",4);
				$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $mytoppath\n";
				print NEW "LOCAL_CONFIG_FILE = $mytoppath/$personal_local\n";
			}
			else
			{
				print NEW "$line\n";
			}
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

	debug( "HMMMMMMMMMMM opening to write: $topleveldir/$personal_local\n",$debuglevel);

		# Dan: Jan 30, '08 added D_NETWORK in order to debug condor_rm timeout
		# print NEW "ALL_DEBUG = D_FULLDEBUG\n";
    	print NEW "DEFAULT_DEBUG = D_FULLDEBUG\n";
		# bill: 8/13/09 speed up dagman
		print NEW "DAGMAN_USER_LOG_SCAN_INTERVAL = 1\n";
		# bill make tools more forgiving of being busy
		print NEW "TOOL_TIMEOUT_MULTIPLIER = 10\n";
		print NEW "TOOL_DEBUG_ON_ERROR = D_ANY D_ALWAYS:2\n";

		if($personal_daemons ne "")
		{
			# Allow the collector to run on the default and expected port as the main
			# condor install on this system.
			print NEW "# Adding requested daemons\n";
			print NEW "DAEMON_LIST = $personal_daemons\n";
		}

		if($personal_universe eq "parallel")
		{
			# set up dedicated scheduler
			print NEW "# Adding Dedicated Scheduler $personal_universe Universe\n";
			print NEW "DedicatedScheduler = \"DedicatedScheduler\@schedd$mpid$version\@$condorhost\"\n";
			print NEW "STARTD_EXPRS = \$(STARTD_EXPRS), DedicatedScheduler\n";
			print NEW "SCHEDD_DEBUG = D_FULLDEBUG\n";
		}

		if( $portchanges eq "dynamic")
		{
			# this variation requests a dynamic port for collector and negotiator
			# and the location where we can look up the adresses.
			print NEW "# Adding for portchanges equal dynamic\n";
			if( $collectorhost )
			{
				print NEW "COLLECTOR_HOST = $collectorhost\n";
				debug("COLLECTOR_HOST = $collectorhost\n",$debuglevel);
			}
			else
			{
				print NEW "COLLECTOR_HOST = \$(CONDOR_HOST):0\n";
				debug("COLLECTOR_HOST = \$(CONDOR_HOST):0\n",$debuglevel);
			}

			# For simulated pools, we need schedds and master to have unique names
			if(exists $control{"nameschedd"}) {
				$mastername = "master" . "_" . $version;
				$scheddname = $mastername . "_schd";
				$startdname = $mastername . "_strtd";
				debug("MASTERNAME now master + $version($mastername)\n",$debuglevel);
				print NEW "MASTER_NAME = $mastername\n";
				print NEW "SCHEDD_NAME = $scheddname\n";
				print NEW "STARTD_NAME = $startdname\n";
			} else {
				print NEW "SCHEDD_NAME = schedd$mpid$version\n";
			}


			print NEW "MASTER_ADDRESS_FILE = \$(LOG)/.master_address\n";
			print NEW "COLLECTOR_ADDRESS_FILE = \$(LOG)/.collector_address\n";
			print NEW "NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address\n";

			print NEW "CONDOR_HOST = $condorhost\n";
			
			print NEW "START = TRUE\n";
			print NEW "SUSPEND = FALSE\n";
			print NEW "UPDATE_COLLECTOR_WITH_TCP = FALSE\n";
			print NEW "RUNBENCHMARKS = FALSE\n";
			print NEW "JAVA_BENCHMARK_TIME = 0\n";
			print NEW "SCHEDD_INTERVAL = 5\n";
			print NEW "UPDATE_INTERVAL = 5\n";
			print NEW "NEGOTIATOR_INTERVAL = 5\n";
			print NEW "CONDOR_JOB_POLL_INTERVAL = 5\n";
			print NEW "PERIODIC_EXPR_TIMESLICE = .99\n";
			print NEW "JOB_START_DELAY = 0\n";
			print NEW "# Done Adding for portchanges equal dynamic\n";
		}
		elsif( $portchanges eq "standard" )
		{
			# Allow the collector to run on the default and expected port as the main
			# condor install on this system.
			print NEW "# Adding for portchanges equal standard\n";
			if( $collectorhost )
			{
				print NEW "COLLECTOR_HOST = $collectorhost\n";
				debug("COLLECTOR_HOST is $collectorhost\n",$debuglevel);
			}
			else
			{
				print NEW "COLLECTOR_HOST = \$(CONDOR_HOST)\n";
				debug("COLLECTOR_HOST is \$(CONDOR_HOST)\n",$debuglevel);
			}

			print NEW "CONDOR_HOST = $condorhost\n";
			print NEW "START = TRUE\n";
			print NEW "SUSPEND = FALSE\n";
			print NEW "UPDATE_COLLECTOR_WITH_TCP = FALSE\n";
			print NEW "SCHEDD_INTERVAL = 5\n";
			print NEW "UPDATE_INTERVAL = 5\n";
			print NEW "NEGOTIATOR_INTERVAL = 5\n";
			print NEW "CONDOR_JOB_POLL_INTERVAL = 5\n";
			print NEW "RUNBENCHMARKS = false\n";
			print NEW "JAVA_BENCHMARK_TIME = 0\n";
			print NEW "# Done Adding for portchanges equal standard\n";
		}
		else
		{		
			debug_flush();
			die "Misdirected request for ports\n";
			exit(1);
		}

		#print NEW "PROCD_LOG = \$(LOG)/ProcLog\n";
		if( CondorUtils::is_windows() == 1 ){
			print NEW "# Adding procd pipe for windows\n";
			print NEW "PROCD_ADDRESS = \\\\.\\pipe\\$procdaddress\n";
		}

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

	debug( "HMMMMMMMMMMM opening to read: $personal_local_src\n",$debuglevel);

			open(LOCSRC,"<$personal_local_src") || die "Can not open local config template: $!\n";
			while(<LOCSRC>)
			{
				CondorUtils::fullchomp($_);
				$line = $_;
				print NEW "$line\n";
			}
			# now make sure we have the local dir we want after the generic .local file is seeded in
			$line = $personal_config_changes{"LOCAL_DIR"};
			print NEW "$line\n";
			# and a lock directory we like
			print NEW "LOCK = \$(LOG)\n";
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

		# Gittrac Ticket 2889
		# This is ok when tests are running as a slot user but if/when we start running tests as root we will
		# need to put these into a directory with permissions 1777.
		print NEW "# Relocate C_GAHP files to prevent collision from multiple tests (running on multiple slots) writing into /tmp\n";
		print NEW "# If we are running tests as root we might need to relocate these to a directory with permissions 1777\n";
		print NEW "C_GAHP_LOG = \$(LOG)/CGAHPLog.\$(USERNAME)\n";
		print NEW "C_GAHP_LOCK = \$(LOCK)/CGAHPLock.\$(USERNAME)\n";
		print NEW "C_GAHP_WORKER_THREAD_LOG = \$(LOG)/CGAHPWorkerLog.\$(USERNAME)\n";
		print NEW "C_GAHP_WORKER_THREAD_LOCK = \$(LOCK)/CGAHPWorkerLock.\$(USERNAME)\n";
	} else {
		print NEW "# minimal local config file\n";
		print NEW "CONDOR_HOST = $condorhost\n";
		print NEW "COLLECTOR_HOST = \$(CONDOR_HOST):0\n";
	}

	close(NEW);

	PostTunePersonalCondor($personal_config_file);
}


#################################################################
#
#   PostTunePersonalCondor() is called after TunePersonalCondor.
#   It assumes that the configuration file is all set up and
#   ready to use.

sub PostTunePersonalCondor
{
    my $config_file = shift;

    # If this is a quill test, then quill is within
    # $personal_daemons AND $topleveldir/../pgpass wants to  be
    # $topleveldir/spool/.pgpass

    my $configured_daemon_list = CondorConfigVal($config_file,"daemon_list");
    if($configured_daemon_list =~ m/quill/i ) {
        debug( "This is a quill test (because DAEMON_LIST=$configured_daemon_list)\n", $debuglevel );
        my $cmd = "cp $topleveldir/../pgpass $topleveldir/spool/.pgpass";
        system("$cmd");
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
	
	if($btdebug == 1) {
		print "Entered StartPersonalCondor\n";
	}
	# If we start a personal Condor as root (for testing the VM universe),
	# we need to change the permissions/ownership on the directories we
	# made so that the master (which runs as condor) can use them.
	if( $> == 0 ) {
		my $testName = $control{ 'test_name' };
		system( "chown condor.condor $home/${testName}.saveme >& /dev/null" );
		system( "chown -R condor.condor $home/${testName}.saveme/$pid >& /dev/null" );
	}

	my $configfile = $control{"condorconfig"};
	my $fullconfig = "$topleveldir/$configfile";
	my $oldpath = $ENV{PATH};
	my $newpath = $localdir . "sbin:" . $localdir . "bin:" . "$oldpath";
	my $figpath = "";
	$ENV{PATH} = $newpath;

	debug( "Using this path: --$newpath--\n",$debuglevel);

	debug( "Want $configfile for config file\n",$debuglevel);

	if( CondorUtils::is_windows() == 1 ){
		$figpath = `cygpath -m $fullconfig`;
		CondorUtils::fullchomp($figpath);
		$fullconfig = $figpath;
		# note: on windows all binaaries in bin!
		$personalmaster = $localdir . "bin/condor_master -f &";
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
	$ENV{CONDOR_CONFIG} = $fullconfig;
	debug( "Is personal condor running config: $fullconfig\n",$debuglevel);
	my $condorstate = IsPersonalRunning($fullconfig);
	debug( "Condor state is $condorstate\n",$debuglevel);
	my $fig = $ENV{CONDOR_CONFIG};
	debug( "Condor_config from environment is --$fig--\n",$debuglevel);

	# At the momment we only restart/start a personal we just configured 
	# or reconfigured

	if( $condorstate == 0 ) {
		#  not running with this config so treat it like a start case
		debug("Condor state is off\n",$debuglevel);
		debug( "start up the personal condor!--$personalmaster--\n",$debuglevel);
		# when open3 is used it sits and waits forever
		system("$personalmaster");
		#system("condor_config_val -v log");
	} else {
		debug_flush();
		die "Bad state for a new personal condor configuration! running :-(\n";
	}

	my $res = IsRunningYet();
	system("date");
	if($btdebug == 1) {
		print "Back from IsRunningYet in start personal condor\n";
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
		if($btdebug == 1) {
			print "left StartPersonalCondor\n";
		}
		return( FindCollectorPort() );
	}
	else
	{
		debug("NOT Looking for collector port!\n",$debuglevel);
		if($btdebug == 1) {
			print "left StartPersonalCondor\n";
		}
		return("0");
	}
}

#################################################################
#
# IsPersonalRunning( configpath )
#
# the one above "WhichConfig" is simply bogus and relies
#	on a side effect of the wrong request condor_config_val -config -master
#	without adding a parameter to look up. What we really want to know
#	is if this personal is running(probably not...)
################################################################


sub IsPersonalRunning
{
    my $pathtoconfig = shift @_;
    my $line = "";
    my $badness = "";
    my $matchedconfig = "";

    CondorUtils::fullchomp($pathtoconfig);
    #if(CondorUtils::is_windows() == 1) {
        #$pathtoconfig =~ s/\\/\\\\/g;
    #}

	debug("call - condor_config_val -config -master log \n",$debuglevel);
    open(CONFIG, "condor_config_val -config -master log 2>&1 |") || die "condor_config_val: $
!\n";
	debug("parse - condor_config_val -config -master log \n",$debuglevel);
    while(<CONFIG>) {
        CondorUtils::fullchomp($_);
        $line = $_;
        debug ("--$line--\n",$debuglevel);


        debug("Looking to match \"$pathtoconfig\"\n",$debuglevel);
		my $indexreturn = index($line,$pathtoconfig);
        #if( $line =~ /^.*($pathtoconfig).*$/ ) {
        if( $indexreturn  > -1 ) {
            $matchedconfig = $pathtoconfig;
            debug ("Matched! $pathtoconfig\n",$debuglevel);
            last;
        } else {
			debug("hmmmm looking for: $pathtoconfig got: $line \n",$debuglevel);
		}
    }
    if ( close(CONFIG) && ($? != 13) ) {       # Ignore SIGPIPE
        warn "Error executing condor_config_val: '$?' '$!'"
    }

    if( $matchedconfig eq "" ) {
		debug_flush();
        die "lost: config does not match expected config setting......\n";
    }

    # find the master file to see if it exists and threrfore is running

    open(MADDR,"condor_config_val MASTER_ADDRESS_FILE 2>&1 |") || die "condor_config_val: $
    !\n";
    while(<MADDR>) {
        CondorUtils::fullchomp($_);
        $line = $_;
        if($line =~ /^(.*master_address)$/) {
            if(-f $1) {
				if(exists $personal_condor_params{"personaldir"}) {
					# ignore if we want to fall to same place
					# and the previous might have been kill badly
					return(0);
				} else {
               		debug("Master running\n",$debuglevel);
                	return(1);
				}
            } else {
                debug("Master not running\n",$debuglevel);
                return(0);
            }
        }
    }
	close(MADDR);
}

#################################################################
#
# IsRunningYet
#
#	We want to do out best to be sure the personal is fully running
#	before going on to start a test against it. And this is also
#	a great time to harvest the PIDS of the daemons to allow a more
#	sure kill then condor_off can do in circumstances like
#	screwed up authentication tests
#
#################################################################

sub IsRunningYet {
    #print "Testing if Condor is up.\n";
	if($btdebug == 1) {
    	print "\tCONDOR_CONFIG=$ENV{CONDOR_CONFIG}\n";
	}
	my $daemonlist = `condor_config_val daemon_list`;
	CondorUtils::fullchomp($daemonlist);
	my $collector = 0;
	my $schedd = 0;
	my $startd = 0;
	my $first = 1;
	my @status;

	my $runlimit = 16;
	my $backoff = 2;
	my $loopcount;
	my @toolarray;
	my $toolres = 0;

	# first failure was had test where we looked for
	# a negotiator but MASTER_NEGOTIATOR_CONTROLLER
	# was set. So we will check for bypasses to normal 
	# operation and rewrite the daemon list

	#my $old_debuglevel = $debuglevel;
	#$debuglevel = $DEBUGLEVEL;
	debug("In IsRunningYet DAEMON_LIST=$daemonlist\n",$debuglevel);
	$daemonlist =~ s/\s*//g;
	my @daemons = split /,/, $daemonlist;
	$daemonlist = "";
	my $line = "";
	foreach my $daemon (@daemons) {
		$line = "";
		debug("Looking for MASTER_XXXXXX_CONTROLLER for $daemon\n",$debuglevel);
		my $definedcontrollstr = "condor_config_val MASTER_" . $daemon . "_CONTROLLER";
		#open(CCV, "$definedcontrollstr 2>&1 |") || die "condor_config_val: $!\n";
		$toolres = CondorTest::runCondorTool("$definedcontrollstr",\@toolarray,2,{expect_result=>\&ANY,emit_output=>0});
		#while(<CCV>) {
		foreach my $line (@toolarray) {
			fullchomp($line);
			if( $line =~ /^.*Not defined.*$/) {
				debug("Add $daemon to daemon list\n",$debuglevel);
				if($first == 1) {
					$first = 0;
					$daemonlist = $daemon;
				} else {
					$daemonlist = $daemonlist . "," . $daemon;
				}
			}
			debug("looking: $daemonlist\n",$debuglevel);
			#print "$daemonlist\n";
		}
		#close(CCV);
	}
	print "Final daemon list:$daemonlist\n";

	print "These Daemons are up: ";
	if($daemonlist =~ /MASTER/i) {
		#print "Has master dropped an address file yet - ";
		# now wait for the master to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $masteradr = `condor_config_val MASTER_ADDRESS_FILE`;
		$masteradr =~ s/\012+$//;
		$masteradr =~ s/\015+$//;
		debug( "MASTER_ADDRESS_FILE: $masteradr\n",$debuglevel);
    	debug( "We are waiting for the file to exist\n",$debuglevel);
    	# Where is the master address file? wait for it to exist
    	my $havemasteraddr = "no";
    	$loopcount = 0;
    	while($havemasteraddr ne "yes") {
        	$loopcount++;
        	debug( "Looking for $masteradr\n",$debuglevel);
        	if( -f $masteradr ) {
            	debug( "Found it!!!! master address file\n",$debuglevel);
            	$havemasteraddr = "yes";
        	} elsif ( $loopcount == $runlimit ) {
				debug_flush();
				debug( "Gave up waiting for master address file\n",$debuglevel);
				return 0;
        	} else {
            	sleep ($loopcount * $backoff);
        	}
    	}
		#print "ok\n";
		print "Master ";
	}

	if($daemonlist =~ /COLLECTOR/i){
		#print "Has collector dropped an address file yet - ";
		# now wait for the collector to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $collectoradr = `condor_config_val COLLECTOR_ADDRESS_FILE`;
		$collectoradr =~ s/\012+$//;
		$collectoradr =~ s/\015+$//;
		debug( "COLLECTOR_ADDRESS_FILE: $collectoradr\n",$debuglevel);
    	debug( "We are waiting for the file to exist\n",$debuglevel);
    	# Where is the collector address file? wait for it to exist
    	my $havecollectoraddr = "no";
    	$loopcount = 0;
    	while($havecollectoraddr ne "yes") {
        	$loopcount++;
        	debug( "Looking for $collectoradr\n",$debuglevel);
        	if( -f $collectoradr ) {
            	debug( "Found it!!!! collector address file\n",$debuglevel);
            	$havecollectoraddr = "yes";
        	} elsif ( $loopcount == $runlimit ) {
				debug( "Gave up waiting for collector address file\n",$debuglevel);
				debug_flush();
				return 0;
        	} else {
            	sleep ($loopcount * $backoff);
        	}
    	}
		#print "ok\n";
		print "Collector ";
	}

	if($daemonlist =~ /NEGOTIATOR/i) {
		#print "Has negotiator dropped an address file yet - ";
		# now wait for the negotiator to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $negotiatoradr = `condor_config_val NEGOTIATOR_ADDRESS_FILE`;
		$negotiatoradr =~ s/\012+$//;
		$negotiatoradr =~ s/\015+$//;
		debug( "NEGOTIATOR_ADDRESS_FILE: $negotiatoradr\n",$debuglevel);
    	debug( "We are waiting for the file to exist\n",$debuglevel);
    	# Where is the negotiator address file? wait for it to exist
    	my $havenegotiatoraddr = "no";
    	$loopcount = 0;
    	while($havenegotiatoraddr ne "yes") {
        	$loopcount++;
        	debug( "Looking for $negotiatoradr\n",$debuglevel);
        	if( -f $negotiatoradr ) {
            	debug( "Found it!!!! negotiator address file\n",$debuglevel);
            	$havenegotiatoraddr = "yes";
        	} elsif ( $loopcount == $runlimit ) {
				debug( "Gave up waiting for negotiator address file\n",$debuglevel);
				debug_flush();
				return 0;
        	} else {
            	sleep ($loopcount * $backoff);
        	}
    	}
		#print "ok\n";
		print "Negotiator ";
	}

	if($daemonlist =~ /STARTD/i) {
		#print "Has startd dropped an address file yet - ";
		# now wait for the startd to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $startdadr = `condor_config_val STARTD_ADDRESS_FILE`;
		$startdadr =~ s/\012+$//;
		$startdadr =~ s/\015+$//;
		debug( "STARTD_ADDRESS_FILE is: $startdadr\n",$debuglevel);
    	debug( "We are waiting for the file to exist\n",$debuglevel);
    	# Where is the startd address file? wait for it to exist
    	my $havestartdaddr = "no";
    	$loopcount = 0;
    	while($havestartdaddr ne "yes") {
        	$loopcount++;
        	debug( "Looking for $startdadr\n",$debuglevel);
        	if( -f $startdadr ) {
            	debug( "Found it!!!! startd address file\n",$debuglevel);
            	$havestartdaddr = "yes";
        	} elsif ( $loopcount == $runlimit ) {
				debug( "Gave up waiting for startd address file\n",$debuglevel);
				debug_flush();
				return 0;
        	} else {
            	sleep ($loopcount * $backoff);
        	}
    	}
		#print "ok\n";
		print "Startd ";
	}

	####################################################################

	if($daemonlist =~ /SCHEDD/i) {
		#print "Has schedd dropped an address file yet - ";
		# now wait for the schedd to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $scheddadr = `condor_config_val SCHEDD_ADDRESS_FILE`;
		$scheddadr =~ s/\012+$//;
		$scheddadr =~ s/\015+$//;
		debug( "SCHEDD_ADDRESS_FILE is: $scheddadr\n",$debuglevel);
    	debug( "We are waiting for the file to exist\n",$debuglevel);
    	# Where is the schedd address file? wait for it to exist
    	my $havescheddaddr = "no";
    	$loopcount = 0;
    	while($havescheddaddr ne "yes") {
        	$loopcount++;
        	debug( "Looking for $scheddadr\n",$debuglevel);
        	if( -f $scheddadr ) {
            	debug( "Found it!!!! schedd address file\n",$debuglevel);
            	$havescheddaddr = "yes";
        	} elsif ( $loopcount == $runlimit ) {
				debug( "Gave up waiting for schedd address file\n",$debuglevel);
				debug_flush();
				return 0;
        	} else {
            	sleep 1;
        	}
    	}
		#print "ok\n";
		print "Schedd ";
	}

	if($daemonlist =~ /STARTD/i) {
		my $output = "";
		my @cmd = ();
		my $done = "no";
        # lets wait for the collector to know about it if we have a collector
            my $currenthost = CondorTest::getFqdnHost();
            if(($daemonlist =~ /COLLECTOR/i) && ($personal_startup_wait eq "true")) {
                #print "Waiting for collector to see startd - ";
                $loopcount = 0;
                while(1) {
                    $loopcount++;
					@cmd = ();
                    @cmd = `condor_status -startd -format \"%s\\n\" name`;
                    
                    my $res = $?;
                    if ($res != 0) {
			# This might mean that the collector isn't running - but we also sometimes have
			# a condition where the collector isn't ready yet.  So we'll retry a few times.
                    	print "\n", timestamp(), "condor_status returned error code $res\n";
                    	print timestamp(), " The collector probably is not running after all, giving up\n";
                    	print timestamp(), " Output from condor_status:\n";
                    	print @cmd;
	
						if($loopcount < $runlimit) {
			    			print timestamp(), " Retrying...\n";
			    			sleep 1;
			    			next;
						} else {
			    			print timestamp(), " Hit the retry limit.  Erroring out.\n";
							debug_flush();
			    			return 0;
						}
					}

					foreach my $arg (@cmd) {
						fullchomp($arg);
						print "$arg\n";
                    	if($arg =~ /$currenthost/) {
                        	#print "ok\n";
							$done = "yes";
							$output = $arg;
                        	last;
                    	}
					}
					if($done eq "yes") {
						last;
					}

                    if($loopcount == $runlimit) { 
                        print "startd did not start - bad\n";
                        print timestamp(), " Timed out waiting for collector to see startd\n";
                        last; 
                    }
                    sleep ($loopcount * $backoff);
                }
            }
			print "Startd in Collector now(<$output>) ";
	}

	if($daemonlist =~ /SCHEDD/i) {
		my $output = "";
		# lets wait for the collector to know about it
		# if we have a collector
		my $haveschedd = "";
		my $done = "no";
		my $currenthost = CondorTest::getFqdnHost();
		if(($daemonlist =~ /COLLECTOR/i) && ($personal_startup_wait eq "true")) {
			#print "Waiting for collector to see schedd - ";
			$loopcount = 0;
			TRY: while( $done eq "no") {
				$loopcount += 1;
				my @cmd = `condor_status -schedd -format \"%s\\n\" name`;

    			foreach my $line (@cmd)
    			{
					fullchomp($line);
        			if( $line =~ /^.*$currenthost.*/)
        			{
						#print "ok\n";
            			$done = "yes";
						$output = $line;
						last TRY;
        			}
    			}
				if($loopcount == $runlimit) { 
					print "schedd did not start - bad\n";
					debug_flush();
					last; 
				}
				sleep ($loopcount * $backoff);
			}
		}
		print "Schedd in Collector now($output) ";
	}


	if($daemonlist =~ /NEGOTIATOR/i) {
		my $output = "";
		# lets wait for the collector to know about it
		# if we have a collector
		my $havenegotiator = "";
		my $done = "no";
		my $currenthost = CondorTest::getFqdnHost();
		if(($daemonlist =~ /COLLECTOR/i) && ($personal_startup_wait eq "true")) {
			#print "Waiting for collector to see negotiator - ";
			$loopcount = 0;
			TRY: while( $done eq "no") {
				$loopcount += 1;
				my @cmd = `condor_status -negotiator -format \"%s\\n\" name`;

    			foreach my $line (@cmd)
    			{
					fullchomp($line);
        			if( $line =~ /^.*$currenthost.*/)
        			{
						#print "ok\n";
            			$done = "yes";
						$output = $line;
						last TRY;
        			}
    			}
				if($loopcount == $runlimit) { 
					print "negotiator did not start - bad\n";
					debug_flush();
					last; 
				}
				sleep ($loopcount * $backoff);
			}
		}
		print "Negotiator in Collector now($output)\n";
	}

	debug("In IsRunningYet calling CollectDaemonPids\n",$debuglevel);
	if($btdebug == 1) {
		print "In IsRunningYet calling CollectDaemonPids\n";
	}
	CollectDaemonPids();
	if($btdebug == 1) {
		print "In IsRunningYet back from calling CollectDaemonPids\n";
	}
	debug("Leaving IsRunningYet\n",$debuglevel);
	if($btdebug == 1) {
		print "Leaving IsRunningYet\n";
	}
	#$debuglevel = $old_debuglevel;

	return(1);
}

#################################################################
#
# CollectWhoPids
#
# Ask condor_who about daemons, write file WHOPIDS to cross
# check PIDS
#
#################################################################

sub CollectWhoPids
{
	my $logdir = shift;
	my $action = shift;

	my @adarray = ();
	if($btdebug == 1) {
		print "Enter CollectWhoPids\n";
	}
	CondorTest::runCondorTool("condor_who -daemon -log $logdir",\@adarray,2,{emit_output=>0});

	if(defined $action) {
		if($action eq "PIDFILE") {
			open(WP,">$logdir/WHOPIDS") or die "Failed opening: $logdir/WHOPIDS:$!\n";
		} elsif($action eq "ALLPIDFILE") {
			open(WP,">$logdir/ALLPIDS") or die "Failed opening: $logdir/ALLPIDS:$!\n";
		}
	}
	foreach my $wholine (@adarray) {
		CondorUtils::fullchomp($wholine);
		# 5th item in goes from no to the exit code when it is done
		if($wholine =~ /(\w+)\s+(\w+)\s+(\d+)\s+(\d+)\s+(\w+)\s+<(.*)>\s+.*/) {
			if($btdebug == 1) {
				print "running: $1 PID $3 exit code $5 \@$6\n";
			}
			if(defined $action) {
				if(($action eq "PIDFILE") or ($action eq "ALLPIDFILE")) {
					print WP "$3 $1\n";
				}
			}
		} elsif($wholine =~ /(\w+)\s+(\w+)\s+(\d+)\s+(\d+)\s+(\d+)\s+<(.*)>\s+.*/) {
			print "done: $1 PID $3 exit code $5\n";
			if(defined $action) {
				if($action eq "ALLPIDFILE") {
					print WP "$3 $1\n";
				}
			}
		} elsif($wholine =~ /(\w+)\s+(\w+)\s+(\d+)\s+(\w+).*/) {
		# even get funky master pid
			if(defined $action) {
				if($action eq "ALLPIDFILE") {
					print WP "$3 $1\n";
				}
			}
		# Master     no       10124       no     no .*
		} else {
			#print "parse error: $wholine\n";
		}
	}
	if(defined $action) {
		if(($action eq "PIDFILE") or ($action eq "ALLPIDFILE")) {
			close(WP);
		}
	}
	if($btdebug == 1) {
		print "Leave CollectWhoPids\n";
	}

}

#################################################################
#
# CollectDaemonPids
#
# 	Open each known daemon's log and extract its PID
# 	and collect them all in a file called PIDS in the
#	log directory.
#
#################################################################

sub CollectDaemonPids {
    my $daemonlist = `condor_config_val daemon_list`;
    $daemonlist =~ s/\s*//g;
    my @daemons = split /,/, $daemonlist;

    my $logdir = `condor_config_val log`;
	if($btdebug == 1) {
		print "Log file from config: $logdir\n";
	}
	$_ = $logdir;
	s/\\/\//g;
	$logdir = $_;
	if($btdebug == 1) {
		print "CollectDaemonPids start Log file from config after edit: $logdir\n";
	}
    CondorUtils::fullchomp($logdir);

    
	CollectWhoPids($logdir,"PIDFILE");
    my $logfile = "$logdir/MasterLog";
    debug("In CollectDaemonPids(), examining log $logfile\n", $debuglevel);
    open(TA, '<', $logfile) or die "Can not read '$logfile': $!\n";
    my $master;
    my %pids = ();
    while(<TA>) {
        fullchomp;
        if(/PID\s+=\s+(\d+)/) {
            # Capture the master pid.  At kill time we will suggest with signal 3
            # that the master and friends go away before we get blunt.
            $master = $1;

            # Every time we find a new master pid it means that all the previous pids
            # we had recorded were for a different instance of Condor.  So reset the list.
            %pids = ();
        }
        elsif(/Started DaemonCore process\s\"(.*)\",\s+pid\s+and\s+pgroup\s+=\s+(\d+)/) {
            # We store these in a hash because if the daemon crashes and restarts
            # we only want the latest value for its pid.
            $pids{$1} = $2;
        }
    }
    close(TA);

    my $pidfile = "$logdir/PIDS";
    open(PIDS, '>', $pidfile) or die "Can not create file '$pidfile': $!\n";
    #debug("Master pid: $master\n");
    print PIDS "$master MASTER\n";
    foreach my $daemon (keys %pids) {
        debug("\t$daemon pid: $pids{$daemon}\n", $debuglevel);
		if(CondorUtils::is_windows() == 1) {
			if($daemon =~ /.*\\(\w+)/) {
        		print PIDS "$pids{$daemon} $1\n";
			} else {
        		print PIDS "$pids{$daemon} $daemon\n";
			}
		} else {
			if($daemon =~ /.*\/(\w+)/) {
        		print PIDS "$pids{$daemon} $1\n";
			} else {
        		print PIDS "$pids{$daemon} $daemon\n";
			}
		}	
    }
    close(PIDS);
	if($btdebug == 1) {
		print "CollectDaemonPids done\n";
	}
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
	if($btdebug == 1) {
		print "KillDaemonPids: Current: $oldconfig Killing: $desiredconfig\n";
	}
	$ENV{CONDOR_CONFIG} = $desiredconfig;
	my $logdir = `condor_config_val log`;
	$logdir =~ s/\012+$//;
	$logdir =~ s/\015+$//;
	$_ = $logdir;
	s/\\/\//g;
	$logdir = $_;
	my $masterpid = 0;
	my $cnt = 0;
	my $cmd;
	my $saveddebuglevel = $debuglevel;
	$debuglevel = 3;

	if($btdebug == 1) {
		print "Before the Kill of $desiredconfig\n";
	}
	CollectWhoPids($logdir);

	CondorTest::runToolNTimes("condor_off -master",1,0);

	if($isnightly) {
		DisplayPartialLocalConfig($desiredconfig);
	}

	#print "logs are here:$logdir\n";
	my $pidfile = $logdir . "/PIDS";
	my $whopidfile = $logdir . "/WHOPIDS";
	#debug("Asked to kill: $oldconfig\n",$debuglevel);
	my $thispid = 0;
	# first find the master and use a kill 3(fast kill)
	#open(PD,"<$pidfile") or die "Can not open :$pidfile:$!\n";
	#while(<PD>) {
		#fullchomp();
		#$thispid = $_;
		#print "$thispid\n";
		#if($thispid =~ /^(\d+)\s+MASTER.*$/) {
			#print "fast kill on master $1\n";
			#$masterpid = $1;
			#if(CondorUtils::is_windows() == 1) {
				#$cmd = "taskkill /PID $masterpid /T /F";
				#system($cmd);
			#} else {
				#$cnt = kill 3, $masterpid;
			#}
			#debug("Gentle kill for master: $masterpid $thispid($cnt)\n",$debuglevel);
			#last;
		#}
	#}
	#close(PD);
	# give it a little time for a shutdown
	sleep(10);
	my $res = CheckPids($pidfile);
	if($res eq "all gone") {
	} elsif($res eq "going away") {
		sleep(20);
		$res = CheckPids($pidfile);
	} elsif($res eq "master gone") {
		if($btdebug == 1) {
			print "Master gone but daemons persist\n";
		}
	} elsif($res eq "stubborn") {
		if($btdebug == 1) {
			print "all daemons persist\n";
		}
		sleep(20);
		$res = CheckPids($pidfile);
	}

	# after a bit more
	if($res eq "all gone") {
	} elsif($res eq "going away") {
		sleep(20);
		$res = CheckPids($pidfile);
	} elsif($res eq "master gone") {
		if($btdebug == 1) {
			print "Master gone but daemons persist\n";
		}
	} elsif($res eq "stubborn") {
		sleep(20);
		$res = CheckPids($pidfile);
		if($btdebug == 1) {
			print "all daemons persist\n";
		}
	}

	# did it work.... is process still around? after 3 tries, where are we?
	if($btdebug == 1) {
		print "Last result was: $res\n";
	}
	if($res eq "going away") {
		#while($res ne "all gone") {
			sleep(10);
			$res = CheckPids($pidfile);
		#}
	} elsif(($res eq "stubborn") || ($res eq "master gone")) {
			sleep(10);
			$res = CheckPids($pidfile,"kill all");
			$res = CheckPids($pidfile,"kill master");
			if($btdebug == 1) {
				print "imposed sudden death, where are we now?\n";
			}
			sleep(10);
			$res = CheckPids($pidfile,);
			if($btdebug == 1) {
				print "After a bullet to the head: $res\n";
			}
	}

	if($btdebug == 1) {
		print "After the Kill of $desiredconfig\n";
	}
	#CollectWhoPids($logdir); condor_who fails with no daemons Just do the check againt
	# daemoon pids it collected initially
	CheckPids($whopidfile);


	# reset config to whatever it was.
	$ENV{CONDOR_CONFIG} = $oldconfig;
	$debuglevel = $saveddebuglevel;
	return(0);
}

#CheckPids($pidfile);
#################################################################
#
# CheckPids is handed a list of pids which may or may not be
# around still as we told master to go away. Check each pid
# and give status.

sub CheckPids
{
	my $pidfile = shift;
	my $action = shift;
	my $line = "";
	my @grift = ();
	my $linecount = 0;
	my $masterlives = 0;
	my $otherslive = 0;
	my $howmanydeamons = 0;
	
	system("date");
	if(defined $action) {
		if($btdebug == 1) {
			print "CheckPids: $pidfile Action: $action\n";
		}
	} else {
		if($btdebug == 1) {
			print "CheckPids: $pidfile Action: none\n";
		}
	}
	open(PF, "<$pidfile") or die "Failed to find pid file:$pidfile:$1\n";
	while(<PF>) {
		chomp();
		$line = $_;
		my $daemon = "";
		my $pid = "";
		if($line =~ /(\d+)\s+(\w+).*/) {
			$daemon = $2;
			$pid = $1;
			$howmanydeamons += 1;
			@grift = ();
			if(CondorUtils::is_windows() == 1) {
				@grift = `tasklist | grep $pid`;
				my $realpid = "";
				$linecount = @grift;
				if($linecount == 0) {
					if($btdebug == 1) {
						print "Daemon $daemon PID $pid is gone\n";
					}
				} else {
					if($btdebug == 1) {
						print "Fishing for status: $daemon/$pid\n";
					}
					foreach my $ent (@grift){
						chomp($ent);
						if($ent =~ /(.*?)\s+(\d+)\s+(\w+).*/) {
							if($2 eq $pid) {
								if($btdebug == 1) {
									print "Daemon $daemon PID $pid is still alive\n";
								}
								if($daemon eq "MASTER") {
									$masterlives += 1;
								} else {
									$otherslive += 1;
								}
							} else {
								if($btdebug == 1) {
									print "Ignoring $1 pid $2\n";
								}
							}
						}
					}
					if(defined $action) {
						if($action eq "kill all") {
							if($daemon ne "MASTER") {
								my $cmd = "taskkill /PID $pid /T /F";
								system($cmd);
							}
						}
						if($action eq "kill master") {
							if($daemon eq "MASTER") {
								my $cmd = "taskkill /PID $pid /T /F";
								system($cmd);
							}
						}
					}
				}
			} else {
				@grift = `ps $1`;
				$linecount = @grift;
				if($linecount == 1) {
					#print "Daemon $2 PID $1 is gone\n";
				} elsif($linecount == 2) {
					#print "Daemon $2 PID $1 is still alive\n";
					if($2 eq "MASTER") {
						$masterlives += 1;
					} else {
						$otherslive += 1;
					}
					if(defined $action) {
						if($action eq "kill all") {
							if($daemon ne "MASTER") {
								kill 15, $1;
							}
						}
						if($action eq "kill master") {
							if($daemon eq "MASTER") {
								kill 15, $1;
							}
						}
					}
				} else {
					print "Should not have more the two ps lines unless windows: daemon $2\n";
					foreach my $ps (@grift) {
						print "$ps";
					}
				}
			}
		} else {
			print "Mismatch: $line\n";
		}
	}
	close(PF);
	if(($masterlives == 0) && ($otherslive == 0)) {
		# condor personal is off;
		if($btdebug == 1) {
			print "all gone\n";
		}
		return("all gone");
	}
	elsif(($masterlives == 0) && ($otherslive > 0)) { 
		#master gone but some daemons persist
		if($btdebug == 1) {
			print "master gone\n";
		}
		return("master gone");
	} else {
		if( $howmanydeamons > ($masterlives + $otherslive)) {
			if($btdebug == 1) {
				print "going away\n";
			}
			return("going away");
		} else {
			if($btdebug == 1) {
				print "stubborn\n";
			}
			return("stubborn");
		}
	}
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
		if( $line =~ /^\s*<([^>]+)>\s*$/ ) {
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
	my $mypid = $$;
	my $res = 1;
	my $mysaveme = $testname . ".saveme";
	$res = system("mkdir -p $mysaveme");
	if($res != 0) {
		print "SaveMeSetup: Could not create \"saveme\" directory for test\n";
		return(0);
	}
	my $mypiddir = $mysaveme . "/" . $mypid;
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


1;
