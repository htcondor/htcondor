#!/usr/bin/env perl

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


# batch_test.pl - Condor Test Suite Batch Tester

use File::Copy;
use FileHandle;
use POSIX "sys_wait_h";
use Cwd;
use CondorUtils;
use CondorTest;
use Time::Local;
use Getopt::Long;
use File::Path qw(mkpath); # make_path is preferred, but too new to rely on
use strict;
use warnings;

################################################################################
# Constants.

# Generic Condor configuration file.
my $GENERICCONFIG = "../condor_examples/condor_config.generic";

# Generic local Condor configuration file.
my $GENERICLOCALCONFIG = "../condor_examples/condor_config.local.central.manager";

# AWK script to make generic Condor configuration file Windows ready.
my $AWKSCRIPT = "../condor_examples/convert_config_to_win32.awk";

# Tests which cannot currently be isolated (run in their own directory).  If
# you are writing a new test, and you need to add it here, you probably made a
# mistake!  Ideally these older tests should be examined and fixed.
my %ISOLATION_BLACKLIST = map({($_,1)} qw(
	cmd_q_shows-global
	cmd_q_shows-name
	cmd_q_shows-pool
	cmd_status_shows-any
	cmd_status_shows-avail
	cmd_status_shows-claim
	cmd_status_shows-constraint
	cmd_status_shows-format
	cmd_status_shows-master
	cmd_status_shows-negotiator
	cmd_status_shows-schedd
	cmd_status_shows-startd
	cmd_status_shows-state
	cmd_status_shows-submitters
	cmd_wait_shows-all
	cmd_wait_shows-base
	cmd_wait_shows-notimeout
	cmd_wait_shows-partial
	fetch_work-basic
	job_amazon_basic
	job_ckpt_combo-sanity_std
	job_ckpt_dup_std
	job_ckpt_dup_std
	job_ckpt_floats-async_std
	job_ckpt_floats_std
	job_ckpt_f-reader_std
	job_ckpt_gettimeofday_std
	job_ckpt_integers_std
	job_ckpt_io-async_std
	job_ckpt_io-buffer-async_std
	job_ckpt_longjmp_std
	job_ckpt_open-N-parallel_std
	job_ckpt_open-N-parallel_std
	job_ckpt_signals_std
	job_ckpt_socket-support_std
	job_ckpt_stack_std
	job_condorc_abc_van
	job_condorc_ab_van
	job_core_basic_par
	job_core_bigenv_std
	job_core_chirp_par
	job_core_compressfiles_std
	job_core_coredump_std
	job_core_crontab_local
	job_core_crontab_van
	job_core_input_java
	job_core_input_sched
	job_core_input_van
	job_core_leaveinqueue_van
	job_core_max-running_local
	job_core_onexithold_local
	job_core_onexithold_van
	job_core_onexitrem_local
	job_core_output_java
	job_core_output_sched
	job_core_output_van
	job_core_perhold_local
	job_core_perhold_sched
	job_core_perhold_van
	job_core_perrelease_local
	job_core_perremove_local
	job_core_perremove_sched
	job_core_perremove_van
	job_core_queue_sched
	job_core_remote-initialdir_van
	job_core_shadow-lessthan-memlimit_van
	job_core_time-deferral-hold_local
	job_core_time-deferral-hold_van
	job_core_time-deferral_local
	job_core_time-deferral-remove_local
	job_core_time-deferral-remove_van
	job_core_time-deferral_van
	job_dagman_abnormal_term_recovery_retries
	job_dagman_abort-A
	job_dagman_basic
	job_dagman_comlog
	job_dagman_default_log
	job_dagman_depth_first
	job_dagman_event_log
	job_dagman_fullremove
	job_dagman_large_dag
	job_dagman_log_path
	job_dagman_maxpostscripts
	job_dagman_maxprescripts
	job_dagman_multi_dag
	job_dagman_node_dir
	job_dagman_node_prio
	job_dagman_noop_node
	job_dagman_parallel-A
	job_dagman_prepost
	job_dagman_recovery_event_check
	job_dagman_rescue-A
	job_dagman_rescue_recov
	job_dagman_retry_recovery
	job_dagman_retry
	job_dagman_script_args
	job_dagman_splice-A
	job_dagman_splice-B
	job_dagman_splice-cat
	job_dagman_splice-C
	job_dagman_splice-D
	job_dagman_splice-E
	job_dagman_splice-F
	job_dagman_splice-G
	job_dagman_splice-H
	job_dagman_splice-I
	job_dagman_splice-K
	job_dagman_splice-L
	job_dagman_splice-M
	job_dagman_splice-N
	job_dagman_splice-O
	job_dagman_splice-scaling
	job_dagman_subdag-A
	job_dagman_submit_fails_post
	job_dagman_uncomlog
	job_dagman_unlessexit
	job_dagman_usedagdir
	job_dagman_vars
	job_dagman_xmlcomlog
	job_dagman_xmluncomlog
	job_filexfer_base-input4_van
	job_filexfer_base_van
	job_filexfer_basic_van
	job_filexfer_input-onegone_van
	job_filexfer_md5-remote_van
	job_filexfer_trans-excut-true_van
	job_flocking_to
	job_quill_basic
	job_rsc_all-syscalls_std
	job_rsc_atexit_std
	job_rsc_fcntl_std
	job_rsc_f-direct-write_std
	job_rsc_fgets_std
	job_rsc_fgets_std
	job_rsc_fread_std
	job_rsc_fread_std
	job_rsc_fstream_std
	job_rsc_ftell_std
	job_rsc_ftell_std
	job_rsc_getdirentries_std
	job_rsc_hello_std
	job_rsc_hello_std
	job_rsc_open-N-serial_std
	job_rsc_stat_std
	job_rsc_stat_std
	job_rsc_truncate_std
	job_rsc_zero-calloc_std
	job_schedd_restart-holdjobs-ok
	job_vmu_basic
	job_vmu_cdrom
	job_vmu_ckpt
	lib_auth_config
	lib_auth_protocol-ctb
	lib_auth_protocol-fs
	lib_auth_protocol-negot-prt2
	lib_auth_protocol-negot-prt3
	lib_auth_protocol-negot-prt4
	lib_auth_protocol-negot
	lib_auth_protocol-ssl
	lib_chirpio_van
	lib_classads
	lib_eventlog_base
	lib_eventlog_build
	lib_eventlog
	lib_eventlog-xml
	lib_lease_manager-get_rel_1
	lib_lease_manager-get_rel_2
	lib_lease_manager-get_rel_3
	lib_lease_manager-get_rel_4
	lib_lease_manager-get_rel_5
	lib_lease_manager-simple
	lib_shared_port_van
	lib_unit_tests
	lib_userlog
	soap_job_tests
	));

################################################################################
# Globals
#
# Keep these to a bare minimum

# Lists of successful and failed tests.
my @successful_tests;
my @failed_tests;

exit main();

################################################################################
sub main {
	#################################################################
	#
	# Debug messages get time stamped. These will start showing up
	# at DEBUGLEVEL = 1.
	# 
	# level 1 - historic test output
	# level 2 - batch_test.pl output
	# level 4 - debug statements from CondorTest.pm
	# level 5 - debug statements from Condor.pm
	#
	# There is no reason not to have debug always on the the level
	# pretty completely controls it. All DebugOff calls
	# have been removed.
	#
	# CondorPersonal.pm has a similar but separate mechanism.
	#
	#################################################################
	
	Condor::DebugOn();
	Condor::DebugLevel(1);
	
	#################################################################
	#
	#	Environament variables used to communicate with CondorPersonal
	#
	# 	This is all triggered by -w
	#
	#	WRAP_TESTS
	#
	#	We want to search out core files and ERROR prints AND we want
	#	to run many tests at once. The first check method looks for
	#	all logs changed during the test and assigns blame based on that.
	#
	#		Allow for every test which is not wrapped in a personal 
	#		condor to be wrapped. This is done in CondorTest.pm in RunTest
	#		and RunDagTest if WRAP_TESTS is set.
	#
	#################################################################
	
	#my $LogFile = "batch_test.log";
	#open(OLDOUT, ">&STDOUT");
	#open(OLDERR, ">&STDERR");
	#open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!";
	#open(STDERR, ">&STDOUT");
	#select(STDERR); $| = 1;
	#select(STDOUT); $| = 1;
	
	my $iswindows = CondorTest::IsThisWindows();
	
	# configuration options
	my $test_retirement = 3600;	# seconds for an individual test timeout - 30 minutes
	my $BaseDir = getcwd();
	my $quiet = 0;
	my $sortfirst = 0;
	my $timestamp = 0;
	my $kindwait = 1; # run tests one at a time
	my $groupsize = 0; # run tests in group for more throughput
	my $currentgroup = 0;
	my $repeat = 1; # Number of times to run the tests.
	my $cleanupcondor = 0;
	my $want_core_dumps = 1;
	my $testpersonalcondorlocation = "$BaseDir/TestingPersonalCondor";
	my $wintestpersonalcondorlocation = get_win_path($testpersonalcondorlocation);

	my $results_dir;
	{
		my($sec,$min,$hour,$mday,$mon,$year) = localtime(time);
		$results_dir = sprintf "$BaseDir/results/%04d-%02d-%02d-%02d-%02d-%02d-%d",
			$year+1900, $mon+1, $mday, $hour, $min, $sec, $$;
	}
	
	my $targetconfig = "$testpersonalcondorlocation/condor_config";
	my $targetconfiglocal = "$testpersonalcondorlocation/condor_config.local";
	my $condorpidfile = "/tmp/condor.pid.$$";
	my @extracondorargs;
	
	my $localdir = "$wintestpersonalcondorlocation/local";
	my $installdir;
	my $wininstalldir; # need to have dos type paths for condor
	my $testdir;
	
	my $wantcurrentdaemons = 1; # dont set up a new testing pool in condor_tests/TestingPersonalCondor
	my $pretestsetuponly = 0; # only get the personal condor in place
	
	# set up to recover from tests which hang
	$SIG{ALRM} = sub { die "timeout" };
	
	my @compilers;
	
	# setup
	STDOUT->autoflush();   # disable command buffering of stdout
	STDERR->autoflush();   # disable command buffering of stderr
	
	# remove . from path
	CleanFromPath(".");
	# yet add in base dir of all tests and compiler directories
	$ENV{PATH} = "$ENV{PATH}:$BaseDir";

	# Search scripts directory for libraries (CondorTest.pm and others)
	$ENV{PERL5LIB} = "$BaseDir/../condor_scripts";
	# Add BaseDir to Perl's search path.  
	$ENV{PERL5LIB} = "$ENV{PERL5LIB}:$BaseDir";
	
	#
	# the args:
	
	my $testfile = "";
	my $ignorefile = "";
	my @testlist;
	
	GetOptions('help|h' => \&usage_and_exit,
		'core!' => \$want_core_dumps,
		'wrap|w' => sub {$ENV{WRAP_TESTS} = "yes";},
		'directory|d=s' => \@compilers,
		'file|f=s' => \$testfile,
		'ignore|i=s' => \$ignorefile,
		'retirement|r=i' => \$test_retirement,
		'sort|s!' => \$sortfirst,
		'kind|k!' => \$kindwait,
		'evenly|e=i' => \$groupsize,
		'again|a=i' => \$repeat,
		'buildandtest|b' => sub { $wantcurrentdaemons = 0; },
		'pretest|p' => sub { $wantcurrentdaemons = 0; $pretestsetuponly = 1; },
		'testname|t=s' => \@testlist,
		'quiet|q!' => \$quiet,
		'marktime|m!' => \$timestamp,
		'cleanup|c' => sub {
				$cleanupcondor = 1;
				push (@extracondorargs, "-pidfile $condorpidfile");
			},
		);
	
	if($groupsize != 0 && $kindwait) {
		die "$0: ERROR: The 'evenly' and 'kind' options are not compatible.\n";
	}

	set_quiet($quiet);
	
	my %test_suite = ();
	
	# take a momment to get a personal condor running if it is not configured
	# and running already
	
	my $nightly = CondorTest::IsThisNightly($BaseDir);
	my $res = 0;
	
	if(!($wantcurrentdaemons)) {
	
	
		if($nightly == 1) {
			print "This is a nightly test run\n";
		}
	
		$res = IsPersonalTestDirThere($testpersonalcondorlocation);
		if($res != 0) {
			die "Could not establish directory for personal condor\n";
		}
	
		($installdir, $wininstalldir) = WhereIsInstallDir($iswindows);
	
		$res = IsPersonalTestDirSetup($testpersonalcondorlocation);
		if($res == 0) {
			debug("Need to set up config files for test condor\n",2);
			CreateConfig($targetconfig, $iswindows, 
				$iswindows?$wininstalldir:$installdir,
				$localdir,
				$wintestpersonalcondorlocation,
				$want_core_dumps
				);
			CreateLocalConfig($targetconfiglocal, $iswindows, 
				$iswindows?$wininstalldir:$installdir);
			CreateLocal($testpersonalcondorlocation);
		}
	
		{
			my $tmp = get_win_path($targetconfig);
			$ENV{CONDOR_CONFIG} = $tmp;
			$res = IsPersonalRunning($tmp, $iswindows);
		}
	
		# capture pid for master
		chdir("$BaseDir");
	
		if($res == 0) {
			debug("Starting Personal Condor\n",2);
			if($iswindows == 1) {
				my $mcmd = "$wininstalldir/bin/condor_master.exe -f &";
				$mcmd =~ s/\\/\//g;
				debug( "Starting master like this:\n",2);
				debug( "\"$mcmd\"\n",2);
				CondorTest::verbose_system("$mcmd",{emit_output=>0,use_system=>1});
			} else {
				CondorTest::verbose_system("$installdir/sbin/condor_master @extracondorargs -f &",{emit_output=>0,use_system=>1});
			}
			debug("Done Starting Personal Condor\n",2);
		}
			
		IsRunningYet();
	
	}
	
	my @myfig = `condor_config_val -config`;
	debug("Current config settings are:\n",2);
	foreach my $fig (@myfig) {
		debug("$fig\n",2);
	}
	
	if($pretestsetuponly == 1) {
		# we have done the requested set up, leave
		exit(0);
	}
	
	print "Ready for Testing\n";
	
	# figure out what tests to try to run.  first, figure out what
	# compilers we're trying to test.  if that was given on the command
	# line, we just use that.  otherwise, we search for all subdirectories
	# in the current directory that might be compiler subdirs...
	if($#compilers == -1 ) {
		@compilers = ("g77", "gcc", "gpp", "gfortran");
	}
	
	if($timestamp == 1) {
		print scalar localtime() . "\n";
	}
	
	foreach my $name (@compilers) {
		quiet_debug("Compiler: $name\n", 2);
	}

	mkpath($results_dir) or die "Unable to mkpath($results_dir): $!";
	
	# now we find the tests we care about.
	if( @testlist ) {
	
		debug("working on testlist\n",2);
		foreach my $name (@testlist) {
			quiet_debug("Testlist: $name\n",2);;
		}
	
		# we were explicitly given a # list on the command-line
		foreach my $test (@testlist) {
			if( ! ($test =~ /(.*)\.run$/) ) {
				$test = "$test.run";
			}
			foreach my $compiler (@compilers)
			{
				push(@{$test_suite{"$compiler"}}, $test);
			}
		}
	} elsif( $testfile ) {
		debug("working on testfile\n",2);
		# if we were given a file, let's read it in and use it.
		#print "found a runfile: $testfile\n";
		open(TESTFILE, $testfile) || die "Can't open $testfile\n";
		while( <TESTFILE> ) {
			CondorTest::fullchomp($_);
			my $test = $_;
			if($test =~ /^#.*$/) {
				#print "skip comment\n";
				next;
			}
			#//($compiler, $test) = split('\/');
			if( ! ($test =~ /(.*)\.run$/) ) {
				$test = "$test.run";
			}
			foreach my $compiler (@compilers)
			{
				push(@{$test_suite{"$compiler"}}, $test);
			}
		}
		close(TESTFILE);
	} else {
		# we weren't given any specific tests or a test list, so we need to
		# find all test programs (all files ending in .run) for each compiler
		my $gotdot = 0;
		debug("working on default test list\n",2);
		foreach my $compiler (@compilers) {
			if($compiler eq ".") {
				$gotdot = 1;
			} else {
			if (-d $compiler) {
				opendir( COMPILER_DIR, $compiler )
					|| die "error opening \"$compiler\": $!\n";
				@{$test_suite{"$compiler"}} =
					grep /\.run$/, readdir( COMPILER_DIR );
				closedir COMPILER_DIR;
				#print "error: no test programs found for $compiler\n"
					#unless @{$test_suite{"$compiler"}};
				} else {
					print "Skipping unbuilt compiler dir: $compiler\n";
				}
			}
		}
		# by default look at the current blessed tests in the top
		# level of the condor_tests directory and run these.
		my $moretests = "";
		my @toptests;
	
		if($iswindows == 1) {
			open(QUICK,"<Windows_list")|| die "Can't open Windows_list\n";
		} else {
			open(QUICK,"<list_quick")|| die "Can't open list_quick\n";
		}
	
		while(<QUICK>) {
			CondorTest::fullchomp($_);
			my $tmp = $_;
			if( $tmp =~ /^#.*$/ ) {
				# comment so skip
				next;
			}
			$moretests = "$tmp.run";
			push @toptests,$moretests;
		}
		close(QUICK);
		@{$test_suite{"."}} = @toptests;
		if($gotdot == 1) {
			#already testing dot
		} else {
			push @compilers, ".";
		}
	}
	
	
	# if we were given a skip file, let's read it in and use it.
	# remove any skipped tests from the test list
	if( $ignorefile ) {
		debug("found a skipfile: $ignorefile \n",1);
		open(SKIPFILE, $ignorefile) || die "Can't open $ignorefile\n";
		while(<SKIPFILE>) {
			CondorTest::fullchomp($_);
			my $test = $_;
			foreach my $compiler (@compilers) {
				# $skip_hash{"$compiler"}->{"$test"} = 1;
				#@{$test_suite{"$compiler"}} = grep !/$test\.run/, @{$test_suite{"$compiler"}};
				@{$test_suite{"$compiler"}} = grep !/$test/, @{$test_suite{"$compiler"}};
			}
		}
		close(SKIPFILE);
	}
	
	# Now we'll run each test.
	print "Testing: " . join(" ", @compilers) . "\n";
	
	my $lastcompiler = "";
	my $hashsize = 0;
	my %test;
	
	foreach my $compiler (@compilers)
	{
		$lastcompiler = $compiler;
		# as long as we have tests to start, loop back again and start
		# another when we are trying to keep N running at once
		my $testspercompiler = $#{$test_suite{"$compiler"}} + 1;
		my $currenttest = 0;
	
		debug("Compiler/Directory <$compiler> has $testspercompiler tests\n",2);
		if($compiler ne "\.") {
			# Meh, if the directory isn't there, just skip it instead of bailing.
			chdir $compiler || (print "Skipping $compiler directory\n" && next);
		}
		my $compilerdir = getcwd();
		# add in compiler dir to the current path
		$ENV{PATH} = "$ENV{PATH}:$compilerdir";
	
		# fork a child to run each test program
		quiet_debug("submitting $compiler tests\n", 2);
	
		# if batching tests, randomize order
		if(($groupsize > 0) && ($sortfirst == 0)){
			yates_shuffle(\@{$test_suite{"$compiler"}});
		}
		my @currenttests = @{$test_suite{"$compiler"}};
		if($sortfirst == 1) {
			@currenttests = sort @currenttests;
		}
		foreach my $test_program (@currenttests)
		{
			# doing this next test
			$currenttest = $currenttest + 1;
	
			debug("Want to test $test_program\n",2);
	
			#next if $skip_hash{$compiler}->{$test_program};
	
			# allow multiple runs easily
			my $repeatcounter = 0;
			quiet_debug("Want $repeat runs of each test\n",3);
			while($repeatcounter < $repeat) {
	
				debug( "About to fork test<$currentgroup>\n",2);
				$currentgroup += 1;
				debug( "About to fork test new size<$currentgroup>\n",2);
				my $pid = fork();
				quiet_debug( "forking for $test_program pid returned is $pid\n",3);
				die "error calling fork(): $!\n" unless defined $pid;

				my $testname = $test_program;
				$testname =~ s/\.run$//;
				my $workdir = "$results_dir/$testname/$repeatcounter";
	
				# two modes
				#		kindwait = resolve each test after the fork
				#		else: fork them all and then wait for all
	
				if( $kindwait == 1 ) {
				#*****************************************************************
					if( $pid > 0 ) {
						$test{$pid} = { program => $test_program,
							testname => $testname,
							workdir => $workdir };
						debug( "Started test: $test_program/$pid\n",2);
	
						# Wait for job before starting the next one
	
						StartTestOutput($compiler,$test_program);
	
						#print "Waiting on test\n";
						my $child;
						while( $child = wait() ) {
	
							# if there are no more children, we're done
							last if $child == -1;
			
							# ignore spurious children
							if(! defined $test{$child}) {
								debug("Can't find jobname for child? <ignore>\n",2);
								next;
							} else {
								debug( "informed $child gone yeilding test $test{$child}{'program'}\n",2);
							}
	
							#finally
							my $test_name = $test{$child}{'testname'};
							debug( "Done Waiting on test($test_name)\n",3);
	
							# record the child's return status
							my $status = $?;
	
							CompleteTestOutput($compiler,$test_program,$child,$status, $test{$child}{'program'}, $test{$child}{'workdir'});
							delete $test{$child};
							$hashsize = keys %test;
							debug("Tests remaining: $hashsize\n",3);
							last if $hashsize == 0;
						}
					} else {
						# if we're the child, start test program
						DoChild($test_program, $test_retirement, $workdir, $repeat);
					}
				#*****************************************************************
				} else {
					if( $pid > 0 ) {
						$test{$pid} = { program => $test_program,
							testname => $testname,
							workdir => $workdir };
						quiet_debug( "Started test: $test_program/$pid\n",2);
						# are we submitting all the tests for a compiler and then
						# waiting for them all? Or are we submitting a bunch and waiting
						# for them before submitting some more.
						if($groupsize != 0) {
							debug( "current group: $currentgroup Limit: $groupsize\n",2);
							if($currentgroup == $groupsize) {
								debug( "wait for batch\n",2);
								while( my $child = wait() ) {
									# if there are no more children, we're done
									last if $child == -1;
								
									# record the child's return status
									my $status = $?;
	
		
									if(! defined $test{$child}) {
										debug("Can't find jobname for child?<ignore>\n",2);
										next;
									} else {
										debug( "informed $child gone yeilding test $test{$child}{'program'}\n",2);
									}
	
									debug( "wait returned test<$currentgroup>\n",2);
									$currentgroup -= 1;
									debug( "wait returned test new size<$currentgroup>\n",2);
	
									my $test_name = $test{$child}{'testname'};
	
									StartTestOutput($compiler,$test_name);
	
									CompleteTestOutput($compiler,$test_name,$child,$status, $test{$child}{'program'}, $test{$child}{'workdir'});
									delete $test{$child};
									$hashsize = keys %test;
									debug("Tests remaining: $hashsize\n",3);
									last if $hashsize == 0;
									# if we have more tests fire off another
									# and don't wait for the last one
									debug("currenttest<$currenttest> testspercompiler<$testspercompiler>\n",2);
									last if $currenttest <= $testspercompiler;
	
								} # end while
								#next;
							} else {
								# batch size not met yet
								debug( "batch size not met yet: current group<$currentgroup>\n",2);
								sleep 1;
								#next;
							}
						} else {
						sleep 1;
						#next;
						}
					} else { # child
						# if we're the child, start test program
						DoChild($test_program, $test_retirement, $workdir);
					}
				}
				#*****************************************************************
	
				$repeatcounter = $repeatcounter + 1;
			}
		}
	
		# wait for each test to finish and print outcome
		quiet_debug("\n", 2);
	
		# complete the tests when batching them up if some are left
		$hashsize = keys %test;
		debug("At end of compiler dir hash size <<$hashsize>>\n",2);
		if(($kindwait == 0) && ($hashsize > 0)) {
			debug("At end of compiler dir about to wait\n",2);
			while( my $child = wait() ) {
				# if there are no more children, we're done
				last if $child == -1;
	
				# record the child's return status
				my $status = $?;
	
				$currentgroup -= 1;
		
				if(! defined $test{$child}) {
					debug("Can't find jobname for child?<ignore>\n",2);
					next;
				} else {
					debug( "informed $child gone yeilding test $test{$child}{'program'}\n",2);
				}
	
				my $test_name = $test{$child}{'testname'};
	
				StartTestOutput($compiler,$test_name);
	
				CompleteTestOutput($compiler,$test_name,$child,$status, $test{$child}{'program'}, $test{$child}{'workdir'});
				delete $test{$child};
				$hashsize = keys %test;
				debug("Tests remaining: $hashsize\n",2);
				last if $hashsize == 0;
			} # end while
		}
	
		quiet_debug("\n", 2);
		if($compiler ne "\.") {
			chdir ".." || die "error switching to directory ..: $!\n";
		}
		# remove compiler directory from path
		CleanFromPath("$compilerdir");
	} # end foreach compiler dir
	
	my $num_success = @successful_tests;
	my $num_failed = @failed_tests;
	
	quiet_debug("$num_success successful, $num_failed failed\n", 2);
	
	open( SUMOUTF, ">>successful_tests_summary" )
		|| die "error opening \"successful_tests_summary\": $!\n";
	open( OUTF, ">successful_tests" )
		|| die "error opening \"successful_tests\": $!\n";
	foreach my $test_name (@successful_tests)
	{
		print OUTF "$test_name 0\n";
		print SUMOUTF "$test_name 0\n";
	}
	close OUTF;
	close SUMOUTF;
	
	open( SUMOUTF, ">>failed_tests_summary$$" )
		|| die "error opening \"failed_tests_summary\": $!\n";
	open( OUTF, ">failed_tests" )
		|| die "error opening \"failed_tests\": $!\n";
	foreach my $test_name (@failed_tests)
	{
		print OUTF "$test_name 1\n";
		print SUMOUTF "$test_name 1\n";
	}
	close OUTF;
	close SUMOUTF;
	
	
	if ( $cleanupcondor )
	{
		my $pid;
		local *IN;
		if( open IN, '<', $condorpidfile) {
			$pid = <IN>;
			close IN;
			chomp $pid;
		}
		if(defined $pid and $pid !~ /^\d+$/) {
			print STDERR "PID file appears corrupt! Contains: $pid\n";
			$pid = undef;
		}
		if(not defined $pid) {
			print STDERR "PID file wasn't available; may not be able to shut down Condor.\n";
		}
		system("condor_off","-master");
		if($pid) {
			if( ! wait_for_process_gone($pid, 5) ) {
				kill('QUIT', $pid);
				if( ! wait_for_process_gone($pid, 5) ) {
					# TODO: More ruthlessly enumerate all of my children and kil them.
					kill('KILL', $pid);
					if( ! wait_for_process_gone($pid, 1) ) {
						print STDERR "Warning: Unable to shut down Condor daemons\n";
					}
				}
			}
		}
		unlink($condorpidfile) if -f $condorpidfile;
	}
	return $num_failed;
}

# Spin wait until $pid is no longer present or $max_wait (seconds) passes.
# Returns 1 if process exited, 0 if we timed out.
# Seconds are the smallest granularity.
# This uses the "kill(0)" trick; specifically, it's asking "Is this PID alive?"
# in a loop. On a very heavily loaded system with PIDs being reused very
# quickly, this could wait on the wrong process or processes.
sub wait_for_process_gone {
	my($pid, $max_wait) = @_;
	my $done_time = time() + $max_wait;
	while(1) {
		if(time() >= $done_time) { return 0; }
		if(kill(0, $pid)) { return 1; }
		select(undef,undef,undef, 0.01); # Sleep 0.01th of a second
	}
}

sub CleanFromPath
{
	my $pulldir = shift;
	my $path = $ENV{PATH};
	my $newpath = "";
	my @pathcomponents = split(/:/, $path);
	foreach my $spath ( @pathcomponents)
	{
		if($spath ne "$pulldir")
		{
			$newpath = "$newpath:$spath";
		}
	}
	$ENV{PATH} = $newpath;
}

sub IsPersonalTestDirThere
{
	my($dir) = @_;
	if( -d $dir ) {
		debug( "Test Personal Condor Directory ($dir) Established prior\n",2);
		return(0)
	} else {
		debug( "Test Personal Condor Directory ($dir) being Established now\n",2);
		mkpath($dir) || return 1;
		return(0)
	}
}

sub IsPersonalTestDirSetup
{
	my($dir) = @_;
	my $configfile = "$dir/condor_config";
	if(!(-f $configfile)) {
		return(0);
	}
	return(1);
}

sub WhereIsInstallDir
{
	my($iswindows) = @_;
	if($iswindows == 1) {
		my $top = getcwd();
		debug( "getcwd says: \"$top\"\n",2);
		my $crunched = get_win_path($top);
		debug( "cygpath changed it to: \"$crunched\"\n",2);
		my $ppwwdd = `pwd`;
		debug( "pwd says: \"$ppwwdd\"\n",2);
	}

	my $tmp = CondorTest::Which("condor_master");
	if ( ! ($tmp =~ /^\// ) ) {
		print STDERR "Unable to find a condor_master in your \$PATH!\n";
		exit(1);
	}
	CondorTest::fullchomp($tmp);
	debug( "Install Directory \"$tmp\"\n",2);
	my $installdir;
	my $wininstalldir;
	if($iswindows == 0) {
	    $tmp =~ s|//|/|g;
		if( ($tmp =~ /^(.*)\/sbin\/condor_master\s*$/) || \
			($tmp =~ /^(.*)\/bin\/condor_master\s*$/) ) {
			$installdir = $1;
			print "Testing This Install Directory: \"$installdir\"\n";
		}
		else {
		    die "'$tmp' didn't match path RE\n";
		}
	} else {
		if($tmp =~ /^(.*)\/bin\/condor_master\s*$/) {
			$installdir = $1;
			$wininstalldir = get_win_path($1);
			$wininstalldir =~ s/\\/\//;
			print "Testing This Install Directory: \"$wininstalldir\"\n";
		}
	}
	return($installdir, $wininstalldir);
}

# Try to make a directory.
# If the directory already exists, return.
# If the directory doesn't already exist, but we created it, return.
# If the directory doesn't already exist and cannot be create, terminate
# with a fatal error.
sub mkdir_or_die {
	my($dir) = @_;
	if(-d $dir) { return; }
	mkdir("$dir") || die "FATAL ERROR: Can't mkdir $dir: $!\n";
}

sub CreateLocal
{
	my($testpersonalcondorlocation) = @_;
	mkdir_or_die("$testpersonalcondorlocation/local");
	mkdir_or_die("$testpersonalcondorlocation/local/spool");
	mkdir_or_die("$testpersonalcondorlocation/local/execute");
	mkdir_or_die("$testpersonalcondorlocation/local/log");
	mkdir_or_die("$testpersonalcondorlocation/local/log/tmp");
}

sub CreateConfig
{
	my($targetconfig, $iswindows, $installdir, $localdir,
		$testpersonalcondorlocation, $want_core_dumps) = @_;
	# The only change we need to make to the generic configuration
	# file is to set the release-dir and local-dir. (non-windows)
	# change RELEASE_DIR and LOCAL_DIR
	my $currenthost = CondorTest::getFqdnHost();
	CondorTest::fullchomp($currenthost);

	debug( "Set RELEASE_DIR and LOCAL_DIR\n",2);

	# Windows needs config file preparation, wrapper scripts etc
	if($iswindows == 1) {
		# set up job wrapper
		# safe_copy("../condor_scripts/exe_switch.pl", "$wininstalldir/bin/exe_switch.pl") || die "couldn't copy exe_swtich.pl";
		# open( WRAPPER, ">$wininstalldir/bin/exe_switch.bat" ) || die "Can't open new job wrapper: $!\n";
		# print WRAPPER "\@c:\\perl\\bin\\perl.exe $wininstalldir/bin/exe_switch.pl %*\n";
		# close(WRAPPER);
		
		# pre-process config file src and windowize it

		# create config file with todd's awk script
		my $configcmd = "gawk -f $AWKSCRIPT $GENERICCONFIG";
		debug("awk cmd is $configcmd\n",2);

		open( OLDFIG, " $configcmd 2>&1 |")
			|| die "Can't run script file\"$configcmd\": $!\n";

	} else {
		open( OLDFIG, "<$GENERICCONFIG" )
			|| die "Can't open base config file: $!\n";
	}

	my $line = "";
	open( NEWFIG, ">$targetconfig" )
		|| die "Can't open new config file: $!\n";
	while( <OLDFIG> ) {
		CondorTest::fullchomp($_);
		$line = $_;
		if($line =~ /^RELEASE_DIR\s*=.*/) {
			debug( "Matching <<$line>>\n",2);
			print NEWFIG "RELEASE_DIR = $installdir\n";
		} elsif($line =~ /^LOCAL_DIR\s*=.*/) {
			debug( "Matching <<$line>>\n",2);
			print NEWFIG "LOCAL_DIR = $localdir\n";
		} elsif($line =~ /^LOCAL_CONFIG_FILE\s*=.*/) {
			debug( "Matching <<$line>>\n",2);
			print NEWFIG "LOCAL_CONFIG_FILE = $testpersonalcondorlocation/condor_config.local\n";
		} elsif($line =~ /^CONDOR_HOST\s*=.*/) {
			debug( "Matching <<$line>>\n",2);
			print NEWFIG "CONDOR_HOST = $currenthost\n";
		} elsif($line =~ /^ALLOW_WRITE\s*=.*/) {
			debug( "Matching <<$line>>\n",2);
			print NEWFIG "ALLOW_WRITE = *\n";
		} elsif($line =~ /NOT_RESPONDING_WANT_CORE\s*=.*/ and $want_core_dumps ) {
			debug( "Matching <<$line>>\n",2);
			print NEWFIG "NOT_RESPONDING_WANT_CORE = True\n";
		} elsif($line =~ /CREATE_CORE_FILES\s*=.*/ and $want_core_dumps ) {
			debug( "Matching <<$line>>\n",2);
			print NEWFIG "CREATE_CORE_FILES = True\n";
		} else {
			print NEWFIG "$line\n";
		}
	}
	close( OLDFIG );
	close( NEWFIG );
}

sub CreateLocalConfig
{
	my($targetconfiglocal, $iswindows, $installdir) = @_;
	debug( "Modifying local config file\n",2);
	my $logsize = 50000000;

	# make sure ports for Personal Condor are valid, we'll use address
	# files and port = 0 for dynamic ports...
	if($iswindows == 1) {
		# create config file with todd's awk script
		my $configcmd = "gawk -f $AWKSCRIPT $GENERICLOCALCONFIG";
		debug("gawk cmd is $configcmd\n",2);

		open( ORIG, " $configcmd 2>&1 |")
			|| die "Can't run script file\"$configcmd\": $!\n";

	} else {
		open( ORIG, "<$GENERICLOCALCONFIG" ) ||
			die "Can't open $GENERICLOCALCONFIG: $!\n";
	}
	open( FIX, ">$targetconfiglocal" ) ||
		die "Can't open $targetconfiglocal: $!\n";

	while( <ORIG> ) {
		print FIX;
	}
	close ORIG;

print FIX <<END;
COLLECTOR_HOST = \$(CONDOR_HOST):0
COLLECTOR_ADDRESS_FILE = \$(LOG)/.collector_address
NEGOTIATOR_ADDRESS_FILE = \$(LOG)/.negotiator_address
MASTER_ADDRESS_FILE = \$(LOG)/.master_address
STARTD_ADDRESS_FILE = \$(LOG)/.startd_address
SCHEDD_ADDRESS_FILE = \$(LOG)/.scheduler_address

# ADD size for log files and debug level
# default settings are in condor_config, set here to override
ALL_DEBUG               = D_FULLDEBUG D_SECURITY

MAX_COLLECTOR_LOG       = $logsize
COLLECTOR_DEBUG         = 

MAX_KBDD_LOG            = $logsize
KBDD_DEBUG              = 

MAX_NEGOTIATOR_LOG      = $logsize
NEGOTIATOR_DEBUG        = D_MATCH
MAX_NEGOTIATOR_MATCH_LOG = $logsize

MAX_SCHEDD_LOG          = 50000000
SCHEDD_DEBUG            = D_COMMAND

MAX_SHADOW_LOG          = $logsize
SHADOW_DEBUG            = D_FULLDEBUG

MAX_STARTD_LOG          = $logsize
STARTD_DEBUG            = D_COMMAND

MAX_STARTER_LOG         = $logsize

MAX_MASTER_LOG          = $logsize
MASTER_DEBUG            = D_COMMAND

# Add a shorter check time for periodic policy issues
PERIODIC_EXPR_INTERVAL = 15
PERIODIC_EXPR_TIMESLICE = .95
NEGOTIATOR_INTERVAL = 20
DAGMAN_USER_LOG_SCAN_INTERVAL = 1

# turn on soap for testing
ENABLE_SOAP             = TRUE
ALLOW_SOAP              = *
QUEUE_ALL_USERS_TRUSTED = TRUE

# condor_config.generic now contains a special value
# for ALLOW_WRITE which causes it to EXCEPT on submit
# till set to some legal value. Old was most insecure..
ALLOW_WRITE             = *
NUM_CPUS                = 15

# Allow a default heap size for java(addresses issues on x86_rhas_3)
# May address some of the other machines with Java turned off also
JAVA_MAXHEAP_ARGUMENT = 

# don't run benchmarks
RunBenchmarks = false
JAVA_BENCHMARK_TIME = 0
END

	if($iswindows == 1) {
		print FIX "JOB_INHERITS_STARTER_ENVIRONMENT = TRUE\n";
	}


	my $jvm = "";
	my $java_libdir = "$installdir/lib";
	my $exec_result;
	my $javabinary = "";
	if($iswindows == 1) {

		$javabinary = "java.exe";
		my $whichtest = `which $javabinary`;
		CondorTest::fullchomp ($whichtest);
		$whichtest =~ s/Program Files/progra~1/g;
		$jvm = get_win_path($whichtest);
		CondorTest::debug("which java found \"$jvm\"\n",2);

	} else {
		# below stolen from condor_configure

		my @default_jvm_locations = ("/bin/java",
			"/usr/bin/java",
			"/usr/local/bin/java",
			"/s/std/bin/java");

		$javabinary = "java";
		unless (system ("which java >> /dev/null 2>&1")) {
			CondorTest::fullchomp (my $which_java = CondorTest::Which("$javabinary"));
			CondorTest::debug("CT::Which for $javabinary said $which_java\n",2);
			@default_jvm_locations = ($which_java, @default_jvm_locations) unless ($?);
		}

		# check some default locations for java and pick first valid one
		foreach my $default_jvm_location (@default_jvm_locations) {
			CondorTest::debug("default_jvm_location is <<$default_jvm_location>>\n",2);
			if ( -f $default_jvm_location && -x $default_jvm_location) {
				$jvm = $default_jvm_location;
				print "Set JAVA to $jvm\n";
				last;
			}
		}
	}
	# if nothing is found, explain that, otherwise see if they just want to
	# accept what I found.
	debug ("Setting JAVA=$jvm",2);
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
	print FIX <<END;
START = TRUE
PREEMPT = FALSE
SUSPEND = FALSE
KILL = FALSE
WANT_SUSPEND = FALSE
WANT_VACATE = FALSE
COLLECTOR_NAME = Personal Condor for Tests
ALL_DEBUG = D_FULLDEBUG D_SECURITY
SCHEDD_INTERVAL_TIMESLICE = .99
END
	#insure path from framework is injected into the new pool
	if($iswindows == 0) {
		print FIX "environment=\"PATH=\'$mypath\'\"\n";
	}
	print FIX "SUBMIT_EXPRS=environment\n";
	print FIX "PROCD_LOG = \$(LOG)/ProcLog\n";
	if($iswindows == 1) {
		my $procdaddress = "buildandtestprocd$$";
		print FIX "PROCD_ADDRESS = \\\\.\\pipe\\$procdaddress\n";
	}

	close FIX;
}

sub IsPersonalRunning
{
	my($pathtoconfig, $iswindows) = @_;
	my $line = "";
	my $badness = "";
	my $matchedconfig = "";

	CondorTest::fullchomp($pathtoconfig);
	if($iswindows == 1) {
		$pathtoconfig =~ s/\\/\\\\/g;
	}

    open(CONFIG, "condor_config_val -config -master log 2>&1 |") || die "condor_config_val: $!\n";
    while(<CONFIG>) {
        CondorTest::fullchomp($_);
        $line = $_;
        debug ("--$line--\n",2);


		debug("Looking to match \"$pathtoconfig\"\n",2);
		if( $line =~ /^.*($pathtoconfig).*$/ ) {
			$matchedconfig = $1;
			debug ("Matched! $1\n",2);
			last;
		}
	}
	close(CONFIG);

	if( $matchedconfig eq "" ) {
		die	"lost: config does not match expected config setting......\n";
	}

	# find the master file to see if it exists and threrfore is running

	open(MADDR,"condor_config_val MASTER_ADDRESS_FILE 2>&1 |") || die "condor_config_val: $
	!\n";
	while(<MADDR>) {
		CondorTest::fullchomp($_);
		$line = $_;
		if($line =~ /^(.*master_address)$/) {
			if(-f $1) {
				debug("Master running\n",2);
				return(1);
			} else {
				debug("Master not running\n",2);
				return(0);
			}
		}
	}
	close(MADDR);
}

sub IsRunningYet
{

	my $daemonlist = `condor_config_val daemon_list`;
	CondorTest::fullchomp($daemonlist);

	if($daemonlist =~ /.*MASTER.*/) {
		# now wait for the master to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $masteradr = `condor_config_val MASTER_ADDRESS_FILE`;
		$masteradr =~ s/\012+$//;
		$masteradr =~ s/\015+$//;
		debug( "MASTER_ADDRESS_FILE is <<<<<$masteradr>>>>>\n",2);
		debug( "We are waiting for the file to exist\n",2);
		# Where is the master address file? wait for it to exist
		my $havemasteraddr = "no";
		while($havemasteraddr ne "yes") {
			debug( "Looking for $masteradr\n",2);
			if( -f $masteradr ) {
				debug("Found it!!!! master address file \n",2);
				$havemasteraddr = "yes";
			} else {
				sleep 1;
			}
		}
	}

	if($daemonlist =~ /.*COLLECTOR.*/) {
		# now wait for the collector to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $collectoradr = `condor_config_val COLLECTOR_ADDRESS_FILE`;
		$collectoradr =~ s/\012+$//;
		$collectoradr =~ s/\015+$//;
		debug( "COLLECTOR_ADDRESS_FILE is <<<<<$collectoradr>>>>>\n",2);
		debug( "We are waiting for the file to exist\n",2);
		# Where is the collector address file? wait for it to exist
		my $havecollectoraddr = "no";
		while($havecollectoraddr ne "yes") {
			debug( "Looking for $collectoradr\n",2);
			if( -f $collectoradr ) {
				debug("Found it!!!! collector address file\n",2);
				$havecollectoraddr = "yes";
			} else {
				sleep 1;
			}
		}
	}

	if($daemonlist =~ /.*NEGOTIATOR.*/) {
		# now wait for the negotiator to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $negotiatoradr = `condor_config_val NEGOTIATOR_ADDRESS_FILE`;
		$negotiatoradr =~ s/\012+$//;
		$negotiatoradr =~ s/\015+$//;
		debug( "NEGOTIATOR_ADDRESS_FILE is <<<<<$negotiatoradr>>>>>\n",2);
		debug( "We are waiting for the file to exist\n",2);
		# Where is the negotiator address file? wait for it to exist
		my $havenegotiatoraddr = "no";
		while($havenegotiatoraddr ne "yes") {
			debug( "Looking for $negotiatoradr\n",2);
			if( -f $negotiatoradr ) {
				debug("Found it!!!! negotiator address file\n",2);
				$havenegotiatoraddr = "yes";
			} else {
				sleep 1;
			}
		}
	}

	if($daemonlist =~ /.*STARTD.*/) {
		# now wait for the startd to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $startdadr = `condor_config_val STARTD_ADDRESS_FILE`;
		$startdadr =~ s/\012+$//;
		$startdadr =~ s/\015+$//;
		debug( "STARTD_ADDRESS_FILE is <<<<<$startdadr>>>>>\n",2);
		debug( "We are waiting for the file to exist\n",2);
		# Where is the startd address file? wait for it to exist
		my $havestartdaddr = "no";
		while($havestartdaddr ne "yes") {
			debug( "Looking for $startdadr\n",2);
			if( -f $startdadr ) {
				debug("Found it!!!! startd address file\n",2);
				$havestartdaddr = "yes";
			} else {
				sleep 1;
			}
		}
	}

	if($daemonlist =~ /.*SCHEDD.*/) {
		# now wait for the schedd to start running... get address file loc
		# and wait for file to exist
		# Give the master time to start before jobs are submitted.
		my $scheddadr = `condor_config_val SCHEDD_ADDRESS_FILE`;
		$scheddadr =~ s/\012+$//;
		$scheddadr =~ s/\015+$//;
		debug( "SCHEDD_ADDRESS_FILE is <<<<<$scheddadr>>>>>\n",2);
		debug( "We are waiting for the file to exist\n",2);
		# Where is the schedd address file? wait for it to exist
		my $havescheddaddr = "no";
		while($havescheddaddr ne "yes") {
			debug( "Looking for $scheddadr\n",2);
			if( -f $scheddadr ) {
				debug("Found it!!!! schedd address file\n",2);
				$havescheddaddr = "yes";
			} else {
				sleep 1;
			}
		}
	}

	return(1);
}
	
# StartTestOutput($compiler,$test_program);
sub StartTestOutput
{
	my $compiler = shift;
	my $test_program = shift;

	debug("StartTestOutput passed compiler<<$compiler>>\n",2);

	printf( "%-6s %-40s ", $compiler, $test_program );
}

# CompleteTestOutput($compiler,$test_program,$child,$status);
sub CompleteTestOutput
{
	my $compiler = shift;
	my $test_name = shift;
	my $child = shift;
	my $status = shift;
	my $test_program = shift;
	my $workdir = shift;
	my $failure = "";

	if( WIFEXITED( $status ) && WEXITSTATUS( $status ) == 0 )
	{
		print "succeeded\n";
		push @successful_tests, "$compiler/$test_name";
	} else {
		my $outpath = "$test_program.out";
		if(defined $workdir) { $outpath = "$workdir/$outpath"; }
		$failure = `grep 'FAILURE' $outpath`;
		$failure =~ s/^.*FAILURE[: ]//;
		CondorTest::fullchomp($failure);
		$failure = "failed" if $failure =~ /^\s*$/;
	
		print "$failure\n";
		push @failed_tests, "$compiler/$test_name";
	}
}

# ExtractExecutable($condor_submit_filename)
#
# extract the executable from the file $condor_submit_filename.
# $$(Arch) and $$(OpSys) will be expanded, based on the current
# platform
sub ExtractExecutable
{
	my($cmd) = @_;
	my $body = slurp($cmd);
	if(not defined $body) { die "Unable to read $cmd: $!"; }
	my($executable) = ($body =~ /^executable\s*=\s*(.*?)\s*$/im);
	my($arch) = `condor_config_val arch`;
	chomp($arch);
	my($opsys) = `condor_config_val opsys`;
	chomp($opsys);
	$executable =~ s/\$\$\(Arch\)/$arch/gi;
	$executable =~ s/\$\$\(OpSys\)/$opsys/gi;
	return $executable;
}

# DoChild($test_program, $test_retirement);
sub DoChild
{
	my $test_program = shift;
	my $test_retirement = shift;
	my $workdir = shift;
	my $repeat = shift;
	my $test_starttime = time();
	# with wrapping all test(most) in a personal condor
	# we know where the published directories are if we ask by name
	# and they are relevant for the entire test time. We need ask
	# and check only once.
	debug( "Test start @ $test_starttime \n",2);
	sleep(1);
	# add test core file

	$_ = $test_program;
	s/\.run//;
	my $testname = $_;

	if(-e $workdir) {
		die "$workdir already exists! Aborting to avoid stepping on old data!"
	}

	mkpath($workdir) or die "unable to mkpath($workdir): $!";

	my $log = "$testname.log";
	my $cmd = "$testname.cmd";
	my $out = "$testname.out";
	my $err = "$testname.err";
	my $runout = "$testname.run.out";
	my $cmdout = "$testname.cmd.out";

	my $isolate_test = not exists $ISOLATION_BLACKLIST{$testname};
	if($isolate_test) {
		link_or_copy_or_die($test_program, $workdir);

		# This is all backwards compatibility for older tests with multiple files.
		# Ideally it will eventually be phased out and older tests are updated.
		if(-e $cmd) {
			link_or_copy_or_die($cmd, $workdir);

			my $executable = ExtractExecutable($cmd);
			if($executable !~ /^\//) {
				link_or_copy_or_die($executable, $workdir);
			}
		}

		# End of backwards compatibility work.

		chdir($workdir) or die "Unable to isolate test $testname into $workdir: $!";
	}

	my $corecount = 0;
	my $res;
 	eval {
		alarm($test_retirement);
		my $cmd = "perl $test_program > $workdir/$test_program.out 2>&1";
		quiet_debug( "Child Starting: $cmd\n",2);
		$res = system($cmd);

		if(not $isolate_test) {
			move($log, $workdir);
			move($cmd, $workdir);
			move($out, $workdir);
			move($err, $workdir);
			move($cmdout, $workdir);
		}

		if($repeat > 1) {
			print "($$)";
		}

		if($res != 0) {
			#print "Perl test($test_program) returned <<$res>>!!! \n";
			exit(1);
		}
		exit(0);
	};

	if($@) {
		if($@ =~ /timeout/) {
			print "\n$test_program            Timeout\n";
			exit(1);
		} else {
			alarm(0);
			exit(0);
		}
	}
	exit(0);
}

# Call down to Condor Perl Module for now

sub debug
{
	my $string = shift;
	my $level = shift;
	my $newstring = "BT:$string";
	Condor::debug($newstring,$level);
}

# yates_shuffle(\@foo) random shuffle of array
sub yates_shuffle
{
	my $array = shift;
	my $i;
	for($i = @$array; --$i; ) {
		my $j = int rand ($i+1);
		next if $i == $j;
		@$array[$i,$j] = @$array[$j,$i];
	}
}

sub timestamp {
	return scalar localtime();
}

sub safe_copy {
	my( $src, $dest ) = @_;
	copy($src, $dest);
	if( $? >> 8 ) {
		print "Can't copy $src to $dest: $!\n";
		return 0;
	} else {
		debug("Copied $src to $dest\n",2);
		return 1;
	}
}

# When running under cygwin, given a cygwin/unix-esque path (/foo/bar), retrieve
# the equivalent Windows path, but using / instead of \ as a directory
# separator (C:/cygwin/foo/bar).  Relies on the cygpath tool.
#
# On non-windows systems, returns the input unmolested.
BEGIN {
my $iswindows;
sub get_win_path {
	if(not defined $iswindows) { $iswindows = CondorTest::IsThisWindows(); }
	if(not $iswindows) { return $_[0]; }
	my($in) = @_;
	my $out = `cygpath -m $in`;
	CondorTest::fullchomp($out);
	if(not defined $out or length($out) == 0) {
		die "Failed to locate Windows equivalent path for $in\n";
	}
	return $out;
}
};

BEGIN {
my $quiet = 0;

# Pass to debug, unless $quiet, in which case say nothing.
sub quiet_debug {
	if($quiet) { return; }
	debug(@_);
}

sub set_quiet {
	$quiet = $_[0];
}

};

# ($src, $dst) - Link or copy the _file_ $src into the directory $dst or the
# filename $dst.  Use a symbolic link if possible, failing that hard link or
# copy it.  Returns 1 on success, 0 on failure.
sub link_or_copy {
	my($src, $dst) = @_;
	if($src !~ /^\// and $src !~ /^.:/) {
		$src = getcwd()."/$src";
	}
	if(not -e $src) {
		print STDERR "Warning, asked to link_or_copy($src, $dst), but $src doesn't exist.\n";
		return 0;
	}
	if(-d $src) {
		die "Unable to link_or_copy the directory $src; only files are supported.";
	}
	$dst =~ s/\/$//; # Strip trailing slash.
	my $basename = $src;
	$basename =~ s/.*\///;
	if(-d $dst) {
		$dst .= "/$basename";
	}
	my $symlink_exists = eval { symlink("",""); 1 };
	if($symlink_exists) {
		if(symlink($src, $dst)) { return 1; }
	}
	# symlink doesn't work on this platform, or failed. Try a copy.
	if(copy($src, $dst)) { return 1; }
	return 0;
}

# ($src, $dst) - Link or copy the _file_ $src into the directory $dst or the
# filename $dst.  Use a symbolic link if possible, failing that hard link or
# copy it.  Returns 1 on success, dies on failure.
sub link_or_copy_or_die {
	my($src, $dst) = @_;
	if(link_or_copy(@_)) { return 1; }
	die "FATAL ERROR: link_or_copy from $src to $dst failed ($!)";
}

# Return to contents of the file specified as an argument.
# Returns undef on any errors.
sub slurp {
	my($filename) = @_;
	local *IN;
	if(not open IN, '<', $filename) { return undef; }
	local $/;
	my $body = <IN>;
	close IN;
	return $body;
}


sub usage_and_exit {
	print <<END;
the args:
-d[irectory <dir>: Test this compiler specific directory
-f[ile] <filename>: Read list of tests to run from this file
-i[gnore] <filename>: Read list of tests to skip from this file
-t[estname] <test-name>: Run this test
-q[uiet]: Reduce output
-m[arktime]: time stamp
-k[ind]: be kind and submit slowly
-e[venly]: <group size>: run a group of tests
-s[ort]: sort before running tests
-b[buildandtest]: set up a personal condor and generic configs
-w[wrap]: test in personal condor enable core/ERROR detection
-a[again]: how many times do we run each test?
-p[pretest]: Set up environment, but run no tests
-c[cleanup]: stop condor when test(s) finish.  Not used on windows atm.
--[no-]core: enable/disable core dumping <enabled>
END
	exit(0);
}

1;
