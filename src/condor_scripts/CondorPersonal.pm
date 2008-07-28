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
use Net::Domain qw(hostfqdn);

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
#   secprepostsrc	New security settings 											$personal_sec_prepost_src
#	condordaemon	daemon list to start				contents of config template $personal_daemons
#	condorconfig	Name for condor config file			condor_config				$personal_config
#	condordomain	Name for domain						local						$condordomain
#	condorlocal		Name for condor local config 		condor_config.local			$personal_local
#	condor			"install" or path to tarball 									$condordistribution
#	collector	 	Used to define COLLECTOR_HOST									$collectorhost
#	nameschedd	 	Used to define SCHEDD_NAME			cat(name and collector)		$scheddname
#	condorhost	 	Used to define CONDOR_HOST										$condorhost
#	ports			Select dynamic or normal ports		dynamic						$portchanges	
#   slot				sets NUM_CPUS NUM_SLOTS				none
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

require 5.0;
use Carp;
use Cwd;
use FileHandle;
use POSIX "sys_wait_h";
use Socket;
use Sys::Hostname;
use CondorTest;

my $topleveldir = getcwd();
my $home = $topleveldir;
my $localdir;
my $condorlocaldir;
my $pid = $$;
my $version = ""; # remote, middle, ....... for naming schedd "schedd . pid . version"
my $mastername = ""; # master_$verison
my $iswindows = IsThisWindows();

BEGIN
{
    %personal_condor_params;
    %personal_config_changes;
	$personal_config = "condor_config";
	$personal_template = "condor_config_template";
	$personal_daemons = "";
	$personal_local = "condor_config.local";
	$personal_local_src = "";
	$personal_local_post_src = "";
	$personal_sec_prepost_src = "";
	$personal_universe = "";
	$personal_startup_wait = "true";
	$portchanges = "dynamic";
	$DEBUG = 0;
	$collector_port = "0";
	$personal_config_file = "";
	$condordomain = "";
	$procdaddress = "";
}

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
	my $paramfile = shift || die "Missing parameter file!\n";
	$version = shift || die "Missing parameter version!\n";
	my $config_and_port = "";
	my $winpath = "";

	if($arraysz == 2) {
		$mpid = $pid; # assign process id
	} else {
		$mpid = shift; # assign process id
	}

	system("mkdir -p $topleveldir");
	$topleveldir = $topleveldir . "/" . $mpid;
	system("mkdir -p $topleveldir");
	$topleveldir = $topleveldir . "/" . $mpid . $version;
	system("mkdir -p $topleveldir");

	$procdaddress = $mpid . $version;

	$personal_config_file = $topleveldir ."/condor_config";

	CondorPersonal::ParsePersonalCondorParams($paramfile);

	$localdir = CondorPersonal::InstallPersonalCondor();
	if($localdir eq "")
	{
		return("Failed to do needed Condor Install\n");
	}

	if( $ENV{NMI_PLATFORM} =~ /win/ ){
		$winpath = `cygpath -w $localdir`;
		CondorTest::fullchomp($winpath);
		$condorlocaldir = $winpath;
		CondorPersonal::TunePersonalCondor($condorlocaldir, $mpid);
	} else {
		CondorPersonal::TunePersonalCondor($localdir, $mpid);
	}

	$collector_port = CondorPersonal::StartPersonalCondor();

	debug( "collector port is $collector_port\n");

	if( $ENV{NMI_PLATFORM} =~ /win/ ){
		$winpath = `cygpath -w $personal_config_file`;
		CondorTest::fullchomp($winpath);
		print "Windows conversion of personal config file to $winpath!!\n";
		$config_and_port = $winpath . "+" . $collector_port ;
	} else {
		$config_and_port = $personal_config_file . "+" . $collector_port ;
	}

	debug( "StartCondor config_and_port is --$config_and_port--\n");
	CondorPersonal::Reset();
	debug( "StartCondor config_and_port is --$config_and_port--\n");
	return( $config_and_port );
}

sub debug
{
    my $string = shift;
    print( "DEBUG ", timestamp(), ": $string" ) if $DEBUG;
}

sub DebugOn
{
	$DEBUG = 1;
}

sub DebugOff
{
	$DEBUG = 0;
}

sub timestamp {
    return scalar localtime();
}

sub Reset
{
	debug( "CondorPersonal RESET\n");
    %personal_condor_params = {};
    %personal_config_changes = {};
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
    my $submit_file = shift || croak "missing submit file argument";
    my $line = 0;

    if( ! open( SUBMIT_FILE, $submit_file ) )
    {
		die "error opening \"$submit_file\": $!\n";
		return 0;
    }
    
    #debug( "reading submit file...\n" );
    while( <SUBMIT_FILE> )
    {
		CondorTest::fullchomp($_);
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
			CondorTest::fullchomp($value);
		
			# Do proper environment substitution
	    	if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ ) {
				my $envlookup = $ENV{$2};
	    		#debug( "Found $envlookup in environment \n");
				$value = $1.$envlookup.$3;
	    	}

	    	debug( "(CondorPersonal.pm) $variable = $value\n" );
	    
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
		CondorTest::fullchomp($_);
		$line = $_;
		debug ("--$line--\n");

		if( $line =~ /^\s*($pathtoconfig)\s*$/ )
		{
			$matchedconfig = $1;
			debug ("Matched! $1\n");
		}

		if( $line =~ /(Can't find address for this master)/ )
		{
			$badness = $1;
			debug( "Not currently running! $1\n");
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

#################################################################
#
# InstallPersonalCondor
#
#	We either find binaries in the environment or we install
#		a particular tar ball.
#

sub InstallPersonalCondor
{
	my %control = %personal_condor_params;

	my $schedd;
	my $master;
	my $collector;
	my $submit;
	my $startd;
	my $negotiator;
	my $condorq = CondorTest::Which("condor_q");
	my $sbinloc;
	my $configline = "";
	my @configfiles;

	if( exists $control{"condor"} )
	{
		$condordistribution = $control{"condor"};
		debug( "Install this condor --$condordistribution--\n");
		if( $condordistribution eq "nightlies" ) {
			# test if this is really the environment we are in or
			# switch it to install mode.
			 if(! -f "../../condor/sbin/condor_master") {
			 	$condordistribution = "install";
			 }
		}
		if( $condordistribution eq "install" )
		{
			# where is the hosting condor_config file? The one assumed to be based
			# on a setup with condor_configure.
			open(CONFIG,"condor_config_val -config | ") || die "Can not find config file: $!\n";
			while(<CONFIG>)
			{
				CondorTest::fullchomp($_);
				$configline = $_;
				push @configfiles, $configline;
			}
			close(CONFIG);
			$personal_condor_params{"condortemplate"} = shift @configfiles;
			$personal_condor_params{"condorlocalsrc"} = shift @configfiles;
			#
			debug( "My path to condor_q is $condorq and topleveldir is $topleveldir\n");

			if( $condorq =~ /^(\/.*\/)(\w+)\s*$/ ) {
				debug( "Root path $1 and base $2\n");
				$binloc = $1;	# we'll get our binaries here.
			} elsif(-f "../release_dir/bin/condor_status") {
				print "Bummer which condor_q failed\n";
				print "Using ../release_dir/bin(s)\n";
				$binloc = "../release_dir/bin"; # we'll get our binaries here.
			}
			else
			{
				print "which condor_q responded <<<$condor_q>>>! CondorPersonal Failing now\n";
				die "Can not seem to find a Condor install!\n";
			}
			

			if( $binloc =~ /^(\/.*\/)s*bin\/\s*$/ )
			{
				debug( "Root path to sbin is $1\n");
				$sbinloc = $1;	# we'll get our binaries here. # local_dir is here
			}
			else
			{
				die "Can not seem to locate Condor release binaries\n";
			}

			$schedd = $sbinloc . "sbin/". "condor_schedd";
			$master = $sbinloc . "sbin/". "condor_master";
			$collector = $sbinloc . "sbin/". "condor_collector";
			$submit = $binloc . "condor_submit";
			$startd = $sbinloc . "sbin/". "condor_startd";
			$negotiator = $sbinloc . "sbin/". "condor_negotiator";

			debug( "$schedd $master $collector $submit $startd $negotiator\n");


			debug( "Sandbox started rooted here: $topleveldir\n");

			system("mkdir -p $topleveldir/execute");
			system("mkdir -p $topleveldir/spool");
			system("mkdir -p $topleveldir/log");
			system("mkdir -p $topleveldir/log/tmp");
		}
		elsif( $condordistribution eq "nightlies" )
		{
			# we want a mechanism by which to find the condor binaries
			# we are testing. But we know where they are relative to us
			# ../../condor/bin etc
			# That is simply the nightly test setup.... for now at least
			# where is the hosting condor_config file? The one assumed to be based
			# on a setup with condor_configure.

			debug(" Nightlies - find environment config files\n");
			open(CONFIG,"condor_config_val -config | ") || die "Can not find config file: $!\n";
			while(<CONFIG>)
			{
				CondorTest::fullchomp($_);
				$configline = $_;
				debug( "$_\n" );
				push @configfiles, $configline;
			}
			close(CONFIG);
			# yes this assumes we don't have multiple config files!
			$personal_condor_params{"condortemplate"} = shift @configfiles;
			$personal_condor_params{"condorlocalsrc"} = shift @configfiles;

			debug( "My path to condor_q is $condorq and topleveldir is $topleveldir\n");

			if( $condorq =~ /^(\/.*\/)(\w+)\s*$/ )
			{
				debug( "Root path $1 and base $2\n");
				$binloc = $1;	# we'll get our binaries here.
			}
			else
			{
				print "which condor_q responded <<<$condor_q>>>! CondorPersonal Failing now\n";
				die "Can not seem to find a Condor install!\n";
			}
			

			if( $binloc =~ /^(\/.*\/)bin\/\s*$/ )
			{
				debug( "Root path to sbin is $1\n");
				$sbinloc = $1;	# we'll get our binaries here. # local_dir is here
			}
			else
			{
				die "Can not seem to locate Condor release binaries\n";
			}
			#$binloc = "../../condor/bin/";
			#$sbinloc = "../../condor/";

			debug( "My path to condor_q is $binloc and topleveldir is $topleveldir\n");

			$schedd = $sbinloc . "sbin/" .  "condor_schedd";
			$master = $sbinloc . "sbin/" .  "condor_master";
			$collector = $sbinloc . "sbin/" .  "condor_collector";
			$submit = $binloc . "condor_submit";
			$startd = $sbinloc . "sbin/" .  "condor_startd";
			$negotiator = $sbinloc . "sbin/" .  "condor_negotiator";

			debug( "$schedd $master $collector $submit $startd $negotiator\n");


			debug( "Sandbox started rooted here: $topleveldir\n");

			system("mkdir -p $topleveldir/execute");
			system("mkdir -p $topleveldir/spool");
			system("mkdir -p $topleveldir/log");
			system("mkdir -p $topleveldir/log/tmp");
		}
		elsif( -e $condordistribution )
		{
			# in this option we ought to run condor_configure
			# to get a current config files but we'll do this
			# after getting the current condor_config from
			# the environment we are in as it is supposed to
			# have been generated this way in the nightly tests
			# run in the NWO.

			my $res = chdir "$topleveldir";
			if(! $res)
			{
				die "Relcation failed!\n";
				exit(1);
			}
			system("mkdir -p $topleveldir/execute");
			system("mkdir -p $topleveldir/spool");
			system("mkdir -p $topleveldir/log");
			system("tar -xf $home/$condordistribution");
			$sbinloc = $topleveldir; # local_dir is here
			chdir "$home";
		}
		else
		{
			die "Undiscernable install directive! (condor = $condordistribution)\n";
		}
	}
	else
	{
		debug( " no condor attribute so not installing condor \n");
		$sbinloc = "";
	}
	debug( "InstallPersonalCondor returning $sbinloc for LOCAL_DIR setting\n");

	# I hate to do this but....
    # The quill daemon really wants to see the passwd file .pgpass
    # when it starts and IF this is a quill test, then quill is within
    # $personal_daemons AND $topleveldir/../pgpass wants to  be
    # $topleveldir/spool/.pgpass
    my %control = %personal_condor_params;
    # was a special daemon list called out?
    if( exists $control{"condordaemons"} )
    {
        $personal_daemons = $control{"condordaemons"};
    }

    debug( "Looking to see if this is a quill test..........\n");
    debug( "Daemonlist requested is <<$personal_daemons>>\n");
    if($personal_daemons =~ /.*quill.*/) {
        debug( "Yup it is a quill test..........copy in .pgpass file\n");
        my $cmd = "cp $topleveldir/../pgpass $topleveldir/spool/.pgpass";
        system("$cmd");
    }
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
	my $myhost = hostfqdn();
	my @domainparts = split /\./, $myhost;
	my $condorhost = "";
	my $collectorhost = "";
	my $mpid = "";
	my $localdir = shift;

	if(scalar( @_ ) == 1) {
		$mpid = $pid; # assign process id
	} else {
		$mpid = shift; # assign process id
	}

	debug( "TunePersonalCondor setting LOCAL_DIR to $localdir\n");
	#print "domain parts follow:";
	#foreach my $part (@domainparts)
	#{
		#print " $part";
	#}
	#print "\n";

	#$myhost = @domainparts[0];

	debug( "My basic name is $myhost\n");


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

	debug( "Fully qualified domain name is ************************ $condorhost ********************\n");

	# was a special template called out?
	if( exists $control{"condortemplate"} )
	{
		$personal_template = $control{"condortemplate"};
	}

	# was a special config file called out?
	if( exists $control{"condorconfig"} )
	{
		$personal_config = $control{"condorconfig"};
	}

	# was a special daemon list called out?
	if( exists $control{"condordaemons"} )
	{
		$personal_daemons = $control{"condordaemons"};
	}

	# was a special local config file name called out?
	if( exists $control{"condorlocal"} )
	{
		$personal_local = $control{"condorlocal"};
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
		debug( "HMMMMMMMMMMM universe request is $personal_universe\n");
	}


	#debug( "Proto file is --$personal_template--\n");

	$personalmaster = "$topleveldir/sbin/condor_master";

	#filter fig file storing entries we set so we can test
	#for completeness when we are done
	my $mytoppath = "";
	if( $ENV{NMI_PLATFORM} =~ /win/ ){
		$mytoppath = `cygpath -w $topleveldir`;
		CondorTest::fullchomp($mytoppath);
	} else {
		$mytoppath =  $topleveldir;
	}

	my $line;
	#system("ls;pwd");
	#print "***************** opening $personal_template as config file template *****************\n";
	open(TEMPLATE,"<$personal_template")  || die "Can not open template<<$personal_template>>: $!\n";
	debug( "want to open new config file as $topleveldir/$personal_config\n");
	open(NEW,">$topleveldir/$personal_config") || die "Can not open new config file: $!\n";
	while(<TEMPLATE>)
	{
		CondorTest::fullchomp($_);
		$line = $_;
		if( $line =~ /^RELEASE_DIR\s*=.*/ )
		{
			#debug( "-----------$line-----------\n");
			$personal_config_changes{"RELEASE_DIR"} = "RELEASE_DIR = $localdir\n";
			print NEW "RELEASE_DIR = $localdir\n";
		}
		elsif( $line =~ /^LOCAL_DIR\s*=.*/ )
		{
			#debug( "-----------$line-----------\n");
			$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $mytoppath\n";
			print NEW "LOCAL_DIR = $mytoppath\n";
		}
		elsif( $line =~ /^LOCAL_CONFIG_FILE\s*=.*/ )
		{
			#debug( "-----------$line-----------\n");
			$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $mytoppath\n";
			print NEW "LOCAL_CONFIG_FILE = $mytoppath/$personal_local\n";
		}
		else
		{
			print NEW "$line\n";
		}
	}
	close(TEMPLATE);

	if( ! exists $personal_config_changes{"CONDOR_HOST"} )
	{
		$personal_config_changes{"CONDOR_HOST"} = "CONDOR_HOST = $condorhost\n";
	}

	close(NEW);

	if( exists $control{"ports"} )
	{
		debug( "Port Changes being Processed!!!!!!!!!!!!!!!!!!!!\n");
		$portchanges = $control{"ports"};
		debug( "portchanges set to $portchanges\n");
	}

	open(NEW,">$topleveldir/$personal_local")  || die "Can not open template: $!\n";
	if($personal_local_src ne "")
	{
		#print "******************** Must seed condor_config.local <<$personal_local_src>> ************************\n";
		open(LOCSRC,"<$personal_local_src") || die "Can not open local config template: $!\n";
		while(<LOCSRC>)
		{
			CondorTest::fullchomp($_);
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


	print NEW "ALL_DEBUG = D_FULLDEBUG D_NETWORK\n";

	if($personal_daemons ne "")
	{
		# Allow the collector to run on the default and expected port as the main
		# condor install on this system.
		print NEW "DAEMON_LIST = $personal_daemons\n";
	}

	if($personal_universe eq "parallel")
	{
		# set up dedicated scheduler
		print "************************ Adding Dedicated Scheduler $personal_universe Universe ***************************\n";
		print NEW "DedicatedScheduler = \"DedicatedScheduler\@schedd$mpid$version\@$condorhost\"\n";
		print NEW "STARTD_EXPRS = \$(STARTD_EXPRS), DedicatedScheduler\n";
		print NEW "SCHEDD_DEBUG = D_FULLDEBUG\n";
	}

	if( $portchanges eq "dynamic")
	{
		# this variation requests a dynamic port for collector and negotiator
		# and the location where we can look up the adresses.
		if( $collectorhost )
		{
			print NEW "COLLECTOR_HOST = $collectorhost\n";
			debug("COLLECTOR_HOST = $collectorhost\n");
		}
		else
		{
			print NEW "COLLECTOR_HOST = \$(CONDOR_HOST):0\n";
			debug("COLLECTOR_HOST = \$(CONDOR_HOST):0\n");
		}

		# For simulated pools, we need schedds and master to have unique names
		if(exists $control{"nameschedd"}) {
			$mastername = "master" . "_" . $version;
			$scheddname = $mastername . "_schd";
			$startdname = $mastername . "_strtd";
			debug("MASTERNAME now master + $version($mastername)\n");
			print NEW "MASTER_NAME = $mastername\n";
			print NEW "SCHEDD_NAME = $scheddname\n";
			print NEW "STARTD_NAME = $startdname\n";
		} else {
			print NEW "SCHEDD_NAME = schedd$mpid$version\n";
		}


		print NEW "MASTER_ADDRESS_FILE = \$(LOG)/.master_address\n";
		print NEW "COLLECTOR_ADDRESS_FILE = \$(LOG)/.collector_address\n";
		print NEW "NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address\n";

		if( $iswindows == 1 ) {
			print NEW "CONDOR_HOST = \$(IP_ADDRESS)\n";
		} else {
			print NEW "CONDOR_HOST = $condorhost\n";
		}
		print NEW "START = TRUE\n";
		print NEW "RunBenchmarks = FALSE\n";
		print NEW "JAVA_BENCHMARK_TIME = 0\n";
		print NEW "SCHEDD_INTERVAL = 5\n";
		print NEW "UPDATE_INTERVAL = 5\n";
		print NEW "NEGOTIATOR_INTERVAL = 5\n";
		print NEW "CONDOR_JOB_POLL_INTERVAL = 5\n";
		print NEW "PERIODIC_EXPR_TIMESLICE = .99\n";
		print NEW "JOB_START_DELAY = 0\n";
	}
	elsif( $portchanges eq "standard" )
	{
		# Allow the collector to run on the default and expected port as the main
		# condor install on this system.
		if( $collectorhost )
		{
			print NEW "COLLECTOR_HOST = $collectorhost\n";
			debug("COLLECTOR_HOST is $collectorhost\n");
		}
		else
		{
			print NEW "COLLECTOR_HOST = \$(CONDOR_HOST)\n";
			debug("COLLECTOR_HOST is \$(CONDOR_HOST)\n");
		}

		print NEW "CONDOR_HOST = $condorhost\n";
		print NEW "START = TRUE\n";
		print NEW "SCHEDD_INTERVAL = 5\n";
		print NEW "UPDATE_INTERVAL = 5\n";
		print NEW "NEGOTIATOR_INTERVAL = 5\n";
		print NEW "CONDOR_JOB_POLL_INTERVAL = 5\n";
	}
	else
	{
		die "Misdirected request for ports\n";
		exit(1);
	}

	#print NEW "PROCD_LOG = \$(LOG)/ProcLog\n";
	if( $ENV{NMI_PLATFORM} =~ /win/ ){
		print NEW "PROCD_ADDRESS = \\\\.\\pipe\\$procdaddress\n";
	}

	# now we consider configuration requests

	if( exists $control{"slots"} )
	{
		my $myslots = $control{"slots"};
		debug( "Slots wanted! Number = $myslots\n");
		print NEW "NUM_CPUS = $myslots\n";
		print NEW "SLOTS = $myslots\n";
		$gendatafile = $gendatafile . "_" . $myslots . "Slots";
	}

	if($personal_sec_prepost_src ne "")
	{
		print "Adding to local config file from $personal_sec_prepost_src\n";
		open(SECURITY,"<$personal_sec_prepost_src")  || die "Can not do local config additions: $! <<$personal_sec_prepost_src>>\n";
		while(<SECURITY>)
		{
			print NEW "$_";
		}
		close(SECURITY);
	}


	if($personal_local_post_src ne "")
	{
		print "Adding to local config file from $personal_local_post_src\n";
		open(POST,"<$personal_local_post_src")  || die "Can not do local config additions: $! <<$personal_local_post_src>>\n";
		while(<POST>)
		{
			print NEW "$_";
		}
		close(POST);
	}

	close(NEW);
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

	my $configfile = $control{"condorconfig"};
	my $fullconfig = "$topleveldir/$configfile";
	my $oldpath = $ENV{PATH};
	my $newpath = $localdir . "sbin:" . $localdir . "bin:" . "$oldpath";
	my $figpath = "";
	$ENV{PATH} = $newpath;

	debug( "Using this path: --$newpath--\n");

	debug( "Want $configfile for config file\n");

	if( $ENV{NMI_PLATFORM} =~ /win/ ){
		$figpath = `cygpath -w $fullconfig`;
		CondorTest::fullchomp($figpath);
		$fullconfig = $figpath;
		# note: on windows all binaaries in bin!
		$personalmaster = $localdir . "bin/condor_master -f &";
	} else {
		$personalmaster = $localdir . "sbin/condor_master -f &";
	}

	# We may not want to wait for certain daemons to talk
	# to each other on startup.

	my $waitparam = $control{"daemonwait"};
	if($waitparam eq "false") {
		$personal_startup_wait = "false";
	}

	# set up to use the existing generated configfile
	$ENV{CONDOR_CONFIG} = $fullconfig;
	debug( "Is personal condor running config<$fullconfig>\n");
	my $condorstate = IsPersonalRunning($fullconfig);
	debug( "Condor state is $condorstate\n");
	my $fig = $ENV{CONDOR_CONFIG};
	debug( "Condor_config from environment is --$fig--\n");

	# At the momment we only restart/start a personal we just configured 
	# or reconfigured

	if( $condorstate == 0 ) {
		#  not running with this config so treat it like a start case
		print "Condor state is off\n";
		debug( "start up the personal condor!--$personalmaster--\n");
		system($personalmaster);
		system("condor_config_val -v log");
	} else {
		die "Bad state for a new personal condor configuration!<<running :-(>>\n";
	}

	$res = IsRunningYet();
	if($res == 0) {
		die "Can not continue because condor is not running!!!!\n";
	}

	# if this was a dynamic port startup, return the port which
	# the collector is listening on...

	if( $portchanges eq "dynamic" )
	{
		debug("Looking for collector port!\n");
		return( FindCollectorPort() );
	}
	else
	{
		debug("NOT Looking for collector port!\n");
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

    CondorTest::fullchomp($pathtoconfig);
    if($iswindows == 1) {
        $pathtoconfig =~ s/\\/\\\\/g;
    }

	debug("call - condor_config_val -config -master log \n");
    open(CONFIG, "condor_config_val -config -master log 2>&1 |") || die "condor_config_val: $
!\n";
	debug("parse - condor_config_val -config -master log \n");
    while(<CONFIG>) {
        CondorTest::fullchomp($_);
        $line = $_;
        debug ("--$line--\n");


        debug("Looking to match \"$pathtoconfig\"\n");
        if( $line =~ /^.*($pathtoconfig).*$/ ) {
            $matchedconfig = $1;
            debug ("Matched! $1\n");
            last;
        }
    }
    close(CONFIG);

    if( $matchedconfig eq "" ) {
        die "lost: config does not match expected config setting......\n";
    }

    # find the master file to see if it exists and threrfore is running

    open(MADDR,"condor_config_val MASTER_ADDRESS_FILE 2>&1 |") || die "condor_config_val: $
    !\n";
    while(<MADDR>) {
        CondorTest::fullchomp($_);
        $line = $_;
        if($line =~ /^(.*master_address)$/) {
            if(-f $1) {
                debug("Master running\n");
                return(1);
            } else {
                debug("Master not running\n");
                return(0);
            }
        }
    }
	close(MADDR);
}

sub IsRunningYet
{
	$maxattempts = 9;
	$attempts = 0;
	$daemonlist = `condor_config_val daemon_list`;
	CondorTest::fullchomp($dameonlist);
	$collector = 0;
	$schedd = 0;
	$startd = 0;

	# first failure was had test where we looked for
	# a negotiator but MASTER_NEGOTIATOR_CONTROLLER
	# was set. So we will check for bypasses to normal 
	# operation and rewrite the daemon list

	debug("In IsRunningYet\n");
	$daemonlist =~ s/\s*//g;
	@daemons = split /,/, $daemonlist;
	$daemonlist = "";
	$first = 1;
	foreach $daemon (@daemons) {
		$line = "";
		debug("Looking for MASTER_XXXXXX_CONTROLLER for $daemon\n");
		$definedcontrollstr = "condor_config_val MASTER_" . $daemon . "_CONTROLLER";
		open(CCV, "$definedcontrollstr 2>&1 |") || die "condor_config_val: $!\n";
		while(<CCV>) {
			$line = $_;
			if( $line =~ /^.*Not defined.*$/) {
				debug("Add $daemon to daemon list\n");
				if($first == 1) {
					$first = 0;
					$daemonlist = $daemon;
				} else {
					$daemonlist = $daemonlist . "," . $daemon;
				}
			}
			debug("looking: $daemonlist\n");
		}
		close(CCV);
	}


	if($daemonlist =~ /.*MASTER.*/i) {
		# now wait for the master to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		$masteradr = `condor_config_val MASTER_ADDRESS_FILE`;
		$masteradr =~ s/\012+$//;
		$masteradr =~ s/\015+$//;
		debug( "MASTER_ADDRESS_FILE is <<<<<$masteradr>>>>>\n");
    	debug( "We are waiting for the file to exist\n");
    	# Where is the master address file? wait for it to exist
    	$havemasteraddr = "no";
    	while($havemasteraddr ne "yes") {
        	debug( "Looking for $masteradr\n");
        	if( -f $masteradr ) {
            	debug( "Found it!!!! master address file\n");
            	$havemasteraddr = "yes";
        	} else {
            	sleep 1;
        	}
    	}
	}

	if($daemonlist =~ /.*COLLECTOR.*/i){
		# now wait for the collector to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		$collectoradr = `condor_config_val COLLECTOR_ADDRESS_FILE`;
		$collectoradr =~ s/\012+$//;
		$collectoradr =~ s/\015+$//;
		debug( "COLLECTOR_ADDRESS_FILE is <<<<<$collectoradr>>>>>\n");
    	debug( "We are waiting for the file to exist\n");
    	# Where is the collector address file? wait for it to exist
    	$havecollectoraddr = "no";
    	while($havecollectoraddr ne "yes") {
        	debug( "Looking for $collectoradr\n");
        	if( -f $collectoradr ) {
            	debug( "Found it!!!! collector address file\n");
            	$havecollectoraddr = "yes";
        	} else {
            	sleep 1;
        	}
    	}
	}

	if($daemonlist =~ /.*NEGOTIATOR.*/i) {
		# now wait for the negotiator to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		$negotiatoradr = `condor_config_val NEGOTIATOR_ADDRESS_FILE`;
		$negotiatoradr =~ s/\012+$//;
		$negotiatoradr =~ s/\015+$//;
		debug( "NEGOTIATOR_ADDRESS_FILE is <<<<<$negotiatoradr>>>>>\n");
    	debug( "We are waiting for the file to exist\n");
    	# Where is the negotiator address file? wait for it to exist
    	$havenegotiatoraddr = "no";
    	while($havenegotiatoraddr ne "yes") {
        	debug( "Looking for $negotiatoradr\n");
        	if( -f $negotiatoradr ) {
            	debug( "Found it!!!! negotiator address file\n");
            	$havenegotiatoraddr = "yes";
        	} else {
            	sleep 1;
        	}
    	}
	}

	if($daemonlist =~ /.*STARTD.*/i) {
		# now wait for the startd to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		$startdadr = `condor_config_val STARTD_ADDRESS_FILE`;
		$startdadr =~ s/\012+$//;
		$startdadr =~ s/\015+$//;
		debug( "STARTD_ADDRESS_FILE is <<<<<$startdadr>>>>>\n");
    	debug( "We are waiting for the file to exist\n");
    	# Where is the startd address file? wait for it to exist
    	$havestartdaddr = "no";
    	while($havestartdaddr ne "yes") {
        	debug( "Looking for $startdadr\n");
        	if( -f $startdadr ) {
            	debug( "Found it!!!! startd address file\n");
            	$havestartdaddr = "yes";
        	} else {
            	sleep 1;
        	}
    	}
	}

	####################################################################

	if($daemonlist =~ /.*SCHEDD.*/i) {
		# now wait for the schedd to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		$scheddadr = `condor_config_val SCHEDD_ADDRESS_FILE`;
		$scheddadr =~ s/\012+$//;
		$scheddadr =~ s/\015+$//;
		debug( "SCHEDD_ADDRESS_FILE is <<<<<$scheddadr>>>>>\n");
    	debug( "We are waiting for the file to exist\n");
    	# Where is the schedd address file? wait for it to exist
    	$havescheddaddr = "no";
    	while($havescheddaddr ne "yes") {
        	debug( "Looking for $scheddadr\n");
        	if( -f $scheddadr ) {
            	debug( "Found it!!!! schedd address file\n");
            	$havescheddaddr = "yes";
        	} else {
            	sleep 1;
        	}
    	}
	}

	if($daemonlist =~ /.*STARTD.*/i) {
		# lets wait for the collector to know about it
		# if we have a collector
		$havestartd = "";
		my $done = "no";
		$currenthost = hostfqdn();
		if(($daemonlist =~ /.*COLLECTOR.*/i) && ($personal_startup_wait eq "true")) {
			while( $done eq "no") {
				my $cmd = "condor_status -startd -format \"%s\\n\" name";
    			my $qstat = CondorTest::runCondorTool($cmd,\@status,3,"CondorPersonal");
    			if(!$qstat)
    			{
        			print "Test failure due to Condor Tool Failure<$cmd>\n";
        			last;
    			}

    			foreach $line (@status)
    			{
        			print "want $currenthost Line: $line\n";
        			if( $line =~ /^.*$currenthost.*/)
        			{
            			$done = "yes";
						print "Found startd running!\n";
        			}
    			}
				sleep 2;
			}
		}
	}

	if($daemonlist =~ /.*SCHEDD.*/i) {
		# lets wait for the collector to know about it
		# if we have a collector
		$haveschedd = "";
		my $done = "no";
		$currenthost = hostfqdn();
		if(($daemonlist =~ /.*COLLECTOR.*/i) && ($personal_startup_wait eq "true")) {
			while( $done eq "no") {
				my $cmd = "condor_status -schedd -format \"%s\\n\" name";
    			my $qstat = CondorTest::runCondorTool($cmd,\@status,3,"CondorPersonal");
    			if(!$qstat)
    			{
        			print "Test failure due to Condor Tool Failure<$cmd>\n";
        			last;
    			}

    			foreach $line (@status)
    			{
        			print "want $currenthost Line: $line\n";
        			if( $line =~ /^.*$currenthost.*/)
        			{
            			$done = "yes";
						print "Found schedd running!\n";
        			}
    			}
				sleep 2;
			}
		}
	}
	debug("Leaving IsRunningYet\n");

	return(1);
}

#################################################################
#
# IsThisWindows
#
#################################################################

sub IsThisWindows
{
    $path = CondorTest::Which("cygpath");
    print "Path return from which cygpath: $path\n";
    if($path =~ /^.*\/bin\/cygpath.*$/ ) {
        print "This IS windows\n";
        return(1);
    }
    print "This is NOT windows\n";
    return(0);
}

#################################################################
#
# FindCollectorPort
#
# 	Looks for collector_address_file via condor_config_val and tries
#		to parse port number out of the file.
#

sub FindCollectorPort
{
	my $collector_address_file = `condor_config_val collector_address_file`;
	my $line;
	CondorTest::fullchomp($collector_address_file);

	debug( "Looking for collector port in file ---$collector_address_file---\n");

	if($collector_address_file eq "") {
		debug( "No collector address file defined! Can not find port\n");
		return("0");
	}

	if( ! -e "$collector_address_file") {
		debug( "No collector address file exists! Can not find port\n");
		return("0");
	}

	open(COLLECTORADDR,"<$collector_address_file")  || die "Can not open collector address file: $!\n";
	while(<COLLECTORADDR>) {
		CondorTest::fullchomp($_);
		$line = $_;
		if( $line =~ /^\s*<(\d+\.\d+\.\d+\.\d+):(\d+)>\s*$/ ) {
			debug( "Looks like ip $1 and port $2!\n");
			return($2);
		} else {
			debug( "$line\n");
		}
	}
	close(COLLECTORADDR);
	debug( "No collector address found in collector address file! returning 0.\n");
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
	$testname = shift;
	$mypid = $$;
	$res = 1;
	$mysaveme = $testname . ".saveme";
	$res = CondorTest::verbose_system("mkdir -p $mysaveme");
	if($res != 0) {
		print "SaveMeSetup: Could not create \"saveme\" directory for test\n";
		return(0);
	}
	$mypiddir = $mysaveme . "/" . $mypid;
	# there should be no matching directory here
	# unless we are getting pid recycling. Start fresh.
	$res = CondorTest::verbose_system("rm -rf $mypiddir");
	if($res != 0) {
		print "SaveMeSetup: Could not remove prior pid directory in savemedir \n";
		return(0);
	}
	$res = CondorTest::verbose_system("mkdir $mypiddir");
	if($res != 0) {
		print "SaveMeSetup: Could not create pid directory in \"saveme\" directory\n";
		return(0);
	}
	# make a symbolic link for personal condor module to use
	# if we get pid recycling, blow the symbolic link 
	$res = CondorTest::verbose_system("rm -rf $mypid");
	if($res != 0) {
		print "SaveMeSetup: Could not remove prior pid directory\n";
		return(0);
	}
	$res = CondorTest::verbose_system("ln -s $mypiddir $mypid");
	if($res != 0) {
		print "SaveMeSetup: Could not link to pid dir in  \"saveme\" directory\n";
		return(0);
	}
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


	my $catch = "vsystem$$";
	$args = $args . " 2>" . $catch;
	my $rc = 0xffff & system $args;

	printf "system(%s) returned %#04x: ", $args, $rc;

	if ($rc == 0) 
	{
		print "ran with normal exit\n";
		system("pwd");
		return $rc;
	}
	elsif ($rc == 0xff00) 
	{
		print "command failed: $!\n";
	}
	elsif (($rc & 0xff) == 0) 
	{
		$rc >>= 8;
		print "ran with non-zero exit status $rc\n";
	}
	else 
	{
		print "ran with ";
		if ($rc &   0x80) 
		{
			$rc &= ~0x80;
			print "coredump from ";
		}
		print "signal $rc\n"
	}

	if( !open( MACH, "<$catch" )) { 
		warn "Can't look at command  output <$catch>:$!\n";
	} else {
    	while(<MACH>) {
        	print "ERROR: $_";
    	}
    	close(MACH);
	}

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
	foreach $file (readdir PD)
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
	$now = getcwd();
	chdir("$logdir");
	#system("pwd");
	#system("ls -la");
	print "\n\n******************* DUMP $logdir ******************\n\n";
	opendir LD, "." || die "failed to open . : $!\n";
	#print "Open worked.... listing follows.....\n";
	foreach $file (readdir LD)
	{
		#print "Consider: $file\n";
		next if $file =~ /^\.\.?$/; # skip . and ..
		if(-f $file ) {
			print "\n\n******************* DUMP $file ******************\n\n";
			open(FF,"<$file") || die "Can not open logfile<$file>: $!\n";
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

1;
