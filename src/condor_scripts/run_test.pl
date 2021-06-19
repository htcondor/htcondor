#!/usr/bin/env perl

##**************************************************************
##
## Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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

# run_test.pl
# V1.0 2016-September-27 / Mihir Shete / smihir@cs.wisc.edu
#  - This file is based on batch_test.pl, but it just runs a single test
#    if the test to be run requires that HTCondor be already running it
#    is the responsibility of the caller of this script to start it.

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
use CheckOutputFormats;

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

Condor::DebugLevel(2);
CondorPersonal::DebugLevel(2);
my @debugcollection = ();

my $starttime = time();

my $time = strftime("%H:%M:%S", localtime);
print "run_test $$: $time Starting ($^O perl)\n";

my $iswindows = CondorUtils::is_windows();

# configuration options
my $test_retirement = 1800;	# seconds for an individual test timeout - 30 minutes
my $BaseDir = getcwd();
my $RunsDir = $BaseDir; $RunsDir =~ s/condor_tests$/test-runs/;
my $hush = 1;

# set up to recover from tests which hang
$SIG{ALRM} = sub { die "!run_test:test timed out!\n" };

# setup
STDOUT->autoflush();   # disable command buffering of stdout
STDERR->autoflush();   # disable command buffering of stderr
my $isXML = 0;  # are we running tests with XML output

if ($iswindows) {
    # yet add in base dir of all tests and compiler directories
    $ENV{PATH} = $ENV{PATH} . ";" . $BaseDir;
} else {
    # remove . from path
    CleanFromPath(".");
    # yet add in base dir of all tests and compiler directories
    $ENV{PATH} = $ENV{PATH} . ":" . $BaseDir;
}
# the args:
my @testlist;

while($_ = shift(@ARGV)) {
    SWITCH: {
        if(/^-h.*/) {
            Help();
            exit(0);
        }
        if(/--debug/) {
            next SWITCH;
        }
        if(/--no-debug/) {
            next SWITCH;
        }
        if(/^-xml.*/) {
            $isXML = 1;
            debug("xml output format selected\n",2);
            next SWITCH;
        }
        if(/^-q.*/) {
            $hush = 1;
            next SWITCH;
        }
        if(/^-v.*/) {
            Condor::DebugLevel(2);
            next SWITCH;
        }
        if(/^-.*/) {
            Help();
            exit(0);
        }

        # Default case
        push(@testlist, $_);
        next SWITCH;
    }
}

my @test_suite;

$time = strftime("%H:%M:%S", localtime);
print "run_test $$: Ready for Testing at $time\n";

# now we find the tests we care about.
if(@testlist) {
    debug("Test list contents:\n", 6);

    # we were explicitly given a # list on the command-line
    foreach my $test (@testlist) {
        debug("    $test\n", 6);
        if($test !~ /.*\.\w+$/) {  # i.e., no extension
            if (-s "$test.run") {
                $test = "$test.run";
            }
            elsif (-s "$test.py") {
                $test = "$test.py";
            }
        }

        push(@test_suite, $test);
    }
}

my $XmlResultDir;
# set up base directory for storing test results
if($isXML) {
    CondorTest::verbose_system ("mkdir -p $RunsDir/results",{emit_output=>0});
    $XmlResultDir = "$RunsDir/results";
    open( XML, ">$XmlResultDir/ncondor_testsuite.xml" ) || die "error opening \"ncondor_testsuite.xml\": $!\n";
    print XML "<\?xml version=\"1.0\" \?>\n<test_suite>\n";
}

# Now we'll run each test.
my $hashsize = 0;
my %test;
my $reaped = 0;
my $res = 0;

my $currenttest = 0;

foreach my $test_program (@test_suite) {
    debug(" *********** Starting test: $test_program *********** \n",6);

    # doing this next test
    $currenttest = $currenttest + 1;

    debug("Want to test $test_program\n",6);


    # run test directly. no more forking
    $res = DoChild($test_program, $test_retirement);
    #StartTestOutput($compiler, $test_program);
    StartTestOutput($test_program);
    #CompleteTestOutput($compiler, $test_program, $res);
    CompleteTestOutput($test_program, $res);
    $time = strftime("%H:%M:%S", localtime);
    if($res == 0) {
        print "run_test $$: $time $test_program passed\n";
    } else {
        print "run_test $$: $time $test_program FAILED\n";
    }

} # end of foreach $test_program

# wait for test to finish and print outcome
if($hush == 0) {
    print "\n";
}

if($isXML) {
    print XML "</test_suite>\n";
    close (XML);
}

$time = strftime("%H:%M:%S", localtime);
my $duration = "";
my $endtime = time();
my $deltatime = $endtime - $starttime;
my $hours = int($deltatime / (60*60));
my $minutes = int(($deltatime - $hours*60*60) / 60);
my $seconds = $deltatime - $hours*60*60 - $minutes*60;

$duration = sprintf("%d:%02d:%02d (%d seconds)", $hours, $minutes, $seconds, $deltatime);

my @returns = CondorUtils::ProcessReturn($res);

$res = $returns[0];
my $signal = $returns[1];

# note we exit with the results of last test tested. Only
# thing looking at the status of run_test is the batlab
# test glue which runs only one test each call to run_test
# and this has no impact on workstation tests.
print "run_test $$: $time exiting with status=$res signal=$signal after $duration\n";
alarm(0); # revoke overall timeout
exit $res;

sub Help
{
    print "the args:\n";
    print "--[no-]debug: enable/disable test debugging disabled\n";
    print "-v[erbose]: print debugging output\n";
    print "-xml: print output in xml format\n";
}

sub CleanFromPath
{
    my $pulldir = shift;
    my $path = $ENV{PATH};
    my $newpath = "";
    my @pathcomponents = split /:/, $path;
    foreach my $spath ( @pathcomponents) {
        if($spath ne "$pulldir") {
            $newpath = $newpath . ":" . $spath;
        }
    }
}

# TODO_TJ: this does nothing unless xml output...
# StartTestOutput($compiler,$test_program);
sub StartTestOutput
{
    my $test_program = shift;

    if($isXML) {
        print XML "<test_result>\n<name>$test_program</name>\n<description></description>\n";
        printf("%40s ", $test_program );
    }
}

# TODO_TJ: this does nothing unless xml output...
# CompleteTestOutput($compiler,$test_program,$status);
sub CompleteTestOutput
{
    my $test_name = shift;
    my $status = shift;
    my $failure = "";
    my @statret = ();

    debug(" *********** Completing test: $test_name *********** \n",6);
    @statret = CondorUtils::ProcessReturn($status);

    if($isXML) {
        print "Copying to $XmlResultDir ...\n";

        # possibly specify exact files in future - for now bring back all
        system ("cp $test_name.* $XmlResultDir/.");

        print XML "<data_file>$$test_name.run.out</data_file>\n<error>";
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
        $id = $test_id;
    }

    $_ = $test_program;
    s/\.\w+$//;
    my $testname = $_;

    my $needs = load_test_requirements($testname, $test_program);
    if(exists($needs->{personal})) {
        print "run_test $$: $testname requires a running HTCondor, checking...\n";
        print "\tCONDOR_CONFIG=$ENV{CONDOR_CONFIG}\n";
        my @whodata = `condor_who -quick 2>&1`;
        my $alive = "false";
        my $not_alive_reason = "Condor not running";
        if (exists($needs->{testconf})) {
            $not_alive_reason = "Test requires custom config";
        } else {
            foreach (@whodata) {
                next if ($_ =~ /^\s*$/);
                next if ($_ =~ /^Daemon|^----/);
                debug($_, 6);
                if ($_ =~ /^IsReady = (\S+)/) { $alive = $1; }
                }
            $alive = trim($alive);
        }
        if($alive eq "false") {
            print "\t$not_alive_reason, Starting new personal HTCondor now\n";
            print " @whodata\n";
            StartTestPersonal($testname, $needs->{testconf});
        }
    }

    my $perl = "perl";
    if (exists($needs->{python})) {
        print "run_test $$: $testname is python, checking python bindings\n";
        SetupPythonPath();
        print "\tPYTHONPATH=$ENV{PYTHONPATH}\n";
        $perl = "python3";

        if ($iswindows) {
            my $regkey = "HKLM\\Software\\Python\\PythonCore\\3.6\\InstallPath";

            # use 32 bit python if we have a 32 bit htcondor python module
            my $reldir = `condor_config_val release_dir`; chomp $reldir;
            my $relpy = "$reldir\\lib\\python";
            if( -f "$relpy\\htcondor\\htcondor.cp36-win32.pyd" ) {
                $regkey = "HKLM\\Software\\wow6432node\\Python\\PythonCore\\3.6\\InstallPath" 
            }

            my @reglines = `reg query $regkey /ve`;
            for my $line (@reglines) {
                if ($line =~ /REG_SZ\s+(.+)$/) {
                    print "\tfound python 3.6 install dir : '$1'\n";
                   	# Since we're using the full path, 'python.exe' will
                   	# always by the right Python version, and some of the
                   	# installs don't have a 'python3.exe'.
                    $perl = "$1python.exe";
                    last;
                }
            }
        }

        print "\tPython version: ";
        system ("$perl --version");
        print "\tPython exe: ";
        system ("$perl -c \"import sys; print(sys.executable)\"");
        print "\tPython-bindings version: ";
        system ("$perl -c \"import htcondor; print(htcondor.version())\"");
    }

    if (exists($needs->{pytest})) {
        print "run_test $$: $testname is pytest, checking pytest version\n";
        if($iswindows){
            system( "$perl -m pytest -s --version" );
        } else {
            system( "$perl -m pytest --version 2>&1" );
        }
        # This only works when a test file or directory is appended, because
        # --base-test-dir is an option our conftest.py adds.
        $perl = "$perl -m pytest -s --base-test-dir ${RunsDir}/";
    }

    my $test_starttime = time();
    debug("Test start @ $test_starttime \n",6);

    # with wrapping all test(most) in a personal condor
    # we know where the published directories are if we ask by name
    # and they are relevant for the entire test time. We need ask
    # and check only once.
    # add test core file

    # make sure pid storage directory exists
    my $save = $RunsDir . "/" . $testname . ".saveme";
    CreateDir("-p $save");
    my $piddir = $save;
    #$piddir .= "/pdir$$";
    #CreateDir("-p $piddir");

    my $log = "";
    my $cmd = "";
    my $out = "";
    my $err = "";
    my $runout = "";
    my $cmdout = "";

    alarm($test_retirement);
    if(defined $test_id) {
        $log = $testname . ".$test_id" . ".log";
        $cmd = $testname . ".$test_id" . ".cmd";
        $out = $testname . ".$test_id" . ".out";
        $err = $testname . ".$test_id" . ".err";
        $runout = $testname . ".$test_id" . ".run.out";
        $cmdout = $testname . ".$test_id" . ".cmd.out";
    } else {
        $log = $testname . ".log";
        $cmd = $testname . ".cmd";
        $out = $testname . ".out";
        $err = $testname . ".err";
        $runout = $testname . ".run.out";
        $cmdout = $testname . ".cmd.out";
    }

    my $res;
    my $use_timed_cmd = 1; # use the timed_cmd helper binary to timeout the test and cleaup processes
    if ($iswindows && $use_timed_cmd) {
        my $dtm = ""; if (defined $ENV{TIMED_CMD_DEBUG_WAIT}) {$dtm = ":$ENV{TIMED_CMD_DEBUG_WAIT}";}
        my $verb = ($hush == 0) ? "" : "-v";
        my $timeout = "-t 12M";
        $res = system("timed_cmd.exe -jgd$dtm $verb -o $runout $timeout $perl $test_program");
    } else {
        if( $hush == 0 ) { debug( "Child Starting: $perl $test_program > $runout\n",6); }
        $res = system("$perl $test_program > $runout 2>&1");
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

    if(exists($needs->{personal})) {
        my $personalstatus = StopTestPersonal($testname);
        if($personalstatus != 0 && $res == 0) {
            print "\tTest succeeded, but condor failed to shut down or there were\n";
            print "\tcore files or error messages in the logs. see CondorTest::EndTest\n";
            $res = $personalstatus;
        }
    }
    return($res);
}

# Call down to Condor Perl Module for now
sub debug {
    my ($msg, $level) = @_;
    my $time = Condor::timestamp(); #chomp($time);
    push @debugcollection, "$time BT$level $msg";
    Condor::debug("BT$level $msg", $level);
}

sub load_test_requirements
{
    my $name = shift;
    my $program = shift;
    my $requirements;

    if (open(TF, "<${program}")) {
        my $record = 0;
        my $triplequote = 0;
        my $conf = "";
        while (my $line = <TF>) {
            CondorUtils::fullchomp($line);
            if($line =~ /^#testreq:\s*(.*)/) {
                my @requirementList = split(/ /, $1);
                foreach my $requirement (@requirementList) {
                    $requirement =~ s/^\s+//;
                    $requirement =~ s/\s+$//;
                    $requirements->{ $requirement } = 1;
                }
            }

            # look at the shebang line to decide what executable to run with
            if($line =~ /^\#\!/) {
                if ($line =~ /python/) { $requirements->{python} = 1; }
                elsif ($line =~ /pytest/) {
                    $requirements->{python} = 1;
                    $requirements->{pytest} = 1;
                }
                next;
            }

            if($line =~ /<<['"]?CONDOR_TESTREQ_CONFIG/) {
                $record = 1;
                if ($line =~ /"""/) { $triplequote = 1; }
                next;
            }

            if($line =~ /^#endtestreq/) {
                if ($record) {
                    $requirements->{testconf} = $conf . "\n";
                    $requirements->{testconf} .= "TEST_DIR = ${BaseDir}\n";
                }
                $record = 0;
                last;
            }

            if($record && $line =~ /CONDOR_TESTREQ_CONFIG/) {
                $requirements->{testconf} = $conf . "\n";
                $requirements->{testconf} .= "TEST_DIR = ${BaseDir}\n";
                $record = 0;
            }

            if($record && $triplequote && $line =~ /^"""$/) {
                $requirements->{testconf} = $conf . "\n";
                $requirements->{testconf} .= "TEST_DIR = ${BaseDir}\n";
                $record = 0;
            }

            if ($record) {
                $conf .= $line . "\n";
            }
        }
    }

    # If the test file does not contain the requirements for it to
    # run then try to read requirements from the "Test_Requirements"
    # file. This file will be depraceted so new test file should
    # mention the requirements in itself rather than adding an
    # entry in "Test_Requirements"
    if (!defined($requirements)) {
        my $requirementslist = "Test_Requirements";
        open(TR, "< ${requirementslist}") or return $requirements;
        while (my $line = <TR>) {
            CondorUtils::fullchomp( $line );
            if ($line =~ /\s*$name\s*:\s*(.*)/) {
                my @requirementList = split(/,/, $1);
                foreach my $requirement (@requirementList) {
                    $requirement =~ s/^\s+//;
                    $requirement =~ s/\s+$//;
                    $requirements->{$requirement} = 1;
                }
            }
        }
    }

    return $requirements;
}

sub StartTestPersonal {
    my $test = shift;
    my $testconf = shift;
    my $firstappend_condor_config;

    if (not defined $testconf) {
        $firstappend_condor_config = '
            DAEMON_LIST = MASTER, SCHEDD, COLLECTOR, NEGOTIATOR, STARTD
            NEGOTIATOR_INTERVAL = 5
            JOB_MAX_VACATE_TIME = 15
        ';
    } else {
        $firstappend_condor_config = $testconf;
    }

    my $configfile = CondorTest::CreateLocalConfig($firstappend_condor_config,"remote_task.$test");

  eval {
    CondorTest::StartCondorWithParams(
        condor_name => "condor",
        fresh_local => "TRUE",
        condorlocalsrc => "$configfile",
        test_name => "$test",
        runs_dir => "$RunsDir"
    ); 1;
  } or do {
    my $msg = $@ || "Condor did not start";
    say STDERR "$msg";
    exit(136); # metronome wants 136 for setup error
  }
}

sub StopTestPersonal {
    my $exit_status = 0;
    $exit_status = CondorTest::EndTest("no_exit");
    return($exit_status);
}

sub SetupPythonPath {
    my $reldir = `condor_config_val release_dir`; chomp $reldir;
    my $pathsep = ':';
    my $relpy = "$reldir/lib/python3";
    if ($iswindows) { $relpy = "$reldir\\lib\\python"; $pathsep = ';'; }

    # debug code, show what is in release dir and lib and lib/python
    # on windows, also interrogate the bitness of the python bindings
    if ($iswindows) {
        system("dir $reldir");
        system("dir $reldir\\lib");
        print "contents of $relpy:\n";
        system("dir $relpy");
        $relpy .= ";$reldir\\bin";
    } else {
        system("ls -lL $reldir");
        system("ls -lL $reldir/lib");
        print "contents of $relpy:\n";
        system("ls -lL $relpy");
    }

    my $pythonpath = "";
    if (exists($ENV{PYTHONPATH})) {
        $pythonpath = $ENV{PYTHONPATH};
        print "\texisting PYTHONPATH=$pythonpath\n";
        { #if (index($pythonpath,$reldir) != -1) {
            print "\tadding $relpy to PYTHONPATH\n";
            $ENV{PYTHONPATH} = "$relpy$pathsep$pythonpath";
        }
    } else {
        print "\tsetting PYTHONPATH to $relpy\n";
        $ENV{PYTHONPATH} = $relpy;
    }
}

1;
