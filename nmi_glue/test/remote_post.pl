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


print "Seeing if personal condor needs killing\n";

if( -f "$SrcDir/condor_tests/TestingPersonalCondor/local/log/.scheduler_address" ) {
    #  Came up and had a scheduler running. good
	$ENV{"CONDOR_CONFIG"} = "$SrcDir/condor_tests/TestingPersonalCondor/condor_config";
	if( defined $ENV{_NMI_STEP_FAILED} ) { 
		print "not calling condor_off for $SrcDir/condor_tests/TestingPersonalCondor/condor_config\n";
	} else {
		print "calling condor_off for $SrcDir/condor_tests/TestingPersonalCondor/condor_config\n";
		system("$BaseDir/userdir/condor/sbin/condor_off -master");
		# give some time for condor to shutdown
		sleep(30);
		print "done calling condor_off for $SrcDir/condor_tests/TestingPersonalCondor/condor_config\n";
	}
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

print "cding to $BaseDir \n";
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

