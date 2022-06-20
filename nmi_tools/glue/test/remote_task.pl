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


######################################################################
# run a test in the Condor testsuite
# return val is the status of the test
# 0 = built and passed
# 1 = build failed
# 2 = run failed
# 3 = internal fatal error (a.k.a. die)
######################################################################

######################################################################
###### WARNING!!!  The return value of this script has special  ######
###### meaning, so you can NOT just call die().  you MUST       ######
###### use the special c_die() method so we return 3!!!!        ######
######################################################################

#use lib "$ENV{BASE_DIR}/condor_tests";

use strict;
use warnings;
use File::Copy;
use File::Basename;

# set to 1 if this script should start a personal condor when Test_Requirements indicates one is needed
# set to 0 to leave that to run_test.pl to handle
my $start_personal_if = 0;

BEGIN {
	my $dir = dirname($0);
	push @INC, $dir;
}

#foreach (@INC) { print "INC: $_\n"; }

use TestGlue;

my $iswindows = TestGlue::is_windows();
my $iscygwin  = TestGlue::is_cygwin_perl();

#if($iswindows == 1) {
#	if($iscygwin == 1) {
#		out("Entering remote_task using cygwin perl");
#	} else {
#		out("Entering remote_task using active state perl");
#	}
#}

TestGlue::setup_test_environment();

push @INC, "$ENV{BASE_DIR}/condor_tests";

require CondorTest;
require CondorPersonal;
require CondorUtils;
require CheckOutputFormats;

if( ! defined $ENV{_NMI_TASKNAME} ) {
    die "_NMI_TASKNAME not in environment, can't test anything!\n";
}
my $fulltestname = $ENV{_NMI_TASKNAME};

if( $fulltestname =~ /remote_task/) {
    die "_NMI_TASKNAME set to remote_task meaning 0 tests seen!\n";
}

if( ! $fulltestname ) {
    # if we have no task, just return success immediately
    out("No tasks specified, returning SUCCESS");
    exit 0;
}


my $BaseDir  = $ENV{BASE_DIR} || c_die("BASE_DIR is not in environment!\n");
my $test_dir = File::Spec->catdir($BaseDir, "condor_tests");

# iterations have numbers placed at the end of the name
# for unique db tracking in nmi for now.
if($fulltestname =~ /([\w\-\.\+]+)-\d+/) {
    my $matchingtest = $fulltestname . ".run";
    if(!(-f $matchingtest)) {
        # if we don't have a test called this, strip iterator off
        $fulltestname = $1;
    }
}


######################################################################
# get the testname and group
######################################################################

my @testinfo = split(/\./, $fulltestname);
my $testname = $testinfo[0];
my $compiler = $testinfo[1];

if( ! $testname ) {
    c_die("Invalid input for testname\n");
}

out("REMOTE_TASK.$$ Testname: '$testname'");
if( not TestGlue::is_windows() ) {
    if( $compiler ) {
        out("compiler is $compiler");
    }
    else {
        $compiler = ".";
    }
}
else {
    $compiler = ".";
}

######################################################################
# run the test using run_test.pl
######################################################################

if (1) {
    my @knobs;
    foreach my $key (keys %ENV) {
        if ( $key =~ m/^_CONDOR_/i ) {

			# keep the _CONDOR_ANCESTOR_* variables, for our parent
			# condor to find us and in the darkness bind us.
			next if ($key =~ m/_CONDOR_ANCESTOR_/i);
            push @knobs, $key;
            delete($ENV{$key});
        }
    }
    if (scalar @knobs > 0) {
        out("Removing environment config overides : " . join(' ',@knobs));
    }
}

# Some of the tests fail because the schedd fsync can take 30 seconds
# or more on busy nmi filesystems, which causes the tools to timeout.
# Up the tool timeout multipler to try to deal with this.
out("Setting _condor_TOOL_TIMEOUT_MULTIPLIER=4");
$ENV{_condor_TOOL_TIMEOUT_MULTIPLIER}="4";

chdir($test_dir) || c_die("Can't chdir($test_dir): $!\n");

############################################################
##
## Here we examine some requirements which we know about
## now and in the future we already have a list for personals
## but we could expand those into our new more general way.
##
############################################################

my $requirements = ProcessTestRequirements();

if( exists( $requirements->{ IPv4 } ) ) {
	my $ENABLE_IPV4 = `condor_config_val ENABLE_IPv4`;
	if( $ENABLE_IPV4 =~ m/FALSE/i || $ENABLE_IPV4 =~ m/0/i ) {
	    out( "'${testname}' requires IPv4, but IPv4 is not enabled.  Returning SUCCESS." );
    	exit( 0 );
	}
}

if ($start_personal_if) {
	if( exists( $requirements->{ personal } ) ) {
		out("'$testname' requires a personal HTCondor, starting one now.");
		StartTestPersonal($testname, $requirements->{testconf});
	}
}

# Now that we stash all debug messages, we'll only dump them all
# when the test fails. This will be debugs at all levels.

# must use cygwin perl on Windows for batch_test.pl
my $perl_exe = "perl";
if ($ENV{NMI_PLATFORM} =~ /_win/i) {
    my $perlver = `$perl_exe -version`;
    if ($perlver =~ /This is (perl .*)$/m) { $perlver = $1; }
    out("First perl is $perlver.");
    if ( !(exists $requirements->{cygwin})) {
        out("attempting to run tests using current perl");
		if ($ENV{PATH} =~ /[a-z]:\\condor\\bin;/i) {
            out("removing base condor from the path");
            my $tmppath = $ENV{PATH};
            $tmppath =~ s/[a-z]:\\condor\\bin;//i;
            if ($^O =~ /MSWin32/) {
	       		if ($tmppath =~ /[a-z]:\\cygwin\\bin;/i) {
                    out("removing cygwin from the path");
					#out("Before removal:$tmppath\n");
                    $tmppath =~ s/[a-z]:\\cygwin\\bin;//ig;
				}
	    	}
            #out("new PATH=$tmppath");
            $ENV{PATH} = $tmppath;
        }
    } elsif (!($perlver =~ /cygwin/i)) {
        out("Adding cygwin to the front of the path to force the use of cygwin perl.");
        $ENV{PATH} = "c:\\cygwin\\bin;$ENV{PATH}";
		out("new PATH=$ENV{PATH}");

        $perlver = `$perl_exe -version`;
		if ($perlver =~ /This is (perl .*)$/m) { $perlver = $1; }
        out("First perl is now $perlver.");
        if (!($perlver =~ /cygwin/i)) {
            die "cygwin perl not found!\n";
        }
    }
    #my $newpath = join("\n", split(';',$ENV{PATH}));
    #out("PATH is now\n$newpath");
} elsif ($ENV{NMI_PLATFORM} =~ /macos/i) {
    # CRUFT Once 9.0 is EOL, remove the python.org version of python3
    #   from the mac build machines and remove this setting of PATH.
    # Ensure we're using the system python3
    $ENV{PATH} ="/usr/bin:$ENV{PATH}";
}

out("RUNNING run_test.pl '$testname'\n    -X-X-X-X-X-X- run_test.pl output -X-X-X-X-X-X-");
my $batchteststatus = system("$perl_exe run_test.pl $testname");
print "    -X-X-X-X-X-X- end of run_test.pl output -X-X-X-X-X-X-\n"; # we print here rather than using out() in order to bracket the run_test output with ----- above and below.
my @returns = TestGlue::ProcessReturn($batchteststatus);

$batchteststatus = $returns[0];
my $signal = $returns[1];
my $signalmsg = $signal ? " and signal $signal" : "";
out("run_test.pl '$testname' returned $batchteststatus" . $signalmsg);

# if we started a personal HTCondor, stop it now, if we can't, then fail the test.
#
my $teststatus = $batchteststatus;
if ($start_personal_if) {
	if(exists $requirements->{personal}) {
		out("Stopping personal HTCondor for '$testname'");
		my $personalstatus = StopTestPersonal($testname);
		if($personalstatus != 0 && $teststatus == 0) {
			print "\tTest succeeded, but condor failed to shut down or there were\n";
			print "\tcore files or error messages in the logs. see CondorTest::EndTest\n";
			$teststatus = $personalstatus;
		}
	}
}

# report here figure if the test passed or failed.
my $run_success = $batchteststatus ? 0 : 1; # 0 is success for run_test.pl, but 1 is success for RegisterResult
CondorTest::RegisterResult($run_success,test_name=>"remote_task.pl", check_name=>$testname);

######################################################################
# print output from .run script to stdout of this task, and final exit
######################################################################

my $run_out       = "$testname.run.out";
my $test_out      = "$testname.out";
my $test_err      = "$testname.err";
out("Looking for $testname.run.out and $testname.out/.err files");

my $run_out_path  = $run_out;
my $test_out_path = $test_out;
my $test_err_path = $test_err;

if ($compiler ne '.') {
	# TJ 2020, obsolete?
	my $compiler_path = File::Spec->catpath($test_dir, $compiler);
	chdir($compiler_path) || c_die("Can't chdir($compiler_path): $!\n");
	$run_out_path  = File::Spec->catfile($compiler_path, $run_out);
	$test_out_path = File::Spec->catfile($compiler_path, $test_out);
	$test_err_path = File::Spec->catfile($compiler_path, $test_err);

	print "\tcompiler_path: $compiler_path\n\trun_out: $run_out\n\trun_out_path: $run_out_path\n\tcompiler_path: $compiler_path\n";
	print "\ttest_out: $test_out\n\ttest_out_path: $test_out_path\n\ttest_err: $test_err\n\ttest_err_path: $test_err_path\n";
}
#print "Here now:\n";
#system("pwd;ls -lh $run_out_path");

if( ! -f $run_out_path ) {
    if( $teststatus == 0 ) {
        # if the test passed but we don't have a run.out file, we
        # should consider that some kind of weird error
        c_die("WTF ERROR: test passed but $run_out does not exist!");
    }
    else {
        # if the test failed, this isn't suprising.  we can print it, 
        # but we should just treat it as if the test failed, not an
        # internal error. 
        out("No test output... Test failed and $run_out does not exist");
    }
}
else {
    # spit out the contents of the run.out file to the stdout of the task
    if( open(RES, '<', $run_out_path) ) {
        out("\n    -X-X-X-X-X-X- Start of $run_out -X-X-X-X-X-X-");
        while(<RES>) {
            print "$_";
        }
        close RES;
        print "\n    -X-X-X-X-X-X- End of $run_out -X-X-X-X-X-X-\n";
    }
    else {
        out("Printing test output...  ERROR: failed to open $run_out_path: $!");
    }
}


print "\n********************** EXIT REMOTE_TASK.$$: Status:$teststatus ******************\n";
exit $teststatus;

######################################################################
# helper methods
######################################################################

sub ProcessTestRequirements
{
	my $requirements;
    my $record = 0;
    my $conf = "";

    if (open(TF, "<${testname}.run")) {
        while (my $line = <TF>) {
            CondorUtils::fullchomp($line);
            if($line =~ /^#testreq:\s*(.*)/) {
                my @requirementList = split(/ /, $1);
                foreach my $requirement (@requirementList) {
                    $requirement =~ s/^\s+//;
                    $requirement =~ s/\s+$//;
                    out("${testname} needs '${requirement}'");
                    $requirements->{ $requirement } = 1;
                }
            }

            if($line =~ /<<CONDOR_TESTREQ_CONFIG/) {
                $record = 1;
                next;
            }

            if($line =~ /^#endtestreq/) {
                $record = 0;
                last;
            }

            if($record && $line =~ /CONDOR_TESTREQ_CONFIG/) {
                $requirements->{testconf} = "TEST_DIR = ${test_dir}\n";
                $requirements->{testconf} .= $conf . "\n";
                $record = 0;
            }

            if ($record) {
                $conf .= $line . "\n";
            }
        }
    }

    return $requirements;
}

sub c_die {
    my($msg) = @_;
    out($msg);
    exit 3;
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

    my $configfile = CondorTest::CreateLocalConfig($firstappend_condor_config,"remotetask$test");

  eval {
    CondorTest::StartCondorWithParams(
        condor_name => "remotetask$test",
        fresh_local => "TRUE",
        condorlocalsrc => "$configfile",
        test_glue => "TRUE",
    ); 1;
  } or do {
    my $msg = $@ || "Condor did not start";
    say STDERR "$msg";
    exit(136); # metronome wants 136 for setup error
  }
}

sub showEnv {
	foreach my $env (sort keys %ENV) {
		print "$env: $ENV{$env}\n";
	}
}

sub StopTestPersonal {
	my $exit_status = 0;
	$exit_status = CondorTest::EndTest("no_exit");
	return($exit_status);
}
