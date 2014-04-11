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

use strict;
use warnings;
use Cwd;
use File::Spec;
use CondorTest;
use CondorUtils;

package TestGlue;

my $installdir = "";
my $wininstalldir = "";
my $initialconfig = "";

my $iswindows = CondorUtils::is_windows();
my $iscygwin  = CondorUtils::is_cygwin_perl();
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
    dir_listing(".");
    print "---------- End Debug Header --------------\n";
}

sub setup_test_environment {

    my $base_dir = Cwd::getcwd();

	$initialconfig = "$base_dir/condor_tests/Config";
	$targetconfig = "$base_dir/condor_tests/Config/condor_config";
	$targetconfiglocal = "$base_dir/condor_tests/Config/condor_config.local";
	if(!(-d $initialconfig)) {
		create_initial_configs($initialconfig);
	}

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

        # Condor will want Win32-style paths for CONDOR_CONFIG
<<<<<<< HEAD
        set_env("CONDOR_CONFIG", "$base_dir\\condor_tests\\TestingPersonalCondor\\condor_config");
		set_env("TESTS","$base_dir\\condor_tests");
=======
        set_env("CONDOR_CONFIG", "$base_dir\\condor_tests\\Config\\condor_config");
>>>>>>> start of moving config file generation to the glue.#4306
        
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

sub create_initial_configs {
	my $configlocation = shift;
	my $basedir = shift;

	my $awkscript = "condor_examples/convert_config_to_win32.awk";
	my $genericconfig = "condor_examples/condor_config.generic";
	my $genericlocalconfig = "condor_examples/condor_config.local.central.manager";
	
	if( -d $configlocation ) {
		#debug( "Test Personal Condor Directory Established prior\n",2);
	} else {
		#debug( "Test Personal Condor Directory being Established now\n",2);
		system("mkdir -p $configlocation");
	}
        
	WhereIsInstallDir();
        
	debug("Need to set up config files for test condor\n",2);
	CreateConfig($awkscript, $genericconfig);
	CreateLocalConfig($awkscript, $genericlocalconfig);
	#CreateLocal(); We are never going to run a condor here, just need a base set
	# of config files.
}

sub WhereIsInstallDir {
	if($iswindows == 1) {
		my $top = getcwd();
		debug( "getcwd says \"$top\"\n",2);
		if ($iscygwin) {
			my $crunched = `cygpath -m $top`;
			CondorUtils::fullchomp($crunched);
			debug( "cygpath changed it to: \"$crunched\"\n",2);
			my $ppwwdd = `pwd`;
			debug( "pwd says: $ppwwdd\n",2);
		} else {
			my $ppwwdd = `cd`;
			debug( "cd says: $ppwwdd\n",2);
		}
	}

	my $master_name = "condor_master"; if ($iswindows) { $master_name = "condor_master.exe"; }
	my $tmp = CondorTest::Which($master_name);
	if ( ! ($tmp =~ /condor_master/ ) ) {
		print STDERR "CondorTest::Which($master_name) returned:$tmp\n";
		print STDERR "Unable to find a $master_name in your \$PATH!\n";
		exit(1);
	}
	CondorUtils::fullchomp($tmp);
	debug( "Install Directory \"$tmp\"\n",2);
	if ($iswindows) {
		if ($iscygwin) {
			$tmp =~ s|\\|/|g; # convert backslashes to forward slashes.
			if($tmp =~ /^(.*)\/bin\/condor_master.exe\s*$/) {
				$installdir = $1;
				$tmp = `cygpath -m $1`;
				CondorUtils::fullchomp($tmp);
				$wininstalldir = $tmp;
			}
		} else {
			$tmp =~ s/\\bin\\condor_master.exe$//i;
			$installdir = $tmp;
			$wininstalldir = $tmp;
		}
		$wininstalldir =~ s|/|\\|g; # forward slashes.to backslashes
		$installdir =~ s|\\|/|g; # convert backslashes to forward slashes.
		print "Testing this Install Directory: \"$wininstalldir\"\n";
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

sub CreateConfig {
	my ($awkscript, $genericconfig) = @_;

	# The only change we need to make to the generic configuration
	# file is to set the release-dir and local-dir. (non-windows)
	# change RELEASE_DIR and LOCAL_DIR    
	my $currenthost = CondorTest::getFqdnHost();
	CondorUtils::fullchomp($currenthost);

	debug( "Set RELEASE_DIR and LOCAL_DIR\n",2);

	# Windows needs config file preparation, wrapper scripts etc
	if($iswindows == 1) {
		# pre-process config file src and windowize it
		# create config file with todd's awk script
		my $configcmd = "gawk -f $awkscript $genericconfig";
		if ($^O =~ /MSWin32/) {  $configcmd =~ s/gawk/awk/; $configcmd =~ s|/|\\|g; }
		debug("awk cmd is $configcmd\n",2);

		open(OLDFIG, " $configcmd 2>&1 |") || die "Can't run script file\"$configcmd\": $!\n";    
	} else {
		open(OLDFIG, '<', $configmain ) || die "Can't read base config file $configmain: $!\n";
	}

	open(NEWFIG, '>', $targetconfig ) || die "Can't write to new config file $targetconfig: $!\n";    
	while( <OLDFIG> ) {
		CondorUtils::fullchomp($_);        
		if(/^RELEASE_DIR\s*=/) {
			debug("Matching:$_\n", 2);
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
			debug( "Matching:$_\n",2);
			if($iswindows == 1) {
				print NEWFIG "LOCAL_CONFIG_FILE = $initialconfig/condor_config.local\n";
			}
			else {
				print NEWFIG "LOCAL_CONFIG_FILE = $initialconfig/condor_config.local\n";
			}
		}
		elsif(/^LOCAL_CONFIG_DIR\s*=/) {
			# we don't want this
		}
		elsif(/^CONDOR_HOST\s*=/) {
			debug( "Matching:$_\n",2);
			print NEWFIG "CONDOR_HOST = $currenthost\n";
		}
		elsif(/^ALLOW_WRITE\s*=/) {
			debug( "Matching:$_\n",2);
			print NEWFIG "ALLOW_WRITE = *\n";
		}
		elsif(/NOT_RESPONDING_WANT_CORE\s*=/ and $want_core_dumps ) {
			debug( "Matching:$_\n",2);
			print NEWFIG "NOT_RESPONDING_WANT_CORE = True\n";
		}
		elsif(/CREATE_CORE_FILES\s*=/ and $want_core_dumps ) {
			debug( "Matching:$_\n",2);
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

	debug( "Modifying local config file\n",2);
	my $logsize = 50000000;

	# make sure ports for Personal Condor are valid, we'll use address
	# files and port = 0 for dynamic ports...
	if($iswindows == 1) {
	# create config file with todd's awk script
		my $configcmd = "gawk -f $awkscript $genericlocalconfig";
		if ($^O =~ /MSWin32/) {  $configcmd =~ s/gawk/awk/; $configcmd =~ s|/|\\|g; }
		debug("gawk cmd is $configcmd\n",2);

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
	if($iswindows == 1) {

		$javabinary = "java.exe";
		if ($^O =~ /MSWin32/) {
			$jvm = `\@for \%I in ($javabinary) do \@echo(\%~sf\$PATH:I`;
					CondorUtils::fullchomp($jvm);
		} else {
			#can't use which. its a linux tool and will lie about the path to java.
			if (1) {
				debug ("Running where $javabinary\n",2);
				$jvm = `where $javabinary`;
				CondorUtils::fullchomp($jvm);
				# if where doesn't tell us the location of the java binary, just assume it's will be
				# in the path once condor is running. (remember that cygwin lies...)
				if ( ! ($jvm =~ /java/i)) {
					# we need a special check for 64bit java if we are a 32 bit app.
					if ( -e '/cygdrive/c/windows/sysnative/java.exe') {
						debug ("where $javabinary returned nothing, but found 64bit java in sysnative dir\n",2);
						$jvm = "c:\\windows\\sysnative\\java.exe";
					} else {
					debug ("where $javabinary returned nothing, assuming java will be in Condor's path.\n",2);
					$jvm = "java.exe";
					}
				}
			} else {
				my $whichtest = `which $javabinary`;
				CondorUtils::fullchomp($whichtest);
				$whichtest =~ s/Program Files/progra~1/g;
				$jvm = `cygpath -m $whichtest`;
				CondorUtils::fullchomp($jvm);
			}
		}
		CondorTest::debug("which java said: $jvm\n",2);

		$java_libdir = "$wininstalldir/lib";

	} else {
		# below stolen from condor_configure

		my @default_jvm_locations = ("/bin/java",
			"/usr/bin/java",
			"/usr/local/bin/java",
			"/s/std/bin/java");

		$javabinary = "java";
		unless (system ("which java >> /dev/null 2>&1")) {
			CondorUtils::fullchomp(my $which_java = CondorTest::Which("$javabinary"));
			CondorTest::debug("CT::Which for $javabinary said $which_java\n",2);
			@default_jvm_locations = ($which_java, @default_jvm_locations) unless ($?);
		}

		$java_libdir = "$installdir/lib";

		# check some default locations for java and pick first valid one
		foreach my $default_jvm_location (@default_jvm_locations) {
			CondorTest::debug("default_jvm_location is:$default_jvm_location\n",2);
			if ( -f $default_jvm_location && -x $default_jvm_location) {
				$jvm = $default_jvm_location;
				print "Set JAVA to $jvm\n";
				last;
			}
		}
	}
	# if nothing is found, explain that, otherwise see if they just want to
	# accept what I found.
	debug ("Setting JAVA=$jvm\n",2);
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
    print "Setting environment variable:\n";
    print "\t$key -> '$val'\n";
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

1;
