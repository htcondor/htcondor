#!/usr/bin/env pere
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

use strict;
use warnings;
use Cwd;
use File::Spec;
#use CondorTest;
#use CondorUtils;

package TestGlue;

use Net::Domain qw(hostfqdn);

my $installdir = "";
my $wininstalldir = "";
my $initialconfig = "";

my $iswindows = is_windows();
my $iscygwin  = is_cygwin_perl();
my $configmain = "condor_examples/condor_config.generic";
my $configlocal = "condor_examples/condor_config.local.central.manager";
my $targetconfig = "";
my $targetconfiglocal = "";
my $want_core_dumps = 1;

BEGIN
{
}

sub print_debug_header {
    my $cwd = Cwd::getcwd();
    
    print "----------- Debug Header ----------------\n";
    print "Current time: " . scalar(localtime) . "\n";
    print "Current host: " . `/bin/hostname -f`;
    print "CWD: $cwd\n";
    print "Perl path: $^X\n";
    print "Perl version: $]\n";
    print "Uptime: " . `uptime`;
	print "PATH:$ENV{PATH}\n";
    dir_listing(".");
    print "---------- End Debug Header --------------\n";
}

sub setup_test_environment {

    my $base_dir = Cwd::getcwd();

    if( not is_windows() ) {
        set_env("BASE_DIR", $base_dir);
        set_env("PATH", "$base_dir/nmi_tools/glue/test:$base_dir/condor/bin:$base_dir/condor/sbin:$ENV{PATH}");
		set_env("TESTS","$base_dir/condor_tests");
        set_env("CONDOR_CONFIG", "$base_dir/condor_tests/Config/condor_config");
		if(exists $ENV{LD_LIBRARY_PATH}) {
			set_env("LD_LIBRARY_PATH","$ENV{LD_LIBRARY_PATH}:$base_dir/condor/libexec:$base_dir/condor/lib:$base_dir/condor/lib/python");
		} else {
			set_env("LD_LIBRARY_PATH","$base_dir/condor/libexec:$base_dir/condor/lib:$base_dir/condor/lib/python");
		}
    }
    else {
        # Get the right slashes for Windows.  Apparently getcwd() returns forward slashes, even
        # on Windows.
        $base_dir =~ s|/|\\|g;
        set_env("BASE_DIR", $base_dir);

        # Create the Windows PATH
        my $front_path = "$base_dir\\nmi_tools\\glue\\test;$base_dir\\condor\\bin";

        # We installed some tools (unzip, tar) in C:\tools on our Windows NMI machines
        my $end_path = "C:\\tools";

        # We need to add Perl to the path (this works only in old-batlab)
        #$end_path .= ";C:\\prereq\\ActivePerl-5.10.1\\bin";

        my $force_cygwin = 1; # set to 0 to work on removing cygwin dependancies from tests.
        if ( $force_cygwin ) {
            if ( ! ($ENV{PATH} =~ /cygwin/i) ) {
                $front_path .= ";C:\\cygwin\\bin";
            }
        }

        # If we are not using cygwin perl, we need to have the msconfig tools in the path.
        if ( is_windows_native_perl() ) { $end_path = "$base_dir\\msconfig;$end_path"; }

        # Windows requires the SystemRoot directory to the PATH.  This is generally C:\Windows.
        # Also, add the system32 subdirectory in this folder
        #my $system_paths = "$ENV{SystemRoot};" . File::Spec->catdir($ENV{SystemRoot}, "system32");
        #$front_path .= ";$ENV{SystemRoot}";
        #$front_path .= ";" . File::Spec->catdir($ENV{SystemRoot}, "system32");

        set_env("PATH", "$front_path;$ENV{PATH};$end_path");

		print "^^^^^^ Windows path set to:$ENV{PATH} ^^^^^^^^^^^^^^^^\n";

        # Condor will want Win32-style paths for CONDOR_CONFIG
		set_env("TESTS","$base_dir\\condor_tests");
        set_env("CONDOR_CONFIG", "$base_dir\\condor_tests\\Config\\condor_config");
        
        # also, throw in the WIN32 version of the base directory path for later use
        set_env("WIN32_BASE_DIR", $base_dir);

        # full environment dump to help debugging
        if ( ! $force_cygwin ) {
            print "----------------------------------\n";
            print "Dumping environment:\n";
            system("set");
            print "----------------------------------\n\n";
        }
    }
}

sub prepare_for_configs {
	my $base_dir = shift;
	if(($iswindows) && (!$iscygwin)) {
		$initialconfig = "$base_dir\\condor_tests\\Config";
		$targetconfig = "$base_dir\\condor_tests\\Config\\condor_config";
		$targetconfiglocal = "$base_dir\\condor_tests\\Config\\condor_config.local";
	} else {
		$initialconfig = "$base_dir/condor_tests/Config";
		$targetconfig = "$base_dir/condor_tests/Config/condor_config";
		$targetconfiglocal = "$base_dir/condor_tests/Config/condor_config.local";
	}
	if(!(-d $initialconfig)) {
		#print "Calling create_initial_configs($initialconfig)\n";
		create_initial_configs($initialconfig,$base_dir);
	}
}

sub create_initial_configs {
	my $configlocation = shift;
	my $base_dir = shift;

	my $awkscript = "";
	my $genericconfig = "";
	my $genericlocalconfig = "";
	if(($iswindows) && (!$iscygwin)) {
		$awkscript = "condor_examples\\convert_config_to_win32.awk";
		$genericconfig = "condor_examples\\condor_config.generic";
		$genericlocalconfig = "condor_examples\\condor_config.local.central.manager";
	} else {
		$awkscript = "condor_examples/convert_config_to_win32.awk";
		$genericconfig = "condor_examples/condor_config.generic";
		$genericlocalconfig = "condor_examples/condor_config.local.central.manager";
	}
	
	if( -d $configlocation ) {
		#print "Config Directory Established prior\n";
	} else {
		#print "Test Personal Condor Directory being Established now\n";
		CreateDir("-p $configlocation");
		print "Showing Config folder\n";
		dir_listing($configlocation);
	}
        
	WhereIsInstallDir($base_dir);
        
	print "Need to set up config files for tests\n";
	CreateConfig($awkscript, $genericconfig);
	print "Showing Config folder\n";
	dir_listing($configlocation);
	CreateLocalConfig($awkscript, $genericlocalconfig);
	print "Showing Config folder\n";
	dir_listing($configlocation);
	#CreateLocal(); We are never going to run a condor here, just need a base set
	# of config files.
}

sub WhereIsInstallDir {
	my $base_dir = shift;
	my $top = $base_dir;
	if($iswindows == 1) {
		my $top = Cwd::getcwd();
		print "base_dir is \"$top\"\n";
		if ($iscygwin) {
			my $crunched = `cygpath -m $top`;
			fullchomp($crunched);
			print "cygpath changed it to: \"$crunched\"\n";
			my $ppwwdd = `pwd`;
			print "pwd says: $ppwwdd\n";
		} else {
			my $ppwwdd = `cd`;
			print"cd says: $ppwwdd\n";
		}
	}

	my $master_name = "condor_master"; if ($iswindows) { $master_name = "condor_master.exe"; }
	my $tmp = Which($master_name);
	if ( ! ($tmp =~ /condor_master/ ) ) {
		print STDERR "Which($master_name) returned:$tmp\n";
		print STDERR "Unable to find a $master_name in your PATH!\n";
		system("ls build");
		print "PATH: $ENV{PATH}\n";
		exit(1);
	} else {
		print "Found master via Which here:$tmp\n";
	}
	fullchomp($tmp);
	print "Install Directory \"$tmp\"\n";
	if ($iswindows) {
		if ($iscygwin) {
			$tmp =~ s|\\|/|g; # convert backslashes to forward slashes.
			if($tmp =~ /^(.*)\/bin\/condor_master.exe\s*$/) {
				$installdir = $1;
				$tmp = `cygpath -m $1`;
				fullchomp($tmp);
				$wininstalldir = $tmp;
			}
		} else {
			print "$tmp before trying to remove bin\condor_master.exe\n";
			$tmp =~ s/\\bin\\condor_master.exe$//i;
			$installdir = $tmp;
			$wininstalldir = $tmp;
			print "$tmp after trying to remove bin\condor_master.exe\n";
			$tmp =~ s/\\bin\\condor_master.exe$//i;
		}
		$wininstalldir =~ s|/|\\|g; # forward slashes.to backslashes
		$installdir =~ s|\\|/|g; # convert backslashes to forward slashes.
		print "Testing this Install Directory: \"$wininstalldir\"\n";
		print "Installdir:$installdir\n";
	} else {
		$tmp =~ s|//|/|g;
		if( ($tmp =~ /^(.*)\/sbin\/condor_master\s*$/) || \
				($tmp =~ /^(.*)\/bin\/condor_master\s*$/) ) {
			$installdir = $1;
			print "Testing This Install Directory: \"$installdir\"\n";
		} else {
			die "'$tmp' didn't match path RE\n";
		}
		if(defined $ENV{LD_LIBRARY_PATH}) {
			$ENV{LD_LIBRARY_PATH} = "$installdir/lib:$ENV{LD_LIBRARY_PATH}";
		} else {
			$ENV{LD_LIBRARY_PATH} = "$installdir/lib";
		}
		if(defined $ENV{PYTHONPATH}) {
			$ENV{PYTHONPATH} = "$installdir/lib/python:$ENV{PYTHONPATH}";
		} else {
			$ENV{PYTHONPATH} = "$installdir/lib/python";
		}
	}
}

# Not called, never going to run a level 1 condor. Jobs will have their own.
sub CreateLocal
{
	if(($iswindows) &&(!$iscygwin)) {
		if( !(-d "$initialconfig\\local")) {
			CreateDir( "$initialconfig\\local") 
				|| die "Can't mkdir $initialconfig\\local: $!\n";
		}
		if( !(-d "$initialconfig\\local\\spool")) {
			CreateDir( "$initialconfig\\local\\spool") 
				|| die "Can't mkdir $initialconfig\\local\\spool: $!\n";
		}
		if( !(-d "$initialconfig\\local\execute")) {
			CreateDir( "$initialconfig\\local\\execute") 
				|| die "Can't mkdir $initialconfig\\local\\execute: $!\n";
		}
		if( !(-d "$initialconfig\\local\\log")) {
			CreateDir( "$initialconfig\\local\\log") 
				|| die "Can't mkdir $initialconfig\\local\\log: $!\n";
		}
		if( !(-d "$initialconfig\\local\\log\\tmp")) {
			CreateDir( "$initialconfig\\local\\log\\tmp") 
				|| die "Can't mkdir $initialconfig\\local\\log: $!\n";
		}
	} else {
		if( !(-d "$initialconfig/local")) {
			mkdir( "$initialconfig/local", 0777 ) 
				|| die "Can't mkdir $initialconfig/local: $!\n";
		}
		if( !(-d "$initialconfig/local/spool")) {
			mkdir( "$initialconfig/local/spool", 0777 ) 
				|| die "Can't mkdir $initialconfig/local/spool: $!\n";
		}
		if( !(-d "$initialconfig/local/execute")) {
			mkdir( "$initialconfig/local/execute", 0777 ) 
				|| die "Can't mkdir $initialconfig/local/execute: $!\n";
		}
		if( !(-d "$initialconfig/local/log")) {
			mkdir( "$initialconfig/local/log", 0777 ) 
				|| die "Can't mkdir $initialconfig/local/log: $!\n";
		}
		if( !(-d "$initialconfig/local/log/tmp")) {
			mkdir( "$initialconfig/local/log/tmp", 0777 ) 
				|| die "Can't mkdir $initialconfig/local/log: $!\n";
		}
	}
}

sub CreateConfig {
	my ($awkscript, $genericconfig) = @_;

	# The only change we need to make to the generic configuration
	# file is to set the release-dir and local-dir. (non-windows)
	# change RELEASE_DIR and LOCAL_DIR    
	my $currenthost = getFqdnHost();
	#function above chomps fullchomp($currenthost);

	#print "Set RELEASE_DIR and LOCAL_DIR\n";

	# Windows needs config file preparation, wrapper scripts etc
	if($iswindows == 1) {
		# pre-process config file src and windowize it
		# create config file with todd's awk script
		my $configcmd = "gawk -f $awkscript $genericconfig";
		if ($^O =~ /MSWin32/) {  $configcmd =~ s/gawk/awk/; $configcmd =~ s|/|\\|g; }
		print "awk cmd is $configcmd\n";

		Which("awk");
		open(OLDFIG, " $configcmd 2>&1 |") || die "Can't run script file\"$configcmd\": $!\n";    
	} else {
		open(OLDFIG, '<', $configmain ) || die "Can't read base config file $configmain: $!\n";
	}

	open(NEWFIG, '>', $targetconfig ) || die "Can't write to new config file $targetconfig: $!\n";    
	print "New config file open......\n";
	while( <OLDFIG> ) {
		fullchomp($_);        
		if(/^RELEASE_DIR\s*=/) {
			#print "Matching:$_\n";
			if($iswindows == 1) {
				print NEWFIG "RELEASE_DIR = $wininstalldir\n";
			}
			else {
				print NEWFIG "RELEASE_DIR = $installdir\n";
			}
		}
		#elsif(/^LOCAL_DIR\s*=/) {
			#debug("Matching:$_\n", 2);
			#if($iswindows == 1) {
				#print NEWFIG "LOCAL_DIR = $initialconfig/local\n";
			#}
			#else {
				#print NEWFIG "LOCAL_DIR = $localdir\n";        
			#}
		#}
		elsif(/^LOCAL_CONFIG_FILE\s*=/) {
			#print "Matching:$_\n";
			if($iswindows == 1) {
				print NEWFIG "LOCAL_DIR = $initialconfig\n";
				print NEWFIG "LOCAL_CONFIG_FILE = $initialconfig\\condor_config.local\n";
			}
			else {
				print NEWFIG "LOCAL_DIR = $initialconfig\n";
				print NEWFIG "LOCAL_CONFIG_FILE = $initialconfig/condor_config.local\n";
			}
		}
		elsif(/^LOCAL_CONFIG_DIR\s*=/) {
			# we don't want this
		}
		elsif(/^CONDOR_HOST\s*=/) {
			#print "Matching:$_\n";
			print NEWFIG "CONDOR_HOST = $currenthost\n";
		}
		elsif(/^ALLOW_WRITE\s*=/) {
			#print "Matching:$_\n";
			print NEWFIG "ALLOW_WRITE = *\n";
		}
		elsif(/NOT_RESPONDING_WANT_CORE\s*=/ and $want_core_dumps ) {
			#print "Matching:$_\n";
			print NEWFIG "NOT_RESPONDING_WANT_CORE = True\n";
		}
		elsif(/CREATE_CORE_FILES\s*=/ and $want_core_dumps ) {
			#print "Matching:$_\n";
			print NEWFIG "CREATE_CORE_FILES = True\n";
		}
		else {
			print NEWFIG "$_\n";
		}    
	}    
	close( OLDFIG );    
	print NEWFIG "TOOL_TIMEOUT_MULTIPLIER = 10\n";
	print NEWFIG "TOOL_DEBUG_ON_ERROR = D_ANY D_ALWAYS:2\n";
	close( NEWFIG );
}

sub CreateLocalConfig {
	my ($awkscript, $genericlocalconfig) = @_;

	my $socketdir = "";
	if($iswindows == 1) {
        # windows does not have a path length limit
	} else {
		if(!(-f "condor_tests/SOCKETDIR")) {
            print "Why can I not find condor_tests/SOCKETDIR?\n";
			print "Where am I?\n";
			system("pwd");
        } else {
            open(SD,"<condor_tests/SOCKETDIR") or print "Failed to open:condor_tests/SOCKETDIR:$!\n";
            $socketdir = (<SD>);
            chomp($socketdir);
            print "Fetch master condor_tests/SOCKETDIR:$socketdir\n";
            $socketdir = "$socketdir" . "/$$";
            print "This tests socketdir:$socketdir\n";
        }
	}

	#print "Modifying local config file\n";
	my $logsize = 50000000;

	# make sure ports for Personal Condor are valid, we'll use address
	# files and port = 0 for dynamic ports...
	if($iswindows == 1) {
	# create config file with todd's awk script
		my $configcmd = "gawk -f $awkscript $genericlocalconfig";
		if ($^O =~ /MSWin32/) {  $configcmd =~ s/gawk/awk/; $configcmd =~ s|/|\\|g; }
		print "gawk cmd is $configcmd\n";

		open( ORIG, " $configcmd 2>&1 |")
			|| die "Can't run script file\"$configcmd\": $!\n";    

	} else {
		open( ORIG, "<$configlocal" ) ||
			die "Can't open $configlocal: $!\n";
	}
	open( FIX, ">$targetconfiglocal" ) ||
		die "Can't open $targetconfiglocal: $!\n";

	while( <ORIG> ) {
		print FIX;
	}
	close ORIG;

	print FIX "COLLECTOR_HOST = \$(CONDOR_HOST):0\n";
	print FIX "COLLECTOR_ADDRESS_FILE = \$(LOG)/.collector_address\n";
	print FIX "NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address\n";
	print FIX "MASTER_ADDRESS_FILE = \$(LOG)/.master_address\n";
	print FIX "STARTD_ADDRESS_FILE = \$(LOG)/.startd_address\n";
	print FIX "SCHEDD_ADDRESS_FILE = \$(LOG)/.schedd_address\n";

	# ADD size for log files and debug level
	# default settings are in condor_config, set here to override 
	#print FIX "ALL_DEBUG               = D_FULLDEBUG D_SECURITY D_HOSTNAME\n";
	#print FIX "DEFAULT_DEBUG               = D_FULLDEBUG D_HOSTNAME\n";

	print FIX "MAX_COLLECTOR_LOG       = $logsize\n";
	print FIX "COLLECTOR_DEBUG         = \n";

	print FIX "MAX_KBDD_LOG            = $logsize\n";
	print FIX "KBDD_DEBUG              = \n";

	print FIX "MAX_NEGOTIATOR_LOG      = $logsize\n";
	print FIX "NEGOTIATOR_DEBUG        = D_MATCH\n";
	print FIX "MAX_NEGOTIATOR_MATCH_LOG = $logsize\n";

	print FIX "MAX_SCHEDD_LOG          = 50000000\n";
	print FIX "SCHEDD_DEBUG            = D_COMMAND\n";

	print FIX "MAX_SHADOW_LOG          = $logsize\n";
	print FIX "SHADOW_DEBUG            = D_FULLDEBUG\n";

	print FIX "MAX_STARTD_LOG          = $logsize\n";
	print FIX "STARTD_DEBUG            = D_COMMAND\n";

	print FIX "MAX_STARTER_LOG         = $logsize\n";

	print FIX "MAX_MASTER_LOG          = $logsize\n";
	print FIX "MASTER_DEBUG            = D_COMMAND\n";

	print FIX "EVENT_LOG               = \$(LOG)/EventLog\n";
	print FIX "EVENT_LOG_MAX_SIZE      = $logsize\n";

	if($iswindows == 1) {
		print FIX "WINDOWS_SOFTKILL_LOG = \$(LOG)\\SoftKillLog\n";
	}

	# Add a shorter check time for periodic policy issues
	print FIX "PERIODIC_EXPR_INTERVAL = 15\n";
	print FIX "PERIODIC_EXPR_TIMESLICE = .95\n";
	print FIX "NEGOTIATOR_INTERVAL = 20\n";
	print FIX "DAGMAN_USER_LOG_SCAN_INTERVAL = 1\n";

	# turn on soap for testing
	print FIX "ENABLE_SOAP            	= TRUE\n";
	print FIX "ALLOW_SOAP            	= *\n";
	print FIX "QUEUE_ALL_USERS_TRUSTED 	= TRUE\n";

	# condor_config.generic now contains a special value
	# for ALLOW_WRITE which causes it to EXCEPT on submit
	# till set to some legal value. Old was most insecure..
	print FIX "ALLOW_WRITE 			= *\n";
	print FIX "LOCAL_CONFIG_DIR 			= \n";
	print FIX "NUM_CPUS 			= 15\n";

	if($iswindows == 1) {
		print FIX "JOB_INHERITS_STARTER_ENVIRONMENT = TRUE\n";
	}
	# Allow a default heap size for java(addresses issues on x86_rhas_3)
	# May address some of the other machines with Java turned off also
	print FIX "JAVA_MAXHEAP_ARGUMENT = \n";

	# don't run benchmarks
	print FIX "RunBenchmarks = false\n";
	print FIX "JAVA_BENCHMARK_TIME = 0\n";


	my $jvm = "";
	my $java_libdir = "";
	my $exec_result;
	my $javabinary = "";
	my @whereresponse = ();
	my $wherecount = 0;
	my @pathsearch = ();
	my $pathcount = 0;
	if($iswindows == 1) {

		$javabinary = "java.exe";
		if ($^O =~ /MSWin32/) {
			#$jvm = `\@for \%I in ($javabinary) do \@echo(\%~sf\$PATH:I`;
			print "Windows native perl\n";
			@pathsearch = `\@for \%I in ($javabinary) do \@echo(\%~sf\$PATH:I`;
			$pathcount = @pathsearch;
			print "FOR search count:$pathcount\n";
			foreach my $targ (@pathsearch) {
				print "FOR:$targ\n";
			}
			if($pathcount > 1) {
				print "Native windows search through path returned multiple choices:$pathcount\n";
				foreach my $choice (@pathsearch) {
					if($choice =~ /sysnative/) {
						print "Found sysnative\n";
						$jvm = $choice;
					}
				}
				if($jvm eq "") {
					$jvm = $pathsearch[0];
				}
			} else {
				$jvm = $pathsearch[0];
			}
			fullchomp($jvm);
		} else {
			#can't use which. its a linux tool and will lie about the path to java.
			if (1) {
				print "Running where $javabinary\n";
				@whereresponse= `where $javabinary`;
				$wherecount = @whereresponse;
				print "where search count:$wherecount\n";
				foreach my $targ (@whereresponse) {
					print "WHERE:$targ\n";
				}
				if($wherecount > 1) {
				print "Running where returned multiple choices:$wherecount\n";
					foreach my $choice (@whereresponse) {
						if($choice =~ /sysnative/) {
							print "Found sysnative\n";
							$jvm = $choice;
						}
					}
					if($jvm eq "") {
						$jvm = $whereresponse[0];
					}
				} else {
					$jvm = $whereresponse[0];
				}

				fullchomp($jvm);
				# if where doesn't tell us the location of the java binary, just assume it's will be
				# in the path once condor is running. (remember that cygwin lies...)
				if ( ! ($jvm =~ /java/i)) {
					# we need a special check for 64bit java if we are a 32 bit app.
					if ( -e '/cygdrive/c/windows/sysnative/java.exe') {
						print "where $javabinary returned nothing, but found 64bit java in sysnative dir\n";
						$jvm = "c:\\windows\\sysnative\\java.exe";
					} else {
					print "where $javabinary returned nothing, assuming java will be in Condor's path.\n";
					$jvm = "java.exe";
					}
				}
			} else {
				my $whichtest = `which $javabinary`;
				fullchomp($whichtest);
				$whichtest =~ s/Program Files/progra~1/g;
				$jvm = `cygpath -m $whichtest`;
				fullchomp($jvm);
			}
		}
		#print "which java said: $jvm\n";

		$java_libdir = "$wininstalldir/lib";

	} else {
		# below stolen from condor_configure

		my @default_jvm_locations = ("/bin/java",
			"/usr/bin/java",
			"/usr/local/bin/java",
			"/s/std/bin/java");

		$javabinary = "java";
		unless (system ("which java >> /dev/null 2>&1")) {
			fullchomp(my $which_java = Which("$javabinary"));
			#print "CT::Which for $javabinary said $which_java\n";
			@default_jvm_locations = ($which_java, @default_jvm_locations) unless ($?);
		}

		$java_libdir = "$installdir/lib";

		# check some default locations for java and pick first valid one
		foreach my $default_jvm_location (@default_jvm_locations) {
			#print "default_jvm_location is:$default_jvm_location\n";
			if ( -f $default_jvm_location && -x $default_jvm_location) {
				$jvm = $default_jvm_location;
				print "Set JAVA to $jvm\n";
				last;
			}
		}
	}
	# if nothing is found, explain that, otherwise see if they just want to
	# accept what I found.
	print "Setting JAVA=$jvm\n";
	# Now that we have an executable JVM, see if it is a Sun jvm because that
	# JVM it supports the -Xmx argument then, which is used to specify the
	# maximum size to which the heap can grow.

	# execute a program in the condor lib directory that just got installed.
	# We are going to pass an -Xmx flag to it and see if we have a Sun JVM,
	# if so, mark that fact for the config file.

	my $tmp = $ENV{"CLASSPATH"} || undef;   # save CLASSPATH environment
	my $java_jvm_maxmem_arg = "";

	$ENV{"CLASSPATH"} = $java_libdir;
	$exec_result = 0xffff &
		system("$jvm -Xmx1024m CondorJavaInfo new 0 > /dev/null 2>&1");
	if ($tmp) {
		$ENV{"CLASSPATH"} = $tmp;
	}

	if ($exec_result == 0) {
		$java_jvm_maxmem_arg = "-Xmx"; # Sun JVM max heapsize flag
	} else {
		$java_jvm_maxmem_arg = "";
	}

	if($iswindows == 1){
		print FIX "JAVA = $jvm\n";
		print FIX "JAVA_EXTRA_ARGUMENTS = -Xmx1024m\n";
	} else {
		print FIX "JAVA = $jvm\n";
	}


	# above stolen from condor_configure

	if( exists $ENV{NMI_PLATFORM} ) {
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

		if( ($ENV{NMI_PLATFORM} =~ /ppc64_macos_10.3/) ) {
	# evil work around for macos
			print FIX "JAVA_EXTRA_ARGUMENTS = -Djava.vm.vendor=Apple\n";
		}
	}

	# Add a job wrapper for windows.... and a few other things which
	# normally are done by condor_configure for a personal condor
	#if($iswindows == 1) {
	#	print FIX "USER_JOB_WRAPPER = $wininstalldir/bin/exe_switch.bat\n";
	#}

	# Tell condor to use the current directory for temp.  This way,
	# if we get preempted/killed, we don't pollute the global /tmp
	#mkdir( "$installdir/tmp", 0777 ) || die "Can't mkdir($installdir/tmp): $!\n";
	print FIX "TMP_DIR = \$(LOG)/tmp\n";

	# do this for all now....

	my $mypath = $ENV{PATH};
	print FIX "START = TRUE\n";
	print FIX "PREEMPT = FALSE\n";
	print FIX "SUSPEND = FALSE\n";
	print FIX "KILL = FALSE\n";
	print FIX "WANT_SUSPEND = FALSE\n";
	print FIX "WANT_VACATE = FALSE\n";
	print FIX "COLLECTOR_NAME = Personal Condor for Tests\n";
	print FIX "SCHEDD_INTERVAL_TIMESLICE = .99\n";
	#insure path from framework is injected into the new pool
	if($iswindows == 0) {
		print FIX "environment=\"PATH=\'$mypath\'\"\n";
	}
	print FIX "SUBMIT_EXPRS=environment\n";
	print FIX "PROCD_LOG = \$(LOG)/ProcLog\n";
	if($iswindows == 1) {
		my $procdaddress = "buildandtestprocd" . $$;
		print FIX "PROCD_ADDRESS = \\\\.\\pipe\\$procdaddress\n";
	}

	close FIX; 
}

sub set_env {
    my ($key, $val) = @_;;
    #print "Setting environment variable:\n";
    #print "\t$key -> '$val'\n";
    $ENV{$key} = $val;
}


sub dir_listing {
    my (@path) = @_;

    my $path = File::Spec->catdir(@path);

    # If we have a relative path then show the CWD
    my $cwd = "";
    if(not File::Spec->file_name_is_absolute($path)) {
        $cwd = "(CWD: '" . Cwd::getcwd() . "')";
    }
    print "Showing directory contents of path '$path' $cwd\n";

    # We have to check $^O because the platform_* scripts will be executed on a Linux
    # submit node - but the nmi platform string will have winnt
    if( is_windows() && $^O ne "linux" ) {
        system("dir $path");
    }
    else {
        system("ls -l $path");
    }
}


sub which {
    my ($exe) = @_;

if( is_windows() ) {
        return `\@for \%I in ($exe) do \@echo(\%~\$PATH:I`;
    }
    else {
        return system("which $exe");
    }
}

sub is_windows {
    if( $ENV{NMI_PLATFORM} =~ /_win/i ) {
        return 1;
    }
    return 0;
}

sub is_cygwin_perl {
    if ($^O =~ /cygwin/i) {
        return 1;
    }
    return 0;
}

sub is_windows_native_perl {
    if ($^O =~ /MSWin32/) {
         return 1;
    }
    return 0;
}

# Cygwin's chomp does not remove the \r
 sub fullchomp {
	# Preserve the behavior of chomp, e.g. chomp $_ if no argument is specified.
	push (@_,$_) if( scalar(@_) == 0);

	foreach my $arg (@_) {
 		$arg =~ s/[\012\015]+$//;
	}

	return;
}

##############################################################################
##
## getFqdnHost
##
## hostname sometimes does not return a fully qualified
## domain name and we must ensure we have one. We will call
## this function in the tests wherever we were calling
## hostname.
###############################################################################

sub getFqdnHost {
	my $host = hostfqdn();
	fullchomp($host);
	return($host);
}

# which is broken on some platforms, so implement a Perl version here
sub Which {
	my ($exe) = @_;

	return "" if(!$exe);

	if( is_windows_native_perl() ) {
		return `\@for \%I in ($exe) do \@echo(\%~\$PATH:I`;
	}
	foreach my $path (split /:/, $ENV{PATH}) {
		chomp $path;
		#print "Checking <$path>\n";
		if(-f "$path/$exe") {
			return "$path/$exe";
		}
	}

return "";
}

sub CreateDir
{
	my $cmdline = shift;
	my @argsin = split /\s/, $cmdline;
	my $cmdcount = @argsin;
	my $ret = 0;
	my $fullcmd = "";
	print  "CreateDir: $cmdline argcout:$cmdcount\n";

	my $amwindows = is_windows();

	my $winpath = "";
	if($amwindows == 1) {
		if(is_windows_native_perl()) {
			if($argsin[0] eq "-p") {
				$_ = $argsin[1];
				s/\\/\\\\/g;
				$winpath = $_;
				#$winpath = $argsin[1];
				if(-d "$argsin[1]") {
					return($ret);
				}
			} else {
				#$_ = $argsin[0];
				#s/\\/\\\\/g;
				#$winpath = $_;
				$winpath = $argsin[0];
				if(-d "$argsin[0]") {
					return($ret);
				}
			}
		} else {
			if($argsin[0] eq "-p") {
				$winpath = `cygpath -w $argsin[1]`;
				CondorUtils::fullchomp($winpath);
				$_ = $winpath;
				s/\\/\\\\/g;
				$winpath = $_;
				if(-d "$argsin[1]") {
					return($ret);
				}
			} else {
				$winpath = `cygpath -w $argsin[0]`;
				CondorUtils::fullchomp($winpath);
				$_ = $winpath;
				s/\\/\\\\/g;
				$winpath = $_;
				if(-d "$argsin[0]") {
					return($ret);
				}
			}
		}

		$fullcmd = "cmd /C mkdir $winpath";
		$ret = system("$fullcmd");
		print "Tried to create dir got ret value:$ret path:$winpath/$fullcmd\n";
	} else {
		if($cmdline =~ /\-p/) {
			$_ = $cmdline;
			s/\-p//;
			$cmdline = $_;
		}
		$fullcmd = "mkdir $cmdline"; 	
		if(-d $cmdline) {
			return($ret);
		}
		$ret = system("$fullcmd");
		#print "Tried to create dir got ret value:$ret path:$cmdline/$fullcmd\n";
	}
	return($ret);
}

sub ProcessReturn {
	my ($status) = @_;
	my $rc = -1;
	my $signal = 0;
	my @result;

	#print "ProcessReturn: Entrance status " . sprintf("%x", $status) . "\n";
	if ($status == -1) {
		# failed to execute, how do I represent this? Choose -1 for now
		# since that is an impossible unix return code.
		$rc = -1;
		#print "ProcessReturn: Process Failed to Execute.\n";
	} elsif ($status & 0x7f) {
		# died with signal and maybe coredump.
		# Ignore the fact a coredump happened for now.

		# XXX Stupidly, we also make the return code the same as the 
		# signal. This is a legacy decision I don't want to change now because
		# I don't know how big the ramifications will be.
		$signal = $status & 0x7f;
		$rc = $signal;
		#print "ProcessReturn: Died with Signal: $signal\n";
	} else {
		# Child returns valid exit code
		$rc = $status >> 8;
		#print "ProcessReturn: Exited normally $rc\n";
	}

	#print "ProcessReturn: return=$rc, signal=$signal\n";
	push @result, $rc;
	push @result, $signal;
	return @result;
}

1;
