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

#################################################################
#
#	Parameters used within parameter config file......
#
#	Parameter		Use								Default						Variable stored in
#	----------------------------------------------------------------------------------------------
#	condortemplate	Core config file					condor_config_template		$personal_template
#	condorlocalsrc	Name for condor local config src 								$personal_local_src
#	condordaemon	daemon list to start				contents of config template $personal_daemons
#	condorconfig	Name for condor config file			condor_config				$personal_config
#	condorlocal		Name for condor local config 		condor_config.local			$personal_local
#	condor			"install" or path to tarball 									$condordistribution
#	collector	 	Used to define CONDOR_HOST										$condorhost
#	ports			Select dynamic or normal ports		dynamic						$portchanges	
#   vms				sets NUM_CPUS NUM_VMS				none
#	encryption		turns on encryption					none						comma list
#
#

require 5.0;
use Carp;
use Cwd;
use FileHandle;
use POSIX "sys_wait_h";
use Socket;
use Sys::Hostname;

my $topleveldir = getcwd();
my $home = $topleveldir;
my $localdir;
my $pid = $$;
my $version = ""; # remote, middle, ....... for naming schedd "schedd . pid . version"

BEGIN
{
    %personal_condor_params;
    %personal_config_changes;
	$personal_config = "condor_config";
	$personal_template = "condor_config_template";
	$personal_daemons = "";
	$personal_local = "condor_config.local";
	$personal_local_src = "";
	$portchanges = "dynamic";
	$DEBUG = 0;
	$collector_port = "0";
	$personal_config_file = "";
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
	my $paramfile = shift || die "Missing parameter file!\n";
	my $config_and_port = "";

	$version = shift || die "Missing parameter version!\n";

	system("mkdir -p $topleveldir");
	$topleveldir = $topleveldir . "/" . $pid;
	system("mkdir -p $topleveldir");
	$topleveldir = $topleveldir . "/" . $pid . $version;
	system("mkdir -p $topleveldir");

	$personal_config_file = $topleveldir ."/condor_config";

	CondorPersonal::ParsePersonalCondorParams($paramfile);

	$localdir = CondorPersonal::InstallPersonalCondor();
	if($localdir eq "")
	{
		return("Failed to do needed Condor Install\n");
	}

	CondorPersonal::TunePersonalCondor($localdir);

	$collector_port = CondorPersonal::StartPersonalCondor();

	debug( "collector port is $collector_port\n");
	$config_and_port = $personal_config_file . ":" . $collector_port;
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

	$topleveldir = getcwd();
	$home = $topleveldir;
	$portchanges = "dynamic";
	$collector_port = "0";
	$personal_config_file = "";
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
	chomp;
	$line++;

	# skip comments & blank lines
	next if /^#/ || /^\s*$/;

	# if this line is a variable assignment...
	if( /^(\w+)\s*\=\s*(.*)$/ )
	{
	    $variable = lc $1;
	    $value = $2;

	    # if line ends with a continuation ('\')...
	    while( $value =~ /\\\s*$/ )
	    {
		# remove the continuation
		$value =~ s/\\\s*$//;

		# read the next line and append it
		<SUBMIT_FILE> || last;
		$value .= $_;
	    }

	    # compress whitespace and remove trailing newline for readability
	    $value =~ s/\s+/ /g;
	    chomp $value;

	
		# Do proper environment substitution
	    if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ )
	    {
			my $envlookup = $ENV{$2};
	    	#debug( "Found $envlookup in environment \n");
			$value = $1.$envlookup.$3;
	    }

	    debug( "(CondorPersonal.pm) $variable = $value\n" );
	    
	    # save the variable/value pair
	    $personal_condor_params{$variable} = $value;
	}
	else
	{
#	    debug( "line $line of $submit_file not a variable assignment... " .
#		   "skipping\n" );
	}
    }
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
		chomp();
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
	my $condorq = `which condor_q`;
	my $sbinloc;
	my $configline = "";
	my @configfiles;

	if( exists $control{"condor"} )
	{
		$condordistribution = $control{"condor"};
		debug( "Install this condor --$condordistribution--\n");
		if( $condordistribution eq "install" )
		{
			# where is the hosting condor_config file? The one assumed to be based
			# on a setup with condor_configure.
			open(CONFIG,"condor_config_val -config | ") || die "Can not find config file: $!\n";
			while(<CONFIG>)
			{
				chomp;
				$configline = $_;
				push @configfiles, $configline;
			}
			close(CONFIG);
			$personal_condor_params{"condortemplate"} = shift @configfiles;
			$personal_condor_params{"condorlocalsrc"} = shift @configfiles;
			#
			#debug( "My path to condor_q is $condorq and topleveldir is $topleveldir\n");

			if( $condorq =~ /^(\/.*\/)(\w+)\s*$/ )
			{
				debug( "Root path $1 and base $2\n");
				$binloc = $1;	# we'll get our binaries here.
			}
			else
			{
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

			$schedd = $sbinloc . "sbin/". "condor_schedd";
			$master = $sbinloc . "sbin/". "condor_master";
			$collector = $sbinloc . "sbin/". "condor_collector";
			$submit = $binloc . "condor_submit";
			$startd = $sbinloc . "sbin/". "condor_startd";
			$negotiator = $sbinloc . "sbin/". "condor_negotiator";

			debug( "$schedd $master $collector $submit $startd $negotiator\n");


			debug( "Sandbox started rooted here: $topleveldir\n");

			#system("mkdir -p $topleveldir/sbin");
			#system("mkdir -p $topleveldir/bin");
			#system("mkdir -p $topleveldir/lib");
			system("mkdir -p $topleveldir/execute");
			system("mkdir -p $topleveldir/spool");
			system("mkdir -p $topleveldir/log");


			# now install condor

			#system("cp $sbinloc/sbin/* $topleveldir/sbin");
			#system("cp $binloc/* $topleveldir/bin");
			#system("cp $sbinloc/lib/* $topleveldir/lib");
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
	print "InstallPersonalCondor returning $sbinloc for LOCAL_DIR setting\n";
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
	my $condorhost = `/bin/hostname`;
	my $localdir = shift;

	print "TunePersonalCondor setting LOCAL_DIR to $localdir\n";

	chomp($condorhost);

	# was a special collector called out?
	if( exists $control{"collector"} )
	{
		$condorhost = $control{"collector"};
	}

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

	#debug( "Proto file is --$personal_template--\n");

	$personalmaster = "$topleveldir/sbin/condor_master";

	#filter fig file storing entries we set so we can test
	#for completeness when we are done

	my $line;
	#system("ls;pwd");
	#print "***************** opening $personal_template as config file template *****************\n";
	open(TEMPLATE,"<$personal_template")  || die "Can not open template: $!\n";
	debug( "want to open new config file as $topleveldir/$personal_config\n");
	open(NEW,">$topleveldir/$personal_config") || die "Can not open new config file: $!\n";
	while(<TEMPLATE>)
	{
		$line = $_;
		chomp($line);
		if( $line =~ /^RELEASE_DIR\s*=.*/ )
		{
			#debug( "-----------$line-----------\n");
			$personal_config_changes{"RELEASE_DIR"} = "RELEASE_DIR = $localdir\n";
			print NEW "RELEASE_DIR = $localdir\n";
		}
		elsif( $line =~ /^LOCAL_DIR\s*=.*/ )
		{
			#debug( "-----------$line-----------\n");
			$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $topleveldir\n";
			print NEW "LOCAL_DIR = $topleveldir\n";
		}
		elsif( $line =~ /^LOCAL_CONFIG_FILE\s*=.*/ )
		{
			#debug( "-----------$line-----------\n");
			$personal_config_changes{"LOCAL_DIR"} = "LOCAL_DIR = $topleveldir\n";
			print NEW "LOCAL_CONFIG_FILE = $topleveldir/$personal_local\n";
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
			chomp;
			$line = $_;
			print NEW "$line\n";
		}
		# now make sure we have the local dir we want after the generic .local file is seeded in
		$line = $personal_config_changes{"LOCAL_DIR"};
		print NEW "$line\n";
		# and a lock file we like
		print NEW "LOCK = \$(LOG)/main_lock\n";
		close(LOCSRC);
	}

	if($personal_daemons ne "")
	{
		# Allow the collector to run on the default and expected port as the main
		# condor install on this system.
		print NEW "DAEMON_LIST = $personal_daemons\n";
	}

	if( $portchanges eq "dynamic")
	{
		# this variation requests a dynamic port for collector and negotiator
		# and the location where we can look up the adresses.
		print NEW "SCHEDD_NAME = schedd$pid$version\n";
		print NEW "COLLECTOR_HOST = \$(CONDOR_HOST):0\n";
		print NEW "NEGOTIATOR_HOST = \$(CONDOR_HOST):0\n";
		print NEW "COLLECTOR_ADDRESS_FILE = \$(LOG)/.collector_address\n";
		print NEW "NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address\n";
		print NEW "CONDOR_HOST = $condorhost\n";
		print NEW "START = TRUE\n";
		print NEW "SCHEDD_INTERVAL = 30\n";
		print NEW "UPDATE_INTERVAL = 30\n";

		print NEW "# Condor-C things\n";
		print NEW "CONDOR_GAHP = \$(SBIN)/condor_c-gahp\n";
		print NEW "C_GAHP_LOG = \$(LOG)/CondorCgahp.\$(USERNAME)\n";
		print NEW "C_GAHP_WORKER_THREAD_LOG = \$(LOG)/CondorCgahpWThread.\$(USERNAME)\n";
	}
	elsif( $portchanges eq "standard" )
	{
		# Allow the collector to run on the default and expected port as the main
		# condor install on this system.
		print NEW "COLLECTOR_HOST = \$(CONDOR_HOST)\n";
		print NEW "NEGOTIATOR_HOST = \$(CONDOR_HOST)\n";
		print NEW "CONDOR_HOST = $condorhost\n";
		print NEW "START = TRUE\n";
		print NEW "SCHEDD_INTERVAL = 30\n";
		print NEW "UPDATE_INTERVAL = 30\n";

		print NEW "# Condor-C things\n";
		print NEW "CONDOR_GAHP = \$(SBIN)/condor_c-gahp\n";
		print NEW "C_GAHP_LOG = \$(LOG)/CondorCgahp.\$(USERNAME)\n";
		print NEW "C_GAHP_WORKER_THREAD_LOG = \$(LOG)/CondorCgahpWThread.\$(USERNAME)\n";
	}
	else
	{
		die "Misdirected request for ports\n";
		exit(1);
	}

	# now we consider configuration requests

	if( exists $control{"vms"} )
	{
		my $myvms = $control{"vms"};
		debug( "VMs wanted! Number = $myvms\n");
		print NEW "NUM_CPUS = $myvms\n";
		print NEW "NUM_VIRTUAL_MACHINES = $myvms\n";
		$gendatafile = $gendatafile . "_" . $myvms . "VMs";
	}

	if( exists $control{"encryption"} )
	{
		my $myencryptions = $control{"encryption"};
		debug( "Encryption wanted! methods = $myencryptions\n");
		print NEW "SEC_DEFAULT_ENCRYPTION = REQUIRED\n";
		print NEW "SEC_DEFAULT_CRYPTO_METHODS = $myencryptions\n";
		$gendatafile = $gendatafile . "_" . $myencryptions;
	}

	if( exists $control{"authentication"} )
	{
		my $myauthentication = $control{"authentication"};
		if($myauthentication eq "GSI")
		{
			debug( "Setting up authentication");
			print NEW "SEC_DEFAULT_AUTHENTICATION = REQUIRED\n";
			print NEW "SEC_DEFAULT_AUTHENTICATION_METHODS = $myauthentication\n";
			print NEW "GSI_DAEMON_DIRECTORY = /etc/grid-security\n";
			print NEW "#Subjecct must be in slash form and associate with condor entry in grid-mapfile\n";
			print NEW "GSI_DAEMON_NAME = /C=US/ST=Wisconsin/L=Madison/O=University of Wisconsin -- Madison/O=Computer Sciences Department/OU=Condor Project/CN=nmi-test-5.cs.wisc.edu/Email=bgietzel@cs.wisc.edu\n";
			$gendatafile = $gendatafile . "_" . $myauthenication;
		}
		else
		{
			debug( "Do not understand requested authentication");
		}
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
	my $personalmaster = "$localdir/sbin/condor_master";

	my $configfile = $control{"condorconfig"};
	my $fullconfig = "$topleveldir/$configfile";
	my $oldpath = $ENV{PATH};
	my $newpath = "$localdir/sbin:$localdir/bin:" . "$oldpath";
	$ENV{PATH} = $newpath;

	debug( "Using this path: --$newpath--\n");

	#debug( "Want $configfile for config file\n");

	#set up to use the existing generated configfile
	$ENV{CONDOR_CONFIG} = $fullconfig;
	my $condorstate = WhichCondorConfig($fullconfig);
	debug( "Condor state is $condorstate\n");
	my $fig = $ENV{CONDOR_CONFIG};
	debug( "Condor_config from environment is --$fig--\n");

	# At the momment we only restart/start a personal we just configured 
	# or reconfigured

	if( $condorstate eq "lost" )
	{
		# probably not running with this config so treat it like a start case
		#die "Should be set with a new config file but LOST!\n";
		debug( "start up the personal condor!--$personalmaster--\n");
		system("$personalmaster");
		system("condor_config_val -v log");
	}
	elsif( $condorstate eq "matched not running" )
	{
		debug( "start up the personal condor!--$personalmaster--\n");
		system("$personalmaster");
		system("condor_config_val -v log");
	}
	elsif( $condorstate eq "matched running" )
	{
		debug( "restart the personal condor!\n");
		system("condor_off -master");
		sleep 5;
		system("$personalmaster");
	}
	else
	{
		die "Bad state for a new personal condor configuration!\n";
	}

	# if this was a dynamic port startup, return the port which
	# the collector is listening on...

	if( $portchanges eq "dynamic" )
	{
		sleep 6;
		return( FindCollectorPort() );
	}
	else
	{
		return("0");
	}
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
	chomp($collector_address_file);

	debug( "Looking for collector port in file ---$collector_address_file---\n");

	if($collector_address_file eq "")
	{
		debug( "No collector address file defined! Can not find port\n");
		return("0");
	}

	if( ! -e "$collector_address_file")
	{
		debug( "No collector address file exists! Can not find port\n");
		return("0");
	}

	open(COLLECTORADDR,"<$collector_address_file")  || die "Can not open collector address file: $!\n";
	while(<COLLECTORADDR>)
	{
		chomp($_);
		$line = $_;
		if( $line =~ /^\s*<(\d+\.\d+\.\d+\.\d+):(\d+)>\s*$/ )
		{
			debug( "Looks like ip $1 and port $2!\n");
			return($2);
		}
		else
		{
			debug( "$line\n");
		}
	}
	debug( "No collector address found in collector address file! returning 0.\n");
	return("0");
}
1;
