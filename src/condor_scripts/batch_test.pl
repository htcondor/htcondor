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
# 	will be different for the nightlies then for a workspace. However if 
# 	we look at our current location and we find an "execute" directory in the
#	path, we can assume it is a nightly test run.
# Nov 07 : Added repaeating a test n times by adding "-a n" to args
# Nov 07 : Added condor_personal setup only by adding -p (pretest work);
# Mar 17 : Added condor cleanup functionality by adding -c option.
#

#require 5.0;
use File::Copy;
use FileHandle;
use POSIX "sys_wait_h";
use Cwd;
use CondorTest;


#my $LogFile = "batch_test.log";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR); $| = 1;
#select(STDOUT); $| = 1;

# configuration options
$test_dir = ".";            # directory in which to find test programs
$test_retirement = 1800;	# seconds for an individual test timeout - 30 minutes
my $BaseDir = getcwd();
$hush = 0;
$timestamp = 0;
$kindwait = 1; # run tests one at a time
$repeat = 1; # run test/s repeatedly
$cleanupcondor = 0;
$nightly;
$testpersonalcondorlocation = "$BaseDir/TestingPersonalCondor";
$wintestpersonalcondorlocation = "";
{
	$tmp = `cygpath -w $testpersonalcondorlocation`;
	chomp($tmp);
	$wintestpersonalcondorlocation = $tmp;
}

$targetconfig = $testpersonalcondorlocation . "/condor_config";
$targetconfiglocal = $testpersonalcondorlocation . "/condor_config.local";
$condorpidfile = "/tmp/condor.pid.$$";
@extracondorargs;

$localdir = $testpersonalcondorlocation . "/local";
$installdir;
$wininstalldir; # need to have dos type paths for condor
$testdir;
$testdirsetup;
$testdirconfig;
$testdirrunning;
$configmain;
$configlocal;
$iswindows = 0;

$wantcurrentdaemons = 1; # dont set up a new testing pool in condor_tests/TestingPersonalCondor
$pretestsetuponly = 0; # only get the personal condor in place

# set up to recover from tests which hang
$SIG{ALRM} = sub { die "timeout" };

# setup
STDOUT->autoflush();   # disable command buffering of stdout
STDERR->autoflush();   # disable command buffering of stderr
$num_success = 0;
$num_failed = 0;
$isXML = 0;  # are we running tests with XML output

# remove . from path
CleanFromPath(".");
# yet add in base dir of all tests and compiler directories
$ENV{PATH} = $ENV{PATH} . ":" . $BaseDir;

#
# the args:
# -d[irectory <dir>: just test this directory
# -f[ile] <filename>: use this file as the list of tests to run
# -s[kip] <filename>: use this file as the list of tests to skip
# -t[estname] <test-name>: just run this test
# -q[uiet]: hush
# -m[arktime]: time stamp
# -k[ind]: be kind and submit slowly
# -b[buildandtest]: set up a personal condor and generic configs
# -a[again]: how many times do we run each test?
# -p[pretest]: get are environment set but run no tests
# -c[cleanup]: stop condor when test(s) finish.  Not used on windows atm.
#
while( $_ = shift( @ARGV ) ) {
  SWITCH: {
        if( /-d.*/ ) {
                push(@compilers, shift(@ARGV));
                next SWITCH;
        }
        if( /-f.*/ ) {
                $testfile = shift(@ARGV);
                next SWITCH;
        }
        if( /-s.*/ ) {
                $skipfile = shift(@ARGV);
                next SWITCH;
        }
        if( /-r.*/ ) { #retirement timeout
                $test_retirement = shift(@ARGV);
                next SWITCH;
        }
        if( /-k.*/ ) {
                $kindwait = 1;
                next SWITCH;
        }
        if( /-a.*/ ) {
                $repeat = shift(@ARGV);
                next SWITCH;
        }
        if( /-b.*/ ) {
				# start with fresh environment
                $wantcurrentdaemons = 0;
                next SWITCH;
        }
        if( /-p.*/ ) {
				# start with fresh environment
                $wantcurrentdaemons = 0;
				$pretestsetuponly = 1;
                next SWITCH;
        }
        if( /-t.*/ ) {
                push(@testlist, shift(@ARGV));
                next SWITCH;
        }
        if( /-xml.*/ ) {
                $isXML = 1;
                print "xml output format selected\n";
                next SWITCH;
        }
        if( /-q.*/ ) {
                $hush = 1;
                next SWITCH;
        }
        if( /-m.*/ ) {
                $timestamp = 1;
                next SWITCH;
        }
        if( /-c.*/ ) {
                # This is not used on windows systems at the moment.
                $cleanupcondor = 1;
                push (@extracondorargs, "-pidfile $condorpidfile");
                next SWITCH;
        }
  }
}


%test_suite = ();

# take a momment to get a personal condor running if it is not configured
# and running already

DebugOff();

my $iswindows =  0;
my $awkscript = "";
my $genericconfig = "";
my $genericlocalconfig = "";
my $nightly = IsThisNightly($BaseDir);
my $res = 0;

if(!($wantcurrentdaemons)) {

	$iswindows = IsThisWindows();
	$awkscript = "../condor_examples/convert_config_to_win32.awk";
	$genericconfig = "../condor_examples/condor_config.generic";
	$genericlocalconfig = "../condor_examples/condor_config.local.central.manager";


	if($nightly == 1) {
		print "This is a nightly test run\n";
	}

	$res = IsPersonalTestDirThere();
	if($res != 0) {
		die "Could not establish directory for personal condor\n";
	}

	WhereIsInstallDir();

	$res = IsPersonalTestDirSetup();
	if($res == 0) {
		print "Need to set up config files for test condor\n";
		CreateConfig();
		CreateLocalConfig();
		CreateLocal();
	}

	if($iswindows == 1) {
		$tmp = `cygpath -w $targetconfig`;
		chomp($tmp);
		$ENV{CONDOR_CONFIG} = $tmp;
		$res = IsPersonalRunning($tmp);
	} else {
		$ENV{CONDOR_CONFIG} = $targetconfig;
		$res = IsPersonalRunning($targetconfig);
	}

	# capture pid for master
	chdir("$BaseDir");

	if($res == 0) {
		print "Starting Personal Condor\n";
		if($iswindows == 1) {
			$mcmd = "$wininstalldir/bin/condor_master.exe -f &";
			$mcmd =~ s/\\/\//g;
			debug( "Starting master like this:\n");
			debug( "\"$mcmd\"\n");
			system("$mcmd");
		} else {
			system("$installdir/sbin/condor_master @extracondorargs -f &");
		}
		print "Done Starting Personal Condor\n";
	}
		
	IsRunningYet();

}

@myfig = `condor_config_val -config`;
debug("Current config settings are:\n");
foreach $fig (@myfig) {
	debug("$fig\n");
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
if( ! @compilers ) {
    # find all compiler subdirectories in the test directory
    opendir( TEST_DIR, $test_dir ) || die "error opening \"$test_dir\": $!\n";
    @compilers = grep -d, readdir( TEST_DIR );
    # filter out . and ..
    @compilers = grep !/^\.\.?$/, @compilers;
	# if the tests have run before we have pid labeled directories and 
	# test.saveme directories which should be ignored.
	@compilers = grep !/^.*saveme.*$/, @compilers;
	@compilers = grep !/^\d+$/, @compilers;
    # get rid of CVS entry for testing - won't hurt to leave it in.
    @compilers = grep !/CVS/, @compilers;
    closedir TEST_DIR;
    die "error: no compiler subdirectories found\n" unless @compilers;
}

if($timestamp == 1) {
	system("date");
}

foreach $name (@compilers) {
	if($hush == 0) { 
		print "Compiler:$name\n";
	}
}

# now we find the tests we care about.
if( @testlist ) {

	foreach $name (@testlist) {
		if($hush == 0) { 
			print "Testlist:$name\n";
		}
	}

    # we were explicitly given a # list on the command-line
    foreach $test (@testlist) {
		if( ! ($test =~ /(.*)\.run$/) ) {
	    	$test = "$test.run";
		}
		foreach $compiler (@compilers)
		{
	    	push(@{$test_suite{"$compiler"}}, $test);
		}
    }
} elsif( $testfile ) {
    # if we were given a file, let's read it in and use it.
    #print "found a runfile: $testfile\n";
    open(TESTFILE, $testfile) || die "Can't open $testfile\n";
    while( <TESTFILE> ) {
	CondorTest::fullchomp($_);
	$test = $_;
	if($test =~ /^#.*$/) {
		#print "skip comment\n";
		next;
	}
	#//($compiler, $test) = split('\/');
	if( ! ($test =~ /(.*)\.run$/) ) {
	    $test = "$test.run";
	}
	foreach $compiler (@compilers)
	{
	    push(@{$test_suite{"$compiler"}}, $test);
	}
    }
    close(TESTFILE);
} else {
    # we weren't given any specific tests or a test list, so we need to 
    # find all test programs (all files ending in .run) for each compiler
    foreach $compiler (@compilers) {
	opendir( COMPILER_DIR, $compiler )
	    || die "error opening \"$compiler\": $!\n";
	@{$test_suite{"$compiler"}} = grep /\.run$/, readdir( COMPILER_DIR );
	closedir COMPILER_DIR;
	#print "error: no test programs found for $compiler\n" 
	    #unless @{$test_suite{"$compiler"}};
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
		$tmp = $_;
		if( $tmp =~ /^#.*$/ ) {
			# comment so skip
			next;
		}
		$moretests = $tmp . ".run";
		push @toptests,$moretests;
	}
	close(QUICK);
	@{$test_suite{"."}} = @toptests;
	push @compilers, ".";
}


# if we were given a skip file, let's read it in and use it.
# remove any skipped tests from the test list  
if( $skipfile ) {
    print "found a skipfile: $skipfile \n";
    open(SKIPFILE, $skipfile) || die "Can't open $skipfile\n";
    while(<SKIPFILE>) {
	CondorTest::fullchomp($_);
	$test = $_;
	foreach $compiler (@compilers) {
	    # $skip_hash{"$compiler"}->{"$test"} = 1;
	    #@{$test_suite{"$compiler"}} = grep !/$test\.run/, @{$test_suite{"$compiler"}};
	    @{$test_suite{"$compiler"}} = grep !/$test/, @{$test_suite{"$compiler"}};
	} 
    }
    close(SKIPFILE);
}

# set up base directory for storing test results
if ($isXML){
      system ("mkdir -p $BaseDir/results");
      $ResultDir = "$BaseDir/results";
      open( XML, ">$ResultDir/ncondor_testsuite.xml" ) || die "error opening \"ncondor_testsuite.xml\": $!\n";
      print XML "<\?xml version=\"1.0\" \?>\n<test_suite>\n";
}

# Now we'll run each test.
foreach $compiler (@compilers)
{
    if ($isXML){
      system ("mkdir -p $ResultDir/$compiler");
    } 
	if($compiler ne "\.") {
    	chdir $compiler || die "error switching to directory $compiler: $!\n";
	}
	my $compilerdir = getcwd();
	# add in compiler dir to the current path
	$ENV{PATH} = $ENV{PATH} . ":" . $compilerdir;

    # fork a child to run each test program
	if($hush == 0) { 
    	print "submitting $compiler tests\n";
	}
    foreach $test_program (@{$test_suite{"$compiler"}})
    {
		if(($hush == 0) && (! defined $kindwait)) { 
        	print ".";
		}

        next if $skip_hash{$compiler}->{$test_program};

		# allow multiple runs easily
		my $repeatcounter = 0;
		debug("Want $repeat runs of each test\n");
		while($repeatcounter < $repeat) {

	        $pid = fork();
			debug( "forking for $test_program pid returned is $pid\n");
	        die "error calling fork(): $!\n" unless defined $pid;

			# two modes 
			#		kindwait = resolve each test after the fork
			#		else:	   fork them all and then wait for all

			if( defined $kindwait ) {
				if( $pid > 0 ) {
	            	$test{$pid} = "$test_program";
					debug( "Started test: $test_program/$pid\n");

					# Wait for job before starting the next one

					StartTestOutput($compiler,$test_program);

					#print "Waiting on test\n";
	    			while( $child = wait() ) {

	        			# if there are no more children, we're done
	        			last if $child == -1;
						debug( "informed $child gone yeilding test $test{$child}\n");
		
						# ignore spurious children
						if(! defined $test{$child}) {
							debug("Can't find jobname for child? <ignore>\n");
							next;
						}

						#finally
	        			($test_name) = $test{$child} =~ /(.*)\.run$/;
						debug( "Done Waiting on test($test_name)\n");

	        			# record the child's return status
	        			$status = $?;

						CompleteTestOutput($compiler,$test_program,$child,$status);
					}
				} else {
	        		# if we're the child, start test program
					DoChild($test_program, $test_retirement);
				}
			} else {
				if( $pid > 0 ) {
	            	$test{$pid} = "$test_program";
					debug( "Started test: $test_program/$pid\n");
	            	sleep 1;
					next;
				} else { # child
	        		# if we're the child, start test program
					DoChild($test_program, $test_retirement);
				}
			}

			$repeatcounter = $repeatcounter + 1;
		}
    }

    # wait for each test to finish and print outcome
	if($hush == 0) { 
    	print "\n";
	}

	# complete the tests when batching them up

	if(! (defined $kindwait)) {
    	while( $child = wait() ) {
        	# if there are no more children, we're done
        	last if $child == -1;

        	# record the child's return status
        	$status = $?;

			debug( "informed $child gone yeilding test $test{$child}\n");
	
			if(! defined $test{$child}) {
				debug("Can't find jobname for child?<ignore>\n");
				next;
			}

        	($test_name) = $test{$child} =~ /(.*)\.run$/;

			StartTestOutput($compiler,$test_name);

			CompleteTestOutput($compiler,$test_name,$child,$status);
    	} # end while
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

open( SUMOUTF, ">>successful_tests_summary" )
    || die "error opening \"successful_tests_summary\": $!\n";
open( OUTF, ">successful_tests" )
    || die "error opening \"successful_tests\": $!\n";
for $test_name (@successful_tests)
{
    print OUTF "$test_name 0\n";
    print SUMOUTF "$test_name 0\n";
}
close OUTF;
close SUMOUTF;

open( SUMOUTF, ">>failed_tests_summary" )
    || die "error opening \"failed_tests_summary\": $!\n";
open( OUTF, ">failed_tests" )
    || die "error opening \"failed_tests\": $!\n";
for $test_name (@failed_tests)
{
    print OUTF "$test_name 1\n";
    print SUMOUTF "$test_name 1\n";
}
close OUTF;
close SUMOUTF;

if ( $cleanupcondor )
{
   if ( $iswindows ) 
   {
      # Currently not implemented.
   }
   else
   {
      $pid=`cat $condorpidfile`;
      system("kill $pid");
      system("rm -f $condorpidfile");
   }
}
exit $num_failed;

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

sub IsThisNightly
{
	$mylocation = shift;

	print "IsThisNightly passed <$mylocation>\n";
	if($mylocation =~ /^.*(\/execute\/).*$/) {
		print "Nightly testing\n";
		$configlocal = "../condor_examples/condor_config.local.central.manager";
		$configmain = "../condor_examples/condor_config.generic";
		if(!(-f $configmain)) {
			system("ls ..");
			system("ls ../condor_examples");
			die "No base config file!!!!!\n";
		}
		return(1);
	} else {
		print "Workspace testing\n";
		$configlocal = "../condor_examples/condor_config.local.central.manager";
		$configmain = "../condor_examples/condor_config.generic";
		if(!(-f $configmain)) {
			system("ls ..");
			system("ls ../condor_examples");
			die "No base config file!!!!!\n";
		}
		return(0);
	}
}

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

sub IsPersonalTestDirThere
{
	if( -d $testpersonalcondorlocation ) {
		debug( "Test Personal Condor Directory Established prior\n");
		return(0)
	} else {
		debug( "Test Personal Condor Directory being Established now\n");
		system("mkdir -p $testpersonalcondorlocation");
		return(0)
	}
}

sub IsPersonalTestDirSetup
{
	$configfile = $testpersonalcondorlocation . "/condor_config";
	if(!(-f $configfile)) {
		return(0);
	}
	return(1);
}

sub WhereIsInstallDir
{
	$top = getcwd();
	debug( "getcwd says \"$top\"\n");
	$crunched = `cygpath -w $top`;
	chomp($crunched);
	debug( "cygpath changed it to: \"$crunched\"\n");
	$ppwwdd = `pwd`;
	debug( "pwd says: $ppwwdd\n");

	$tmp = CondorTest::Which("condor_master");
	chomp($tmp);
	debug( "Install Directory \"$tmp\"\n");
	if($iswindows == 0) {
		if($tmp =~ /^(.*)\/sbin\/condor_master\s*$/) {
			$installdir = $1;
			print "Testing This Install Directory: \"$installdir\"\n";
		}
	} else {
		if($tmp =~ /^(.*)\/bin\/condor_master\s*$/) {
			$installdir = $1;
			$tmp = `cygpath -w $1`;
    		CondorTest::fullchomp($tmp);
			$wininstalldir = $tmp;
			$wininstalldir =~ s/\\/\//;
			print "Testing This Install Directory: \"$wininstalldir\"\n";
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

sub CreateConfig
{
	# The only change we need to make to the generic configuration
	# file is to set the release-dir and local-dir. (non-windows)
	# change RELEASE_DIR and LOCAL_DIR    
	$currenthost = CondorTest::getFqdnHost();
	chomp($currenthost);

	debug( "Set RELEASE_DIR and LOCAL_DIR\n");    

	# Windows needs config file preparation, wrapper scripts etc
	if($iswindows == 1) {
		# set up job wrapper
		safe_copy("../condor_scripts/exe_switch.pl", "$wininstalldir/bin/exe_switch.pl") ||
        die "couldn't copy exe_swtich.pl";
		open( WRAPPER, ">$wininstalldir/bin/exe_switch.bat" ) || die "Can't open new job wrapper: $!\n";
    	print WRAPPER "\@c:\\perl\\bin\\perl.exe $wininstalldir/bin/exe_switch.pl %*\n";
    	close(WRAPPER);
		
		# pre-process config file src and windowize it

		# create config file with todd's awk script
		$configcmd = "gawk -f $awkscript $genericconfig";
		debug("awk cmd is $configcmd\n");

		open( OLDFIG, " $configcmd 2>&1 |")
			|| die "Can't run script file\"$configcmd\": $!\n";    

	} else {
		open( OLDFIG, "<$configmain" ) 
			|| die "Can't open base config file: $!\n";    
	}

	open( NEWFIG, ">$targetconfig" ) 
		|| die "Can't open new config file: $!\n";    
	$line = "";    
	while( <OLDFIG> ) {        
		chomp;        
		$line = $_;        
		if($line =~ /^RELEASE_DIR\s*=.*/) {            
			debug( "Matching <<$line>>\n");            
			if($iswindows == 1) {
				print NEWFIG "RELEASE_DIR = $wininstalldir\n";        
			} else {
				print NEWFIG "RELEASE_DIR = $installdir\n";        
			}
		} elsif($line =~ /^LOCAL_DIR\s*=.*/) {            
			debug( "Matching <<$line>>\n");            
			if($iswindows == 1) {
				$newloc = $wintestpersonalcondorlocation . "/local";
				print NEWFIG "LOCAL_DIR = $newloc\n";
			} else {
				print NEWFIG "LOCAL_DIR = $localdir\n";        
			}
		} elsif($line =~ /^LOCAL_CONFIG_FILE\s*=.*/) {            
			debug( "Matching <<$line>>\n");            
			if($iswindows == 1) {
				$newloc = $wintestpersonalcondorlocation . "/condor_config.local";
				print NEWFIG "LOCAL_CONFIG_FILE = $newloc\n";
			} else {
				print NEWFIG "LOCAL_CONFIG_FILE = $testpersonalcondorlocation/condor_config.local\n";        
			}
		} elsif($line =~ /^CONDOR_HOST\s*=.*/) {            
			debug( "Matching <<$line>>\n");            
			print NEWFIG "CONDOR_HOST = $currenthost\n";
		} elsif($line =~ /^HOSTALLOW_WRITE\s*=.*/) {            
			debug( "Matching <<$line>>\n");            
			print NEWFIG "HOSTALLOW_WRITE = *\n";
		} else {            
			print NEWFIG "$line\n";        
		}    
	}    
	close( OLDFIG );    
	close( NEWFIG );
}

sub CreateLocalConfig
{
	debug( "Modifying local config file\n");

	# make sure ports for Personal Condor are valid, we'll use address
	# files and port = 0 for dynamic ports...
	if($iswindows == 1) {
		# create config file with todd's awk script
		$configcmd = "gawk -f $awkscript $genericlocalconfig";
		debug("gawk cmd is $configcmd\n");

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
	print FIX "SCHEDD_ADDRESS_FILE = \$(LOG)/.scheduler_address\n";

	# ADD size for log files and debug level
	# default settings are in condor_config, set here to override 
	print FIX "ALL_DEBUG               = D_FULLDEBUG D_NETWORK\n";

	print FIX "MAX_COLLECTOR_LOG       = $logsize\n";
	print FIX "COLLECTOR_DEBUG         = \n";

	print FIX "MAX_KBDD_LOG            = $logsize\n";
	print FIX "KBDD_DEBUG              = \n";

	print FIX "MAX_NEGOTIATOR_LOG      = $logsize\n";
	print FIX "NEGOTIATOR_DEBUG        = D_MATCH\n";
	print FIX "MAX_NEGOTIATOR_MATCH_LOG = $logsize\n";

	print FIX "MAX_SCHEDD_LOG          = 2500000\n";
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
	print FIX "PERIODIC_EXPR_TIMESLICE = .95\n";
	print FIX "NEGOTIATOR_INTERVAL = 20\n";

	# turn on soap for testing
	print FIX "ENABLE_SOAP            	= TRUE\n";
	print FIX "ALLOW_SOAP            	= *\n";
	print FIX "QUEUE_ALL_USERS_TRUSTED 	= TRUE\n";

	# condor_config.generic now contains a special value
	# for HOSTALLOW_WRITE which causes it to EXCEPT on submit
	# till set to some legal value. Old was most insecure..
	print FIX "HOSTALLOW_WRITE 			= *\n";
	print FIX "NUM_CPUS 			= 2\n";

	# Allow a default heap size for java(addresses issues on x86_rhas_3)
	# May address some of the other machines with Java turned off also
	print FIX "JAVA_MAXHEAP_ARGUMENT = \n";


	# below stolen from condor_configure

	my $jvm = "";

    my @default_jvm_locations = ("/bin/java",
                 "/usr/bin/java",
                 "/usr/local/bin/java",
                 "/s/std/bin/java");

    unless (system ("which java >> /dev/null 2>&1")) {
    	chomp (my $which_java = CondorTest::Which("java"));
    	@default_jvm_locations = ($which_java, @default_jvm_locations) unless ($?);
    }

    my $java_libdir = "$release_dir/lib";
    my $exec_result;
    my $default_jvm_location;

    # check some default locations for java and pick first valid one
    foreach $default_jvm_location (@default_jvm_locations) {
    	if ( -f $default_jvm_location && -x $default_jvm_location) {
        	$jvm = $default_jvm_location;
        	last;
    	}
    }

    # if nothing is found, explain that, otherwise see if they just want to
    # accept what I found.

	debug ("Setting JAVA=$jvm");

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

	print FIX "JAVA = $jvm\n";


	# above stolen from condor_configure

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

	# Add a job wrapper for windows.... and a few other things which
	# normally are done by condor_configure for a personal condor
	if($iswindows == 1) {
		print FIX "USER_JOB_WRAPPER = $wininstalldir/bin/exe_switch.bat\n";
	}

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
	print FIX "ALL_DEBUG = D_FULLDEBUG\n";
	#insure path from framework is injected into the new pool
	print FIX "environment=\"PATH=\'$mypath\'\"\n";
	print FIX "SUBMIT_EXPRS=environment\n";
	print FIX "PROCD_LOG = \$(LOG)/ProcLog\n";
	if($iswindows == 1) {
		print FIX "PROCD_ADDRESS = \\\\.\\pipe\\buildandtestprocd\n";
	}

	close FIX; 
}

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

    open(CONFIG, "condor_config_val -config -master log 2>&1 |") || die "condor_config_val: $
!\n";
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

	if($daemonlist =~ /.*MASTER.*/) {
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
            	print "Found it!!!! master address file \n";
            	$havemasteraddr = "yes";
        	} else {
            	sleep 2;
        	}
    	}
	}

	if($daemonlist =~ /.*COLLECTOR.*/) {
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
            	print "Found it!!!! collector address file\n";
            	$havecollectoraddr = "yes";
        	} else {
            	sleep 2;
        	}
    	}
	}

	if($daemonlist =~ /.*NEGOTIATOR.*/) {
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
            	print "Found it!!!! negotiator address file\n";
            	$havenegotiatoraddr = "yes";
        	} else {
            	sleep 2;
        	}
    	}
	}

	if($daemonlist =~ /.*STARTD.*/) {
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
            	print "Found it!!!! startd address file\n";
            	$havestartdaddr = "yes";
        	} else {
            	sleep 2;
        	}
    	}
	}

	if($daemonlist =~ /.*SCHEDD.*/) {
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
            	print "Found it!!!! schedd address file\n";
            	$havescheddaddr = "yes";
        	} else {
            	sleep 2;
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

	if ($isXML){
		print XML "<test_result>\n<name>$compiler.$test_program</name>\n<description></description>\n";
		printf( "%-40s ", $test_program );
	} else {
		printf( "%-40s ", $test_program );
	}
}

# CompleteTestOutput($compiler,$test_program,$child,$status);
sub CompleteTestOutput
{
	my $compiler = shift;
	my $test_name = shift;
	my $child = shift;
	my $status = shift;

	if( WIFEXITED( $status ) && WEXITSTATUS( $status ) == 0 )
	{
		if ($isXML){
			print XML "<status>SUCCESS</status>\n";
			print "succeeded\n";
		} else {
			print "succeeded\n";
		}
		$num_success++;
		@successful_tests = (@successful_tests, "$compiler/$test_name");
	} else {
		$failure = `grep 'FAILURE' $test{$child}.out`;
		$failure =~ s/^.*FAILURE[: ]//;
		CondorTest::fullchomp($failure);
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

# DoChild($test_program, $test_retirement);
sub DoChild
{
	my $test_program = shift;
	my $test_retirement = shift;

	my $res;
 	eval {
            alarm($test_retirement);
			debug( "Child Starting:perl $test_program > $test_program.out\n");
			$res = system("perl $test_program > $test_program.out 2>&1");

			# if not build and test move key files to saveme/pid directory
			$_ = $test_program;
			s/\.run//;
			$testname = $_;
			$save = $testname . ".saveme";
			$piddir = $save . "/$$";
			# make sure pid storage directory exists
			$mksave = system("mkdir -p $save");
			$pidcmd = "mkdir -p " . $save . "/" . "$$";
			$mkpid = system("$pidcmd");

			# generate file names
			$log = $testname . ".log";
			$cmd = $testname . ".cmd";
			$out = $testname . ".out";
			$err = $testname . ".err";
			$runout = $testname . ".run.out";
			$cmdout = $testname . ".cmd.out";

			$newlog =  $piddir . "/" . $log;
			$newcmd =  $piddir . "/" . $cmd;
			$newout =  $piddir . "/" . $out;
			$newerr =  $piddir . "/" . $err;
			$newrunout =  $piddir . "/" . $runout;
			$newcmdout =  $piddir . "/" . $cmdout;

			if( $nightly == 0) {
				move($log, $newlog);
				copy($cmd, $newcmd);
				move($out, $newout);
				move($err, $newerr);
				copy($runout, $newrunout);
				move($cmdout, $newcmdout);
			} else {
				copy($log, $newlog);
				copy($cmd, $newcmd);
				copy($out, $newout);
				copy($err, $newerr);
				copy($runout, $newrunout);
				copy($cmdout, $newcmdout);
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
