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

my $thisrunjobs = $ARGV[0];
my $increasejobs = $ARGV[1];
my $lastduration = $ARGV[2];
my $datafile = $ARGV[3];
my $cmd = $ARGV[4];
my $testname = $ARGV[5];


my $completedgood = 0;
my $completedbad = 0;



my $percentmorejobs = ($increasejobs / $thisrunjobs) * 100;
my $percentmoretime = 0;

#system("ls;pwd");

print "Doing $percentmore percent more jobs this run\n";
sleep 3;

my $line = "";
open(DATAOUT,"<$datafile") || print "Can not open existing data file: $!\n";
open(NEWDATAOUT,">$datafile.new") || die "Can not open new data file: $!\n";
while(<DATAOUT>)
{
	CondorTest::fullchomp($_);
	print NEWDATAOUT "$_\n";
}
close(DATAOUT);

my $doonlyonce = "";
my $starttime = time();
my $stoptime = 0;

open(QUEUECMD,"<$cmd") || die "Can't open cmd file $!\n";
open(NEWCMD,">$cmd.new") || die "Can not open new command file: $!\n";
while(<QUEUECMD>)
{
	CondorTest::fullchomp($_);
	$line = $_;
	if( $line =~ /^\s*queue\s*.*$/ )
	{
		print NEWCMD "queue $thisrunjobs\n";
	}
	else
	{
		print NEWCMD "$line\n";
	}
}
close(QUEUECMD);
close(NEWCMD);
system("mv $cmd.new $cmd");

$ExitSuccess = sub 
{
	my %info = @_;

	$completedgood += 1;
	print "Completed count: $completedgood\n";
	if( $completedgood == $thisrunjobs )
	{
		print "Completed Done Count: $completedgood\n";
		$stoptime = time();
		my $duration = ($stoptime - $starttime);
		my $unittime = ($duration)/($thisrunjobs);
		if( $lastduration != 0 )
		{
			$percentmoretime = (($duration - $lastduration) / $lastduration) * 100;
		}
		print "Completed time per test: $unittime\n";
		print NEWDATAOUT "Scaling($thisrunjobs) Duration($duration) UnitTime($unittime) JobIncrease($percentmorejobs) TimeIncrease($percentmoretime)\n";
		close(NEWDATAOUT);
		system("mv $datafile.new $datafile");
		return(0);
	}
};

CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

