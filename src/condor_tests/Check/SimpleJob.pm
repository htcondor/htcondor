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

package SimpleJob;

use CondorTest;

$submitted = sub
{
	CondorTest::debug("Job submitted\n",1);
};

$aborted = sub 
{
	die "Abort event NOT expected\n";
};

$execute = sub
{
};

$ExitSuccess = sub {
	CondorTest::debug("Job completed\n",1);
};

sub RunTest
{
    my %args = @_;
    my $checkname = CondorTest::GetCheckName(__FILE__,%args);
    my $testname = $args{test_name} || CondorTest::GetDefaultTestName();
    my $universe = $args{universe} || "vanilla";
    my $user_log = $args{user_log} || CondorTest::TempFileName("$testname.user_log");

    CondorTest::RegisterAbort( $testname, $aborted );
    CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
    CondorTest::RegisterExecute($testname, $execute);
    CondorTest::RegisterSubmit( $testname, $submitted );

    my $submit_fname = CondorTest::TempFileName("$testname.submit");
    open( SUBMIT, ">$submit_fname" ) || die "error writing to $submit_fname: $!\n";
    print SUBMIT "universe = $universe\n";
    print SUBMIT "executable = x_sleep.pl\n";
    print SUBMIT "log = $user_log\n";
    print SUBMIT "arguments = 1\n";
    print SUBMIT "notification = never\n";
    print SUBMIT "queue\n";
    close( SUBMIT );

    my $result = CondorTest::RunTest($testname, $submit_fname, 0);
    CondorTest::RegisterResult( $testname, $checkname, $result);
    return $result;
}

1;
