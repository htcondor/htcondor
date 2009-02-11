#! /usr/bin/env perl
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

use CondorTest;

$cmd      = 'job_filexfer_base-input4_van.cmd';
$testdesc =  'Jobs run with imput when transfer_input = false - vanilla U';
$testname = "job_filexfer_base-input4_van";

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my $alreadydone=0;

$execute = sub {
	my %args = @_;
	my $cluster = $args{"cluster"};

	die "Running $cluster\n";

};

$abort = sub {
	die "Job is gone now, cool.\n";
};

$wanterror = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
	my $errmsg = $args{"ErrorMessage"};

	CondorTest::debug("Submit error message:<<$errmsg>>\n",1);

	if($errmsg =~ /^.*died abnormally.*$/) {
		CondorTest::debug("BAD. Submit died was to fail but with error 1\n",1);
		CondorTest::debug("$testname: Failure\n",1);
		exit(1);
	} elsif($errmsg =~ /^.*\(\s*returned\s*(\d+)\s*\).*$/) {
		if($1 == 1) {
			CondorTest::debug("Good. Job was not to submit with File Transfer off and input files requested\n",1);
			CondorTest::debug("$testname: SUCCESS\n",1);
			return(0);
		} else {
			CondorTest::debug("BAD. Submit was to fail but with error 1 not <<$1>>\n",1);
			CondorTest::debug("$testname: Failure\n",1);
			return(1);
		}
	} else {
			CondorTest::debug("BAD. Submit failure mode unexpected....\n",1);
			CondorTest::debug("$testname: Failure\n",1);
			return(1);
	}
};

$success = sub
{
	die "Base file transfer job!\n";
};

$timed = sub
{
	die "Job should have ended by now. file transfer broken!\n";
};


# make some needed files. All 0 sized and xxxxxx.txt for
# easy cleanup

my $job = $$;
CondorTest::debug("Process Id for this script is  $job\n",1);
my $basefile = "submit_filetrans_input" . "$job";
my $in = "$basefile".".txt";
my $ina = "$basefile"."a.txt";
my $inb = "$basefile"."b.txt";
my $inc = "$basefile"."c.txt";
my $inputdir = "job_"."$job"."_dir";

system("mkdir -p $inputdir");
system("touch $inputdir/$in");
system("touch $inputdir/$ina");
system("touch $inputdir/$inb");
system("touch $inputdir/$inc");

my $line = "";
my $args = "--job=$job --extrainput ";

# pass pid for output file production
# open submitfile and fix
open(CMD,"<$cmd") || die "Can not open command file: $!\n";
open(NEWCMD,">$cmd.new") || die "Can not open command file: $!\n";
while(<CMD>)
{
	CondorTest::fullchomp($_);
	CondorTest::debug("$_\n",1);
	$line = $_;
	if( $line =~ /^\s*input\s*=\s*job_\d+_dir\/([a-zA-Z_]+)\d*\.txt\s*$/)
	{
		my $input = "$1"."$job".".txt";
		CondorTest::debug("Input file was $1\n",1);
		print NEWCMD "input = $inputdir/$input\n"
	}
	elsif( $line =~ /^\s*transfer_input_files\s*=\s*.*$/ )
	{
		print NEWCMD "transfer_input_files = $inputdir/$ina,$inputdir/$inb,$inputdir/$inc\n";
	}
	elsif( $line =~ /^\s*arguments\s*=\s*.*$/)
	{
		print NEWCMD "arguments = $args\n";
	}
	else
	{
		print NEWCMD "$line\n";
	}

}
close(CMD);
close(NEWCMD);
system("mv $cmd.new $cmd");

CondorTest::RegisterExecute($testname, $execute);
CondorTest::RegisterAbort($testname, $abort);
CondorTest::RegisterWantError($testname, $wanterror);
CondorTest::RegisterExitedSuccess($testname, $success);
#CondorTest::RegisterTimed($testname, $timed, 3600);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

