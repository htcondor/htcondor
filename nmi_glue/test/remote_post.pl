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
# $Id: remote_post.pl,v 1.12.6.2 2008-02-08 17:21:45 bt Exp $
# post script for Condor testsuite runs
######################################################################

use File::Copy;

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $SrcDir = $ENV{SRC_DIR} || die "SRC_DIR not in environment!\n";
my $testdir = "condor_tests";
my $exit_status = 0;

# This is debugging output for the sake of the NWO infrastructure.
# However, it might be useful to us some day so we can see what's
# going on in case of failures...
if( defined $ENV{_NMI_STEP_FAILED} ) { 
    my $nmi_task_failure = "$ENV{_NMI_STEP_FAILED}";
    print "The value of _NMI_STEP_FAILED is: '$nmi_task_failure'\n";
} else {
    print "The _NMI_STEP_FAILED variable is not set\n";
}


######################################################################
# kill test suite personal condor daemons
######################################################################


if( -f "$SrcDir/condor_tests/TestingPersonalCondor/local/log/.scheduler_address" ) {
    #  Came up and had a scheduler running. good
	$ENV{"CONDOR_CONFIG"} = "$SrcDir/condor_tests/TestingPersonalCondor/condor_config";
	system("$BaseDir/userdir/condor/sbin/condor_off -master");
	# give some time for condor to shutdown
	sleep(30);
} else {
    # if there's no pid_file, there must be no personal condor running
    # which we'd have to kill.  this would be caused by an empty
    # tasklist.  so, make sure the tasklist is empty.  if so, we can
    # move on with success.  if there are tasks but no pid_file,
    # that's a weird fatal error and we should propagate that.
    if( ! -f "tasklist.nmi" || -z "tasklist.nmi" ) {
        # our tasklist is empty, good.
        print "tasklist.nmi is empty and there's no condor_master_pid " .
            "file.\nNothing to cleanup, returning SUCCESS.\n";
    } else {
        print "ERROR: tasklist.nmi contains data but " .
            "condor_master_pid does not exist!\n";
        $exit_status = 1;
    }
}
    

######################################################################
# save and tar up test results
######################################################################

if( ! -f "tasklist.nmi" || -z "tasklist.nmi" ) {
    # our tasklist is empty, so don't do any real work
    print "No tasks in tasklist.nmi, nothing to do\n";
    exit $exit_status;
}

chdir("$BaseDir") || die "Can't chdir($BaseDir): $!\n";

#----------------------------------------
# final tar and exit
#----------------------------------------

$results = "results.tar.gz";
print "Tarring up all results\n";
chdir("$BaseDir") || die "Can't chdir($BaseDir): $!\n";
system( "tar zcf $results --exclude *.exe src/condor_tests local" );
# don't care if condor is still running or sockets
# are being skipped. Save what we can and don't bitch
#if( $? >> 8 ) {
    #print "Can't tar zcf src/condor_tests local\n";
    #$exit_status = 1;
#}

exit $exit_status;

my $etc_dir = "$BaseDir/results/etc";
my $log_dir = "$BaseDir/results/log";

if( ! -d "$BaseDir/results" ) {
    # If there's no results, and we can't even make the directory, we
    # might as well die, since there's nothing worth saving...
    mkdir( "$BaseDir/results", 0777 ) || die "Can't mkdir($BaseDir/results): $!\n";
}


#----------------------------------------
# debugging info from the personal condor
#----------------------------------------

if( ! -d $etc_dir ) {
    if( ! mkdir( "$etc_dir", 0777 ) ) {
        print "ERROR: Can't mkdir($etc_dir): $!\n";
        $exit_status = 1;
    }
}
if( -d $etc_dir ) {
    print "Copying config files to $etc_dir\n";
    system( "cp $SrcDir/condor_tests/TestingPersonalCondor/condor_config $etc_dir" );
    if( $? >> 8 ) {
        print "Can't copy $SrcDir/condor_tests/TestingPersonalCondor/condor_config to $etc_dir\n";
        $exit_status = 1;
    }
    system( "cp $SrcDir/condor_tests/TestingPersonalCondor/condor_config.local $etc_dir" );
    if( $? >> 8 ) {
        print "Can't copy $SrcDir/condor_tests/TestingPersonalCondor/condor_config.local to $etc_dir\n";
        $exit_status = 1;
    }
}

if( ! -d $log_dir ) {
    if( ! mkdir( "$log_dir", 0777 ) ) {
        print "ERROR: Can't mkdir($log_dir): $!\n";
        $exit_status = 1;
    }
}
if( -d $log_dir ) {
    print "Copying log files to $log_dir\n";
    system( "cp $SrcDir/condor_tests/TestingPersonalCondor/local/log/*Log* $log_dir" );
    if( $? >> 8 ) {
        print "Can't copy $SrcDir/condor_tests/TestingPersonalCondor/local/log/ to $log_dir\n";
        $exit_status = 1;
    }
}
 

#----------------------------------------
# save output from tests
#----------------------------------------

system( "cp tasklist.nmi $BaseDir/results/" );
if( $? ) {
    print "Can't copy tasklist.nmi to $BaseDir/results\n";
    $exit_status = 1;
}

open (TASKFILE, "tasklist.nmi") || die "Can't tasklist.nmi: $!\n";
while( <TASKFILE> ) {
    chomp;
	my $testcopy = 1;
	my ($taskname, $timeout) = split(/ /);
	# iterations have numbers placed at the end of the name
	# for unique db tracking in nmi for now.
	if($taskname =~ /([\w\-\.\+]+)-(\d+)/) {
		$testcopy = $2;
		$taskname = $1;
	}
    my ($testname, $compiler) = split(/\./, $taskname);
    if( $compiler eq "." ) {
        $resultdir = "$BaseDir/results/base";
    } else {
        $resultdir = "$BaseDir/results/$compiler";
    }
    if( ! -d "$resultdir" ) {
        mkdir( "$resultdir", 0777 ) || die "Can't mkdir($resultdir): $!\n";
    }
    chdir( "$SrcDir/$testdir/$compiler" ) ||
      die "Can't chdir($SrcDir/$testdir/$compiler): $!\n";

    # first copy the files that MUST be there.
    copy_file( "$testname*.run.out", $resultdir, true );

    # these files are all optional.  if they exist, we'll save 'em, if
    # they do not, we don't worry about it.

	@savefiles = glob "$testname*.out* $testname*.err* $testname*.log $testname*.cmd.out";
	foreach $target (@savefiles) {
		copy_file( $target, $resultdir, false );
	}

    # if it exists, tarup the 'saveme' subdirectory for this test, which
    # may contain test debug info etc. Sometimes we run endurance tests
	# and we really only need to tar up the results once.
	# Except 1/15/08 we are taring after the individual tests to
	# keep disk use lower.
	# Except, 5/15/08, if the test is timed out the saveme never
	# gets tared up. So if it is still there, tar it up now
	if( -d "$testname.saveme") {
		system("tar -zcvf $testname.saveme.tar.gz $testname.saveme");
	}
    if( (-f "$testname.saveme.tar.gz") && ($testcopy == 1) ) {
            print "Saving $testname.saveme.tar.gz.\n";
            copy_file( "$testname.saveme.tar.gz", $resultdir, true );
    }
}


#----------------------------------------
# final tar and exit
#----------------------------------------

$results = "results.tar.gz";
print "Tarring up all results\n";
chdir("$BaseDir") || die "Can't chdir($BaseDir): $!\n";
system( "tar zcf $BaseDir/$results results" );
if( $? >> 8 ) {
    print "Can't tar zcf $BaseDir/$results results\n";
    $exit_status = 1;
}

exit $exit_status;


######################################################################
# helper methods
######################################################################

sub copy_file {
    my( $src, $dest, $required ) = @_;
    my $had_error = false;
    copy($src, $dest);
    if( $? >> 8 ) {
        if( $required ) {
            print "ERROR: Can't copy $src to $dest: $!\n";
        } else {
            print "Optional file $src not copied into $dest: $!\n";
        }
        $had_error = true;
    } else {
        print "Copied $src to $dest\n";
    }
    return $had_error;
}

