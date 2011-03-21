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

use File::Copy;

if( ! defined $ENV{_NMI_TASKNAME} ) {
    die "_NMI_TASKNAME not in environment, can't test anything!\n";
}
my $fulltestname = $ENV{_NMI_TASKNAME};

my $testenvtst =   $ENV{GCBTARGET};
print "Test for test environment variable GCBTARGET yields <$testenvtst>\n";

if( $fulltestname =~ /remote_task/) {
	die "_NMI_TASKNAME set to remote_task meaning 0 tests seen!\n";
}

if( ! $fulltestname ) {
    # if we have no task, just return success immediately
    print "No tasks specified, returning SUCCESS\n";
    exit 0;
}

my $BaseDir = $ENV{BASE_DIR} || c_die("BASE_DIR is not in environment!\n");
my $testdir = "condor_tests";

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

@testinfo = split(/\./, $fulltestname);
my $testname = $testinfo[0];
my $compiler = $testinfo[1];

if( ! $testname ) {
    c_die("Invalid input for testname\n");
}

#track time.....

system("date");

print "testname is $testname\n";
if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
    if( $compiler ) {
        print "compiler is $compiler\n";
        $targetdir = "$BaseDir/$testdir/$compiler";
    } else {
        $compiler = ".";
        $targetdir = "$BaseDir/$testdir";
    }
} else {
    $compiler = ".";
    $targetdir = "$BaseDir/$testdir";
}

######################################################################
# run the test using batch_test.pl
######################################################################

print "RUNNING $testinfo\n";
chdir("$BaseDir/$testdir") || c_die("Can't chdir($BaseDir/$testdir): $!\n");

print "About to run batch_test.pl\n";

# -b means build & test and ensures the first time that
# we have our testing personal condor configered from
# release generic config files.

system("perl ./batch_test.pl --no-error -d $compiler -t $testname -b");
$batchteststatus = $?;

# figure out here if the test passed or failed.  
if( $batchteststatus != 0 ) {
    $teststatus = 2;
} else {
    $teststatus = 0;
}

######################################################################
# print output from .run script to stdout of this task, and final exit
######################################################################

chdir( "$BaseDir/$testdir/$compiler" ) ||
  c_die("Can't chdir($BaseDir/$testdir/$compiler): $!\n");
$local_out = "$BaseDir/$testdir/TestingPersonalCondor/condor_config.local";
$run_out = "$testname.run.out";
$run_out_full = "$BaseDir/$testdir/$compiler/$run_out";
$test_out = "$testname.out";
$test_out_full = "$BaseDir/$testdir/$compiler/$test_out";
$test_err = "$testname.err";
$test_err_full = "$BaseDir/$testdir/$compiler/$test_err";

if( ! -f $run_out_full ) {
    if( $teststatus == 0 ) {
        # if the test passed but we don't have a run.out file, we
        # should consider that some kind of weird error
        c_die("ERROR: test passed but $run_out does not exist!");
    } else {
        # if the test failed, this isn't suprising.  we can print it, 
        # but we should just treat it as if the test failed, not an
        # internal error. 
        print "\n\nTest failed and $run_out does not exist\n";
    }
} else {
    # spit out the contents of the run.out file to the stdout of the task
    if( open(RES, "<$run_out_full") ) {
        print "\n\n----- Start of $run_out -----\n";
        while(<RES>) {
            print "$_";
        }
        close RES;
        print "\n----- End of $run_out -----\n";
    } else {
        print "\n\nERROR: failed to open $run_out_full: $!\n";
    }
}
# add test.out and test.err to run output if they exist

# spit out the contents of the test.out file to the stdout of the task
if( open(RES, "<$test_out_full") ) {
    print "\n\n----- Start of $test_out -----\n";
    while(<RES>) {
        print "$_";
    }
    close RES;
    print "\n----- End of $test_out -----\n";
} else {
    print "\n\nERROR: failed to open $test_out_full: $!\n";
}

# spit err the contents of the test.err file to the stdout of the task
if( open(RES, "<$test_err_full") ) {
    print "\n\n----- Start of $test_err -----\n";
    while(<RES>) {
        print "$_";
    }
    close RES;
    print "\n----- End of $test_err -----\n";
} else {
    print "\n\nERROR: failed to open $test_err_full: $!\n";
}
# add  local config file
if( open(RES, "<$local_out") ) {
    print "\n\n----- Start of TestingPersonalCondor/condor_config.local -----\n";
    while(<RES>) {
        print "$_";
    }
    close RES;
    print "\n----- End of TestingPersonalCondor/condor_config.local -----\n";
} else {
    print "\n\nERROR: failed to open $test_err_full: $!\n";
}

exit $teststatus;

######################################################################
# helper methods
######################################################################

sub c_die {
    my( $msg ) = @_;
    print $msg;
    exit 3;
}
