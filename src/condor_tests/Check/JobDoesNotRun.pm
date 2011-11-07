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

package JobDoesNotRun;

use CondorTest;

sub RunCheck
{
    my %args = @_;
    my $testname = $args{test_name} || CondorTest::GetDefaultTestName();
    my $universe = $args{universe} || "vanilla";
    my $append_submit_commands = $args{append_submit_commands} || "";
    my $timeout = 60;

    my $user_log = CondorTest::TempFileName("$testname.user_log");

    my $submit_fname = CondorTest::TempFileName("$testname.submit");
    open( SUBMIT, ">$submit_fname" ) || die "error writing to $submit_fname: $!\n";
    print SUBMIT "universe = $universe\n";
    print SUBMIT "executable = x_sleep.pl\n";
    print SUBMIT "log = $user_log\n";
    print SUBMIT "arguments = 1\n";
    print SUBMIT "notification = never\n";
    if( $append_submit_commands ne "" ) {
        print SUBMIT "\n" . $append_submit_commands . "\n";
    }
    print SUBMIT "queue\n";
    close( SUBMIT );

    my $jobid = Condor::Submit($submit_fname);

    if( $jobid == 0 ) {
        CondorTest::RegisterResult( 0, %args );
        return 0;
    }

    my $q_cmd = "condor_q " .
       "-format \"%d \" 'LastMatchTime=!=undefined' " .
       "-format \"%d \" 'LastRejMatchTime=!=undefined' " .
       "-format \"%d\\n\" 'JobStatus' " .
       "$jobid";

    CondorTest::debug("monitoring job with following command: $q_cmd\n",1);

    my $job_was_considered = 0;
    my $result = 1;
    my $i = 1;
    while( $i < $timeout ) {
        $i=$i+1;

        my $q_output = `$q_cmd`;
        my @fields=split(" ",$q_output);

        my $job_matched = $fields[0];
        my $job_rejected = $fields[1];
        my $job_status = $fields[2];

        if( $job_status ne "1" ) {
            $result = 0;
            CondorTest::debug("Job status is $job_status.  Bad!",1);
            last;
        }
        if( $job_matched eq "1" || $job_rejected eq "1" ) {
            $job_was_considered=$job_was_considered+1;
        }
        # We wait a bit after first seeing that the job was considered.
        # If the job still has not run, then we assume it never will.
        if( $job_was_considered > 5 ) {
            CondorTest::debug("Job was considered but has not run.  Good!",1);
            last;
        }

        sleep(1);
    }

    CondorTest::runcmd("condor_rm $jobid");

    CondorTest::RegisterResult( $result, %args );
    return $result;
}

1;
