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
#
# V2.0 / 2000-May-31 / Peter Couvares / pfc@cs.wisc.edu
# V2.1 / 2004-Apr-29 / Becky Gietzel / bgietzel@cs.wisc.edu  
# Dec 03 : Added an xml output format, triggered by a command line switch, -xml
# Feb 04 : now you don't need to list all compilers to run/skip for a test, just add the test 
# Feb 05 : bt Explicit removal of . from the path and explicit addition
#	of test and sub test directories(during use only) in.
# make sure tests are called testname.run for skip and run files
# can use multiple command line options now
# Oct 06 : bt Make default mode with no args to be to search for compilers
#	and run .run files within BUT skip either test.saveme directories from
#	a previous local run AND pid specific subdirectories used to generate
#	personal condors for tests. Also "." will be added with a list
#	of tests based on current enabled test listed in "list_quick".
# Sept 07 : bt besides adding the -kind option to allow tests to be submitted
#	and run serially, I am having batch test look if it is currently running 
#	out of the generic personal condor that it knows how to configure around
#	the current installed binaries. We want this special test personal condor
#	to test with modified default config files eliminating possible
#	unique workspace settings and to have short update and negotiator cycles
#	etc. We will look for the existence of this location(TestingPersonalCondor)
# 	and see if it's CONDOR_CONFIG is using it and if its live now.
#	If all of those are not true, they will be remedied. Setting this up
# 	will be different for the nightlies then for a workspace.
# Nov 07 : Added repaeating a test n times by adding "-a n" to args
# Nov 07 : Added condor_personal setup only by adding -p (pretest work);
# Mar 17 : Added condor cleanup functionality by adding -c option.
# Dec 2008: working on concurrency testing. Here with -e 20 we try to keep
#	20 tests going at a time within the given compiler or toplevel
#	directory.
#

use strict;
use warnings;

use File::Copy;
use POSIX qw/sys_wait_h strftime/;
use Cwd;
use CondorTest;
use CondorUtils;

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

Condor::DebugOff();
Condor::DebugLevel(2);
CondorPersonal::DebugLevel(2);
CondorPersonal::DebugOff();
my @debugcollection = ();

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

my $time = strftime("%Y/%m/%d %H:%M:%S", localtime);
print "$time: batch_test.pl starting up ($^O perl)\n";

my $iswindows = CondorUtils::is_windows();
my $iscygwin  = CondorUtils::is_cygwin_perl();

# configuration options
my $test_retirement = 3600;	# seconds for an individual test timeout - 30 minutes
my $BaseDir = getcwd();
my $hush = 1;
my $cmd_prompt = 0;
my $sortfirst = 0;
my $timestamp = 0;
my $kindwait = 1; # run tests one at a time
my $groupsize = 0; # run tests in group for more throughput
my $currentgroup = 0;
my $repeat = 1; # run test/s repeatedly
my $cleanupcondor = 0;
my $want_core_dumps = 1;
my $testpersonalcondorlocation = "$BaseDir/TestingPersonalCondor";
my $wintestpersonalcondorlocation = "";
if($iswindows == 1) {
    if ($iscygwin) {
	my $tmp = `cygpath -m $testpersonalcondorlocation`;
	CondorUtils::fullchomp($tmp);
	$wintestpersonalcondorlocation = $tmp;
    } else {
	$wintestpersonalcondorlocation = $testpersonalcondorlocation;
	$wintestpersonalcondorlocation =~ s|/|\\|g;
    }
}

my $targetconfig = $testpersonalcondorlocation . "/condor_config";
my $targetconfiglocal = $testpersonalcondorlocation . "/condor_config.local";
my $condorpidfile = "/tmp/condor.pid.$$";
my @extracondorargs;

my $localdir = $testpersonalcondorlocation . "/local";
my $installdir;
my $wininstalldir; # need to have dos type paths for condor
my $testdir;
my $configmain = "../condor_examples/condor_config.generic";
my $configlocal = "../condor_examples/condor_config.local.central.manager";

my $wantcurrentdaemons = 1; # dont set up a new testing pool in condor_tests/TestingPersonalCondor
my $pretestsetuponly = 0; # only get the personal condor in place

my $isolated = 0;

# set up to recover from tests which hang
$SIG{ALRM} = sub { die "timeout" };

my @compilers;
my @successful_tests;
my @failed_tests;

# setup
STDOUT->autoflush();   # disable command buffering of stdout
STDERR->autoflush();   # disable command buffering of stderr
my $num_success = 0;
my $num_failed = 0;
my $isXML = 0;  # are we running tests with XML output

# remove . from path
CleanFromPath(".");
# yet add in base dir of all tests and compiler directories
$ENV{PATH} = $ENV{PATH} . ":" . $BaseDir;

#
# the args:

my $testfile = "";
my $ignorefile = "";
my @testlist;

# -c[md] create a command prompt with the testing environment (windows only)\n
# -d[irectory] <dir>: just test this directory
# -f[ile] <filename>: use this file as the list of tests to run
# -i[gnore] <filename>: use this file as the list of tests to skip
# -s[ort] sort tests before testing
# -t[estname] <test-name>: just run this test
# -q[uiet]: hush
# -m[arktime]: time stamp
# -k[ind]: be kind and submit slowly
# -b[buildandtest]: set up a personal condor and generic configs
# -w[wrap]: test in personal condor enable core/ERROR detection
# -a[again]: how many times do we run each test?
# -p[pretest]: get are environment set but run no tests
# -c[cleanup]: stop condor when test(s) finish.  Not used on windows atm.
#
while( $_ = shift( @ARGV ) ) {
  SWITCH: {
      if( /-h.*/ ) {
	  print "the args:\n";
	  print "-c[md] create a command prompt with the testing environment (windows only)\n";
	  print "-d[irectory] <dir>: just test this directory\n";
	  print "-f[ile] <filename>: use this file as the list of tests to run\n";
	  print "-i[gnore] <filename>: use this file as the list of tests to skip\n";
	  print "-t[estname] <test-name>: just run this test\n";
	  print "-q[uiet]: hush\n";
	  print "-m[arktime]: time stamp\n";
	  print "-k[ind]: be kind and submit slowly\n";
	  print "-e[venly]: <group size>: run a group of tests\n";
	  print "-s[ort] sort before running tests\n";
	  print "-b[buildandtest]: set up a personal condor and generic configs\n";
	  print "-w[wrap]: test in personal condor enable core/ERROR detection\n";
	  print "-xml: Output in xml\n";
	  print "-w[wrap]: test in personal condor enable core/ERROR detection\n";
	  print "-a[again]: how many times do we run each test?\n";
	  print "-p[pretest]: get are environment set but run no tests\n";
	  print "-c[cleanup]: stop condor when test(s) finish.  Not used on windows atm.\n";
	  print "--[no-]core: enable/disable core dumping <enabled>\n";
	  print "--[no-]debug: enable/disable test debugging <disabled>\n";
          print "--isolated: run tests in separate Condor instances\n";
          print "-v[erbose]: print debugging output\n";
	  exit(0);
      }
      if( /--debug/ ) {
	  next SWITCH;
      }
      if( /--no-debug/ ) {
	  next SWITCH;
      }
      if( /--core/ ) {
	  $want_core_dumps = 1;
	  next SWITCH;
      }
      if( /--no-core/ ) {
	  $want_core_dumps = 0;
	  next SWITCH;
      }
      if( /^-w.*/ ) {
	  $ENV{WRAP_TESTS} = "yes";
	  next SWITCH;
      }
      if( /^-c.*/ ) {
	  $cmd_prompt = 1;
	  next SWITCH;
      }
      if( /^-d.*/ ) {
	  push(@compilers, shift(@ARGV));
	  next SWITCH;
      }
      if( /^-f.*/ ) {
	  $testfile = shift(@ARGV);
	  next SWITCH;
      }
      if( /^-i.*/ ) {
	  $ignorefile = shift(@ARGV);
	  next SWITCH;
      }
      if( /^-r.*/ ) { #retirement timeout
	  $test_retirement = shift(@ARGV);
	  next SWITCH;
      }
      if( /^-s.*/ ) {
	  $sortfirst = 1;
	  next SWITCH;
      }
      if( /^-k.*/ ) {
	  $kindwait = 1;
	  next SWITCH;
      }
      if( /^-e.*/ ) {
	  $groupsize = shift(@ARGV);
	  $kindwait = 0;
	  print "kindwait set to 0 by -e switch\n";
	  next SWITCH;
      }
      if( /^-a.*/ ) {
	  $repeat = shift(@ARGV);
	  next SWITCH;
      }
      if( /^-b.*/ ) {
	  # start with fresh environment
	  $wantcurrentdaemons = 0;
	  next SWITCH;
      }
      if( /^-p.*/ ) {
	  # start with fresh environment
	  $wantcurrentdaemons = 0;
	  $pretestsetuponly = 1;
	  next SWITCH;
      }
      if( /^-t.*/ ) {
	  push(@testlist, shift(@ARGV));
	  next SWITCH;
      }
      if( /^-xml.*/ ) {
	  $isXML = 1;
	  debug("xml output format selected\n",2);
	  next SWITCH;
      }
      if( /^-q.*/ ) {
	  $hush = 1;
	  next SWITCH;
      }
      if( /^-m.*/ ) {
	  $timestamp = 1;
	  next SWITCH;
      }
      if( /^-c.*/ ) {
	  $cleanupcondor = 1;
	  push (@extracondorargs, "-pidfile $condorpidfile");
	  next SWITCH;
      }
      if( /^--isolated/ ) {
          $isolated = 1;
      }
      if( /^-v.*/ ) {
          Condor::DebugOn();
          Condor::DebugLevel(2);
          CondorPersonal::DebugOn();
          CondorPersonal::DebugLevel(2);
      }
    }
}


my %test_suite = ();

# If a Condor was requested we want to start one up...
if(!($wantcurrentdaemons)) {
    # ...unless isolation was requested.  Then we do one before running each test
    if(!$isolated) {
	start_condor();
    }
}

# If we are running isolated then we probably don't have a valid CONDOR_CONFIG set
# in the environment, therefore condor_config_val won't work.
if(!$isolated) {
    my @myfig = `condor_config_val -config 2>&1`;
    debug("Current config settings are:\n",2);
    foreach my $fig (@myfig) {
        debug("$fig\n",2);
    }
}

if($pretestsetuponly == 1) {
    # we have done the requested set up, leave
    exit(0);
}

#print "Ready for Testing\n";

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

#foreach my $name (@compilers) {
    #if($hush == 0) { 
	#print "Compiler:$name\n";
    #}
#}

# now we find the tests we care about.
if( @testlist ) {
    debug("Test list contents:\n", 2);
    
    # we were explicitly given a # list on the command-line
    foreach my $test (@testlist) {
        debug("    $test\n", 2);
        if($test !~ /.*\.run$/) {
            $test = "$test.run";
        }

        foreach my $compiler (@compilers) {
            push(@{$test_suite{$compiler}}, $test);
        }
    }
}
elsif( $testfile ) {
    debug("Using test file '$testfile'\n", 2);
    open(TESTFILE, '<', $testfile) || die "Can't open $testfile\n";
    while( <TESTFILE> ) {
        next if(/^\s*#/);  # Skip comment lines

        CondorUtils::fullchomp($_);
        
        my $test = $_;
        if($test !~ /.*\.run$/) {
            $test = "$test.run";
        }

        foreach my $compiler (@compilers) {
            push(@{$test_suite{$compiler}}, $test);
        }
    }
    close(TESTFILE);
}
else {
    # we weren't given any specific tests or a test list, so we need to 
    # find all test programs (all files ending in .run) for each compiler
    my $gotdot = 0;
    debug("working on default test list\n",2);
    foreach my $compiler (@compilers) {
	if($compiler eq ".") {
	    $gotdot = 1;
	}
        else {
	    if (-d $compiler) {
		opendir( COMPILER_DIR, $compiler ) || die "error opening \"$compiler\": $!\n";
		@{$test_suite{$compiler}} = grep /\.run$/, readdir( COMPILER_DIR );
		closedir COMPILER_DIR;
	    }
            else {
		print "Skipping unbuilt compiler dir: $compiler\n";
	    }
	}
    }
    # by default look at the current blessed tests in the top
    # level of the condor_tests directory and run these.
    my @toptests;

    if($iswindows == 1) {
	open(QUICK, '<', "Windows_list") || die "Can't open Windows_list\n";
    }
    else {
	open(QUICK, '<', "list_quick") || die "Can't open list_quick\n";
    }

    while(<QUICK>) {
	CondorUtils::fullchomp($_);
        next if(/^#/ );
	push @toptests, "$_.run";
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
	CondorUtils::fullchomp($_);
	my $test = $_;
	foreach my $compiler (@compilers) {
	    # $skip_hash{"$compiler"}->{"$test"} = 1;
	    #@{$test_suite{"$compiler"}} = grep !/$test\.run/, @{$test_suite{"$compiler"}};
	    @{$test_suite{"$compiler"}} = grep !/$test/, @{$test_suite{"$compiler"}};
	} 
    }
    close(SKIPFILE);
}

my $ResultDir;
# set up base directory for storing test results
if ($isXML){
    CondorTest::verbose_system ("mkdir -p $BaseDir/results",{emit_output=>0});
    $ResultDir = "$BaseDir/results";
    open( XML, ">$ResultDir/ncondor_testsuite.xml" ) || die "error opening \"ncondor_testsuite.xml\": $!\n";
    print XML "<\?xml version=\"1.0\" \?>\n<test_suite>\n";
}

# Now we'll run each test.
#print "Testing: " . join(" ", @compilers) . "\n";

my $hashsize = 0;
my %test;
my $reaped = 0;

foreach my $compiler (@compilers) {
    # as long as we have tests to start, loop back again and start
    # another when we are trying to keep N running at once
    my $testspercompiler = $#{$test_suite{$compiler}} + 1;
    my $currenttest = 0;

    #debug("Compiler/Directory <$compiler> has $testspercompiler tests\n",2); 
    if ($isXML){
		CondorTest::verbose_system ("mkdir -p $ResultDir/$compiler",{emit_output=>0});
    } 
    if($compiler ne "\.") {
	# Meh, if the directory isn't there, just skip it instead of bailing.
    	chdir $compiler || (print "Skipping $compiler directory\n" && next);
    }
    my $compilerdir = getcwd();
    # add in compiler dir to the current path
    $ENV{PATH} = $ENV{PATH} . ":" . $compilerdir;

    # fork a child to run each test program
    if($hush == 0) { 
    	print "submitting $compiler tests\n";
    }

    # if batching tests, randomize order
    if(($groupsize > 0) && ($sortfirst == 0)){
		yates_shuffle(\@{$test_suite{"$compiler"}});
    }
    my @currenttests = @{$test_suite{"$compiler"}};
    if($sortfirst == 1) {
		@currenttests = sort @currenttests;
    }
    foreach my $test_program (@currenttests) {
        #print "----------------------------\n";
        #print "Starting test: $test_program\n";
		debug(" *********** Starting test: $test_program *********** \n",2);

		# doing this next test
		$currenttest = $currenttest + 1;

		if(($hush == 0) && ($kindwait == 0)) { 
	    	print ".";
		}
		debug("Want to test $test_program\n",2);

        #next if $skip_hash{$compiler}->{$test_program};

		# allow multiple runs easily
		my $repeatcounter = 0;
		#if( $hush == 0 ) {
	    	#debug("Want $repeat runs of each test\n",3);
		#}
		while($repeatcounter < $repeat) {
	    	if($isolated) {
                my $state_dir = "$BaseDir/$test_program.$repeatcounter";
                if($compiler ne ".") {
                    $state_dir .= ".$compiler";
                }
                my $time = time();
                start_condor("$state_dir.$time.saveme");
	    	}

	    	#debug( "About to fork test<$currentgroup>\n",2);
	    	$currentgroup += 1;
			#print "currentgroup now <$currentgroup>\n";
	    	#debug( "About to fork test new size<$currentgroup>\n",2);
			#print "About to fork...........\n";
	    	my $pid = fork();
	    	if( $hush == 0 ) {
				debug( "forking for $test_program pid returned is $pid\n",3);
	    	}
			#print "forking for $test_program pid returned is $pid\n";
	
			#debug_flush();
	    	die "error calling fork(): $!\n" unless defined $pid;

	    	# two modes 
	    	#		kindwait = resolve each test after the fork
	    	#		else:	   fork them all and then wait for all
	
	    	if( $kindwait == 1 ) {
			#*****************************************************************
				if( $pid > 0 ) {
		    		$test{$pid} = "$test_program";
		    		debug( "Started test: kindwait: $test_program/$pid\n",2);
		
		    		# Wait for job before starting the next one
	
					#print "Calling StartTestOutput $compiler/$test_program <$pid>\n";
		    		StartTestOutput($compiler,$test_program);
		
		    		#print "kindwait: Waiting on test\n";
		    		wait_for_test_children(\%test, $compiler,
					   	suppress_start_test_output=>1);
				} else {
		    		# if we're the child, start test program
					#print "DoChild kindwait $test_program/$test_retirement\n";
		    		DoChild($test_program, $test_retirement);
				}
			#*****************************************************************
		    } else {
				if( $pid > 0 ) {
			    	#$test{$pid} = "$test_program <$pid>";
			    	$test{$pid} = "$test_program";
			    	if( $hush == 0 ) {
						debug( "Started test: $test_program/$pid\n",2);
						print "Started test: $test_program/$pid\n";
			    	}
			    	# are we submitting all the tests for a compiler and then
			    	# waiting for them all? Or are we submitting a bunch and waiting
			    	# for them before submitting some more.
			    	if($groupsize != 0) {
						debug( "current group: $currentgroup Limit: $groupsize\n",2);
						#print  "current group: $currentgroup Limit: $groupsize\n";
						if($currentgroup == $groupsize) {
				    		debug( "wait for batch\n",2);
							#print "wait for batch as $currentgroup == $groupsize\n";
				    		my $max_to_reap = 0; # unlimited;
							#print "test number $currenttest batch size $testspercompiler\n";
				    		if($currenttest <= $testspercompiler) {
								#print "currenttest<$currenttest> >= testcompiler:$testspercompiler> maxreap now 1\n";
								$max_to_reap = 1;
				    		}
				    		#my $reaped = wait_for_test_children(\%test, 
									#$compiler, max_to_reap => $max_to_reap);
				    		$reaped = wait_for_test_children(\%test, 
									$compiler);
				    		debug( "wait returned test<$currentgroup>\n",2);
				    		$currentgroup -= $reaped;
				    		debug( "wait returned test new size<$currentgroup>\n",2);
				    		debug("currenttest<$currenttest> testspercompiler<$testspercompiler>\n",2);
			
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
			    	DoChild($test_program, $test_retirement,$currentgroup);
				}
			}
		    #*****************************************************************

	    	$repeatcounter = $repeatcounter + 1;

            if($isolated) {
                stop_condor();
            }
		} #end of repeater loop
    } # end of foreach $test_program

    # wait for each test to finish and print outcome
    if($hush == 0) { 
    	print "\n";
    }

    # complete the tests when batching them up if some are left
    $hashsize = keys %test;
    debug("At end of compiler dir hash size <<$hashsize>>\n",2);
    if(($kindwait == 0) && ($hashsize > 0)) {
		debug("At end of compiler dir about to wait\n",2);
		$reaped = wait_for_test_children(\%test, $compiler);
		$currentgroup -= $reaped;
    }

    if($hush == 0) {
    	print "\n";
    }
    if($compiler ne "\.") {
    	chdir ".." || die "error switching to directory ..: $!\n";
    }
    # remove compiler directory from path
    CleanFromPath("$compilerdir");
} # end foreach compiler dir

if ($isXML){
    print XML "</test_suite>\n";
    close (XML);
}


if( $hush == 0 ) {
    print "$num_success successful, $num_failed failed\n";
}

if($num_failed > 0) {
	debug_flush();
}

open( SUMOUTF, ">>successful_tests_summary" )
    || die "error opening \"successful_tests_summary\": $!\n";
open( OUTF, ">successful_tests" )
    || die "error opening \"successful_tests\": $!\n";
foreach my $test_name (@successful_tests)
{
    print OUTF "$test_name 0\n";
    print SUMOUTF "$test_name 0\n";
	print "$test_name passed\n";
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
	print "$test_name failed\n";
}
close OUTF;
close SUMOUTF;


if ( $cleanupcondor ) {
    stop_condor();
}

exit $num_failed;


sub start_condor {
    if($isolated) {
		$testpersonalcondorlocation = $_[0];
		if($iswindows == 1) {
	    	if ($iscygwin) {
				$wintestpersonalcondorlocation = `cygpath -m $testpersonalcondorlocation`;
				CondorUtils::fullchomp($wintestpersonalcondorlocation);
	    	} else {
				($wintestpersonalcondorlocation = $testpersonalcondorlocation) =~ s|/|\\|g;
	    	}
		}
		$targetconfig      = "$testpersonalcondorlocation/condor_config";
		$targetconfiglocal = "$testpersonalcondorlocation/condor_config.local";
		$localdir          = "$testpersonalcondorlocation/local";

        $condorpidfile     = "$testpersonalcondorlocation/.pidfile";
        push(@extracondorargs, "-pidfile $condorpidfile");
    }
    
    my $awkscript = "../condor_examples/convert_config_to_win32.awk";
    my $genericconfig = "../condor_examples/condor_config.generic";
    my $genericlocalconfig = "../condor_examples/condor_config.local.central.manager";

    if( -d $testpersonalcondorlocation ) {
		#debug( "Test Personal Condor Directory Established prior\n",2);
    }
    else {
		#debug( "Test Personal Condor Directory being Established now\n",2);
		system("mkdir -p $testpersonalcondorlocation");
    }

    WhereIsInstallDir();

    my $res = IsPersonalTestDirSetup();
    if($res == 0) {
		debug("Need to set up config files for test condor\n",2);
		CreateConfig($awkscript, $genericconfig);
		CreateLocalConfig($awkscript, $genericlocalconfig);
		CreateLocal();
    }

    if($iswindows == 1) {
	my $tmp = $targetconfig;
	if ($iscygwin) {
	    $tmp = `cygpath -m $targetconfig`;
	    CondorUtils::fullchomp($tmp);
	}
	$ENV{CONDOR_CONFIG} = $tmp;
	print "setting CONDOR_CONFIG=$tmp\n";
	# need to know if there is already a personal condor running 
	# at CONDOR_CONFIG so we know if we have to uniqify ports & pipes
    }
    else {
		$ENV{CONDOR_CONFIG} = $targetconfig;
    }

    $res = 0;
    if(!$isolated) {
        $res = CondorPersonal::IsRunningYet($targetconfig);
    }

    chdir("$BaseDir");

    if($res == 0) {
		debug("Starting Personal Condor\n",2);
		unlink("$testpersonalcondorlocation/local/log/.master_address");
		unlink("$testpersonalcondorlocation/local/log/.collector_address");
		unlink("$testpersonalcondorlocation/local/log/.negotiator_address");
		unlink("$testpersonalcondorlocation/local/log/.startd_address");
		unlink("$testpersonalcondorlocation/local/log/.schedd_address");

		if($iswindows == 1) {
	    	my $mcmd = "$wininstalldir/bin/condor_master.exe -f &";
	    	if ($iscygwin) {
				$mcmd =~ s|\\|/|g;
	    	}
            else {
				my $keep = ($cmd_prompt) ? "/k" : "/c";
				#$mcmd = "$ENV[COMSPEC] /c \"$wininstalldir\\bin\\condor_master.exe\" -f"; 
				$mcmd = "cmd /s $keep start /b $wininstalldir\\bin\\condor_master.exe -f"; 
	    	}
	    	#debug( "Starting master like this:\n",2);
	    	#debug( "\"$mcmd\"\n",2);
	    	CondorTest::verbose_system("$mcmd",{emit_output=>0,use_system=>1});
		}
        else {
	    	CondorTest::verbose_system("$installdir/sbin/condor_master @extracondorargs -f &",{emit_output=>0,use_system=>1});
		}
		#debug("Done Starting Personal Condor\n",2);
    }
    CondorPersonal::IsRunningYet() || die "Failed to start Condor";
}


sub stop_condor {
    my $pid = undef;
    local *IN;
    if( open(IN, '<', $condorpidfile)) {
	$pid = <IN>;
	close IN;
	chomp($pid);
    }

    if(not defined $pid) {
	print STDERR "PID file wasn't available; may not be able to shut down Condor.\n";
    }
    elsif($pid !~ /^\d+$/) {
	print STDERR "PID file appears corrupt! Contains: $pid\n";
	$pid = undef;
    }

	#shhhhhhhhh
    #system("condor_off","-master");
	my @condoroff = `condor_off -master`;

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
    my @pathcomponents = split /:/, $path;
    foreach my $spath ( @pathcomponents)
    {
	if($spath ne "$pulldir")
	{
	    $newpath = $newpath . ":" . $spath;
	}
    }
}

sub IsPersonalTestDirSetup
{
    my $configfile = $testpersonalcondorlocation . "/condor_config";
    if(!(-f $configfile)) {
	return(0);
    }
    return(1);
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
	print STDERR "CondorTest::Which($master_name) returned <<$tmp>>\n";
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

sub CreateLocal
{
    if( !(-d "$testpersonalcondorlocation/local")) {
	mkdir( "$testpersonalcondorlocation/local", 0777 ) 
	    || die "Can't mkdir $testpersonalcondorlocation/local: $!\n";
    }
    if( !(-d "$testpersonalcondorlocation/local/spool")) {
	mkdir( "$testpersonalcondorlocation/local/spool", 0777 ) 
	    || die "Can't mkdir $testpersonalcondorlocation/local/spool: $!\n";
    }
    if( !(-d "$testpersonalcondorlocation/local/execute")) {
	mkdir( "$testpersonalcondorlocation/local/execute", 0777 ) 
	    || die "Can't mkdir $testpersonalcondorlocation/local/execute: $!\n";
    }
    if( !(-d "$testpersonalcondorlocation/local/log")) {
	mkdir( "$testpersonalcondorlocation/local/log", 0777 ) 
	    || die "Can't mkdir $testpersonalcondorlocation/local/log: $!\n";
    }
    if( !(-d "$testpersonalcondorlocation/local/log/tmp")) {
	mkdir( "$testpersonalcondorlocation/local/log/tmp", 0777 ) 
	    || die "Can't mkdir $testpersonalcondorlocation/local/log: $!\n";
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
	    debug("Matching <<$_>>\n", 2);
	    if($iswindows == 1) {
		print NEWFIG "RELEASE_DIR = $wininstalldir\n";
	    }
	    else {
		print NEWFIG "RELEASE_DIR = $installdir\n";
	    }
	}
	elsif(/^LOCAL_DIR\s*=/) {
	    debug("Matching <<$_>>\n", 2);
	    if($iswindows == 1) {
		print NEWFIG "LOCAL_DIR = $wintestpersonalcondorlocation/local\n";
	    }
	    else {
		print NEWFIG "LOCAL_DIR = $localdir\n";        
	    }
	}
	elsif(/^LOCAL_CONFIG_FILE\s*=/) {
	    debug( "Matching <<$_>>\n",2);
	    if($iswindows == 1) {
		print NEWFIG "LOCAL_CONFIG_FILE = $wintestpersonalcondorlocation/condor_config.local\n";
	    }
	    else {
		print NEWFIG "LOCAL_CONFIG_FILE = $testpersonalcondorlocation/condor_config.local\n";
	    }
	}
	elsif(/^CONDOR_HOST\s*=/) {
	    debug( "Matching <<$_>>\n",2);
	    print NEWFIG "CONDOR_HOST = $currenthost\n";
	}
	elsif(/^ALLOW_WRITE\s*=/) {
	    debug( "Matching <<$_>>\n",2);
	    print NEWFIG "ALLOW_WRITE = *\n";
	}
	elsif(/NOT_RESPONDING_WANT_CORE\s*=/ and $want_core_dumps ) {
	    debug( "Matching <<$_>>\n",2);
	    print NEWFIG "NOT_RESPONDING_WANT_CORE = True\n";
	}
	elsif(/CREATE_CORE_FILES\s*=/ and $want_core_dumps ) {
	    debug( "Matching <<$_>>\n",2);
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
    print FIX "ALL_DEBUG               = D_FULLDEBUG D_SECURITY D_HOSTNAME\n";

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
	    my $whichtest = `which $javabinary`;
	    CondorUtils::fullchomp($whichtest);
	    $whichtest =~ s/Program Files/progra~1/g;
	    $jvm = `cygpath -m $whichtest`;
	    CondorUtils::fullchomp($jvm);
	}
	CondorTest::debug("which java said<<$jvm>>\n",2);

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

# StartTestOutput($compiler,$test_program);
sub StartTestOutput
{
    my $compiler = shift;
    my $test_program = shift;

    debug("StartTestOutput passed compiler<<$compiler>>\n",2);

    if ($isXML){
		print XML "<test_result>\n<name>$compiler.$test_program</name>\n<description></description>\n";
		printf( "%-40s ", $test_program );
    } else {
		#printf( "%-6s %-40s ", $compiler, $test_program );
    }
}

# CompleteTestOutput($compiler,$test_program,$child,$status);
sub CompleteTestOutput
{
    my $compiler = shift;
    my $test_name = shift;
    my $child = shift;
    my $status = shift;
    my $failure = "";

	debug(" *********** Completing test: $test_name *********** \n",2);
    if( WIFEXITED( $status ) && WEXITSTATUS( $status ) == 0 )
    {
		if ($isXML){
	    	print XML "<status>SUCCESS</status>\n";
			if($groupsize == 0) {
	    		print "succeeded\n";
			} else {
				#print "Xml: group size <$groupsize> test <$test_name>\n";
	    		print "$test_name succeeded\n";
			}
		} else {
			if($groupsize == 0) {
	    		print "succeeded\n";
			} else {
				#print "Not Xml: group size <$groupsize> test <$test_name>\n";
	    		print "$test_name succeeded\n";
			}
		}
		$num_success++;
		@successful_tests = (@successful_tests, "$compiler/$test_name");
    } else {
		my $testname = "$test{$child}";
		$testname = $testname . ".out";
		$failure = `grep 'FAILURE' $testname`;
		$failure =~ s/^.*FAILURE[: ]//;
		CondorUtils::fullchomp($failure);
		$failure = "failed" if $failure =~ /^\s*$/;
		
		if ($isXML){
	    	print XML "<status>FAILURE</status>\n";
	    	print "$failure\n";
		} else {
	    	print "$failure\n";
		}
		$num_failed++;
		@failed_tests = (@failed_tests, "$compiler/$test_name");
    }

    if ($isXML){
	print "Copying to $ResultDir/$compiler ...\n";
	
	# possibly specify exact files in future - for now bring back all 
	#system ("cp $test_name.run.out $ResultDir/$compiler/.");
	system ("cp $test_name.* $ResultDir/$compiler/.");
	
	# uncomment this when we decide to have each test tar itself up when it finishes
	#system ("cp $test_name.tar $ResultDir/$compiler/.");
	
	print XML "<data_file>$compiler.$test_name.run.out</data_file>\n<error>";
	print XML "</error>\n<output>";
	print XML "</output>\n</test_result>\n";
    }
}

# DoChild($test_program, $test_retirement,groupmemebercount);
sub DoChild
{
    my $test_program = shift;
    my $test_retirement = shift;
	my $test_id = shift;
	my $id = 0;

	if(defined $test_id) {
		print "Starting batch id <$test_id> PID <$$>\n";
		$id = $test_id;
		print "ID = <$id>\n";
	}
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
    my $save = $testname . ".saveme";
    my $piddir = $save . "/$$";
    # make sure pid storage directory exists
    verbose_system("mkdir -p $save",{emit_output=>0});
    my $pidcmd = "mkdir -p " . $save . "/" . "$$";
    verbose_system("$pidcmd",{emit_output=>0});

    my $log = "";
    my $cmd = "";
    my $out = "";
    my $err = "";
    my $runout = "";
    my $cmdout = "";


    # before starting test clean trace of earlier run
    my $rmcmd = "rm -f $log $out $err $runout $cmdout";
    CondorTest::verbose_system("$rmcmd",{emit_output=>0});

    my $corecount = 0;
    my $res;
    eval {
	alarm($test_retirement);
	if(defined $test_id) {
    	$testname . ".$test_id" . ".log";
    	$testname . ".$test_id" . ".cmd";
    	$testname . ".$test_id" . ".out";
    	$testname . ".$test_id" . ".err";
    	$testname . ".$test_id" . ".run.out";
    	$testname . ".$test_id" . ".cmd.out";

		if( $hush == 0 ) {
	    	debug( "Child Starting:perl $test_program > $test_program.$test_id.out\n",2);
		}
		$res = system("perl $test_program > $test_program.$test_id.out 2>&1");
	} else {
    	$testname . ".log";
    	$testname . ".cmd";
    	$testname . ".out";
    	$testname . ".err";
    	$testname . ".run.out";
    	$testname . ".cmd.out";

		if( $hush == 0 ) {
	    	debug( "Child Starting:perl $test_program > $test_program.out\n",2);
		}
		$res = system("perl $test_program > $test_program.out 2>&1");
	}

    my $newlog =  $piddir . "/" . $log;
    my $newcmd =  $piddir . "/" . $cmd;
    my $newout =  $piddir . "/" . $out;
    my $newerr =  $piddir . "/" . $err;
    my $newrunout =  $piddir . "/" . $runout;
    my $newcmdout =  $piddir . "/" . $cmdout;


	# generate file names

	copy($log, $newlog);
	copy($cmd, $newcmd);
	copy($out, $newout);
	copy($err, $newerr);
	copy($runout, $newrunout);
	copy($cmdout, $newcmdout);

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
sub debug {
    my ($msg, $level) = @_;
	my $time = Condor::timestamp();
	chomp($time);
	push @debugcollection, "$time: $msg";
    Condor::debug("batch_test(L=$level) - $msg", $level);
}

sub debug_flush {
    print "\n\n\n---------------------------- Flushing Test Info Cache ----------------------------\n";
	foreach my $line (@debugcollection) {
		print "$line";
	}
    print "---------------------------- Done Flushing Test Info Cache ----------------------------\n\n\n\n";
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

# Returns a string describing the exit status.  The general form will be
# "exited with value 4", "exited with signal 7", or "exited with signal 11
# leaving a core file" and thus is suitable for concatenating into larger
# messages.
sub describe_exit_status {
    my($status) = @_;
    my $msg = 'exited with ';
    if($status & 127) {
	$msg .= sprintf('signal %d%s', $status & 127,
			($status & 128) ? 'leaving a core file': '');
    } else {
	$msg .= 'value '.($status >> 8);
    }
    return $msg;
}

# Wait for and reap child processes, taking particular note of children that 
# are tests.
#
# arguments:
#   $test - reference to the %test has mapping PIDs to test names.
#           Reaped tests will be deleted from this hash.
#   $compiler - the compiler name/string 
#   %options - All elements are optional
#      - suppress_start_test_output=>1 - don't call StartTestOutput on
#                  each test as it exits.  Defaults to calling StartTestOutput
#      - max_to_reap - Maximum tests to reap. If not specified or 0,
#                  will reap until no children are left.
sub wait_for_test_children {
    my($test, $compiler, %options) = @_;

    my($max_to_reap) = $options{'max_to_reap'};
    my($suppress_start_test_output) = $options{'suppress_start_test_output'};

    my $tests_reaped = 0;

	if(defined $max_to_reap) {
		#print "wait_for_test_children: max to reap set: $max_to_reap = <$max_to_reap>\n";
	}
	if(defined $suppress_start_test_output) {
		#print "wait_for_test_children: suppress start test output: $suppress_start_test_output = <$suppress_start_test_output>\n";
	}

	$hashsize = keys %{$test};
	debug("Tests remaining: $hashsize\n",2);
	#print "Hash size of tests = <$hashsize>\n";

    while( my $child = wait() ) {

		#print "Caught pid <$child>\n";
		# if there are no more children, we're done
		if ($child == -1) {
			print "wait_for_test_children: wait returned -1, done\n";
		}

		# record the child's return status
		my $status = $?;
	
		my $debug_message = "Child PID $child ".describe_exit_status($status);
	
		#print "wait_for_test_children: debug message\n";
		# ignore spurious children
		if(! defined $test->{$child}) {
	    	debug($debug_message.". It was not known. Ignoring.\n", 2);
			#print "Test not known<$test->{$child}>.....\n";
	    	next;
		} else {
	    	debug($debug_message.". Test: $test->{$child}.\n", 2);
			print "processing PID <$child>: ";
			#print "Test known: $test->{$child}\n";
		}

		$tests_reaped++;
		#print "Reaping test:\n";

		#finally
		#(my $test_name) = $test->{$child} =~ /(.*)\.run$/;
		my $hashnamefortest = "";
		$hashnamefortest = $test->{$child};
		my $test_name = "";
		if($hashnamefortest =~ /^(.*?)\.run.*$/) {
			$test_name = $1;
			#print "Set Test Name as $test_name\n";
		} else {
			print "Name of the test not meeting test.run format<$hashnamefortest>\n";
		}

		debug( "Done waiting on test $test_name\n",3);
		my $tname = "pid $child indexed hash value - $test->{$child}\n";
		#print "Name from hash is <$tname>\n";
		#print "Done waiting on test $test_name\n";

		StartTestOutput($compiler, $test_name) 
	    	unless $suppress_start_test_output;

		CompleteTestOutput($compiler, $test_name, $child, $status);
		delete $test->{$child};
		$hashsize = keys %{$test};
		debug("Tests remaining: $hashsize\n",2);
		#print "Hash size of tests = <$hashsize>\n";
		last if $hashsize == 0;

		if((defined $max_to_reap) and ($max_to_reap != 0) and ($max_to_reap <= $tests_reaped)){
			print "Leaving wait_for_test_children max_to_read <$max_to_reap> tests reaped <$tests_reaped>\n";
			last;
		}
		#last if defined $max_to_reap 
	    	#and $max_to_reap != 0
	    	#and $max_to_reap <= $tests_reaped;
    }

	#print "Tests Reaped = <$tests_reaped>\n";
    return $tests_reaped;
}

1;
