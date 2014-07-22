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
# April 14 : moved all condor startups, turnoffs out forever. However the initial
# config is now done once by remote_pre. The tests will all start their own 
# condor. Massive code deletion now that glue is doing the config and the tests
# their own personal condor startups from list personal_list. (bt)
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
#
# CondorPersonal.pm has a similar but separate mechanism.
#
#################################################################

Condor::DebugLevel(1);
CondorPersonal::DebugLevel(1);
my @debugcollection = ();
my $UseNewRunning = 1;

my $starttime = time();

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
my $cleanupcondor = 0;
my $want_core_dumps = 1;
my $condorpidfile = "/tmp/condor.pid.$$";
my @extracondorargs;

my $testdir;

my $wantcurrentdaemons = 1; # dont set up a new testing pool in condor_tests/TestingPersonalCondor
my $pretestsetuponly = 0; # only get the personal condor in place

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
# add 64 bit  location for java
if($iswindows == 1) {
	$ENV{PATH} = $ENV{PATH} . ":/cygdrive/c/windows/sysnative:c:\\windows\\sysnative";
}
#
# the args:

my $testfile = "";
my $ignorefile = "";
my @testlist;

# -c[md] create a command prompt with the testing environment (windows only)\n
# -c[cleanup]: stop condor when test(s) finish.  Not used on windows atm.
# -d[irectory] <dir>: just test this directory
# -f[ile] <filename>: use this file as the list of tests to run
# -i[gnore] <filename>: use this file as the list of tests to skip
# -s[ort] sort tests before testing
# -t[estname] <test-name>: just run this test
# -q[uiet]: hush
# -m[arktime]: time stamp
# -k[ind]: be kind and submit slowly
# -a[again]: how many times do we run each test?
# -p[pretest]: get are environment set but run no tests
#
while( $_ = shift( @ARGV ) ) {
	SWITCH: {
		if( /-h.*/ ) {
			print "the args:\n";
			print "-c[md] create a command prompt with the testing environment (windows only)\n";
			print "-d[irectory] dir: just test this directory\n";
			print "-f[ile] filename: use this file as the list of tests to run\n";
			print "-i[gnore] filename: use this file as the list of tests to skip\n";
			print "-t[estname] test-name: just run this test\n";
			print "-q[uiet]: hush\n";
			print "-m[arktime]: time stamp\n";
			print "-k[ind]: be kind and submit slowly\n";
			print "-e[venly]: group size: run a group of tests\n";
			print "-s[ort] sort before running tests\n";
			print "-xml: Output in xml\n";
			print "-p[pretest]: get are environment set but run no tests\n";
			print "--[no-]core: enable/disable core dumping enabled\n";
			print "--[no-]debug: enable/disable test debugging disabled\n";
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
		if( /^-c.*/ ) {
			# backward compatibility.
			if($iswindows) {
				$cmd_prompt = 1;
			}

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
		if( /^-p.*/ ) { # start with fresh environment
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
		if( /^-v.*/ ) {
			Condor::DebugLevel(2);
			CondorPersonal::DebugOn();
			CondorPersonal::DebugLevel(2);
			next SWITCH;
		}
	}
}

my $ripout = 1;

my %test_suite = ();

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
my $res = 0;

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
		debug(" *********** Starting test: $test_program *********** \n",2);

		# doing this next test
		$currenttest = $currenttest + 1;

		debug("Want to test $test_program\n",2);


		$currentgroup += 1;
		# run test directly. no more forking
		$res = DoChild($test_program, $test_retirement);
		StartTestOutput($compiler, $test_program); 
		CompleteTestOutput($compiler, $test_program, $res);
		if($res == 0) {
			print "batch_test says $test_program succeeded\n";
		} else {
			print "batch_test says $test_program failed\n";
		}

	} # end of foreach $test_program

	# wait for each test to finish and print outcome
	if($hush == 0) { 
		print "\n";
	}

	# complete the tests when batching them up if some are left
	#$hashsize = keys %test;
	#debug("At end of compiler dir hash size:$hashsize\n",2);
	#if(($kindwait == 0) && ($hashsize > 0)) {
		#debug("At end of compiler dir about to wait\n",2);
		#$reaped = wait_for_test_children(\%test, $compiler);
		#$currentgroup -= $reaped;
	#}

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



{
	my $endtime = time();
	my $deltatime = $endtime - $starttime;
	my $hours = int($deltatime / (60*60));
	my $minutes = int(($deltatime - $hours*60*60) / 60);
	my $seconds = $deltatime - $hours*60*60 - $minutes*60;

	printf("Tests took %d:%02d:%02d (%d seconds)\n", $hours, $minutes, $seconds, $deltatime);
}

my @returns = CondorUtils::ProcessReturn($res);

$res = $returns[0];
my $signal = $returns[1];

# note we exit with the results of last test tested. Only
# thing looking at the status of batch_test is the batlag
# test glue which runs only one test each call to batch_test
# and this has no impact on workstation tests.
print "Batch_test: about to exit($res/$signal)\n";
exit $res;

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

# StartTestOutput($compiler,$test_program);
sub StartTestOutput
{
	my $compiler = shift;
	my $test_program = shift;

	debug("StartTestOutput passed compiler: $compiler\n",2);

	if ($isXML){
		print XML "<test_result>\n<name>$compiler.$test_program</name>\n<description></description>\n";
		printf( "%-40s ", $test_program );
	} else {
		#printf( "%-6s %-40s ", $compiler, $test_program );
	}
}

# CompleteTestOutput($compiler,$test_program,$status);
sub CompleteTestOutput
{
	my $compiler = shift;
	my $test_name = shift;
	my $status = shift;
	my $failure = "";

	debug(" *********** Completing test: $test_name *********** \n",2);
	if( WIFEXITED( $status ) && WEXITSTATUS( $status ) == 0 )
	{
		if($groupsize == 0) {
			print "$test_name: succeeded\n";
		} else {
			#print "Not Xml: group size <$groupsize> test <$test_name>\n";
			print "$test_name succeeded\n";
		}
		$num_success++;
		@successful_tests = (@successful_tests, "$compiler/$test_name");
	} else {
		#my $testname = "$test{$child}";
		#$testname = $testname . ".out";
		#$failure = `grep 'FAILURE' $testname`;
		#$failure =~ s/^.*FAILURE[: ]//;
		#CondorUtils::fullchomp($failure);
		#$failure = "$test_name: failed" if $failure =~ /^\s*$/;
		
		print "$test_name ***** FAILED *****\n";
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
		print "Starting batch id: $test_id PID: $$\n";
		$id = $test_id;
		print "ID = $id\n";
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
	#print "Batch_test: checking on saveme: $save\n";
	CreateDir("-p $save");
	#verbose_system("mkdir -p $save",{emit_output=>0});
	my $pidcmd =  $save . "/" . "$$";
	#print "Batch_test: checking on saveme/pid: $pidcmd\n";
	CreateDir("-p $pidcmd");
	#verbose_system("$pidcmd",{emit_output=>0});

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
	alarm($test_retirement);
	if(defined $test_id) {
		$log = $testname . ".$test_id" . ".log";
		$cmd = $testname . ".$test_id" . ".cmd";
		$out = $testname . ".$test_id" . ".out";
		$err = $testname . ".$test_id" . ".err";
		$runout = $testname . ".$test_id" . ".run.out";
		$cmdout = $testname . ".$test_id" . ".cmd.out";

		if( $hush == 0 ) {
			debug( "Child Starting:perl $test_program > $test_program.$test_id.out\n",2);
		}
		$res = system("perl $test_program > $test_program.$test_id.out 2>&1");
	} else {
		$log = $testname . ".log";
		$cmd = $testname . ".cmd";
		$out = $testname . ".out";
		$err = $testname . ".err";
		$runout = $testname . ".run.out";
		$cmdout = $testname . ".cmd.out";

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

	return($res);
	#if($res != 0) { 
	    #print "Perl test($test_program) returned <<$res>>!!! \n"; 
	    #exit(1); 
	#}
	#exit(0);
#
	#if($@) {
		#if($@ =~ /timeout/) {
			#print "\n$test_program            Timeout\n";
			#exit(1);
		#} else {
			#alarm(0);
			#exit(0);
		#}
	#}
	#exit(0);
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
			#print "processing PID $child: ";
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
			print "Name of the test not meeting test.run format:$hashnamefortest\n";
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
			print "Leaving wait_for_test_children max_to_read:$max_to_reap tests reaped:$tests_reaped\n";
			last;
		}
		#last if defined $max_to_reap 
			#and $max_to_reap != 0
			#and $max_to_reap <= $tests_reaped;
	}

	#print "Tests Reaped = <$tests_reaped>\n";
	return $tests_reaped;
}

sub showEnv {
	foreach my $env (sort keys %ENV) {
		print "$env: $ENV{$env}\n";
	}
}
1;
