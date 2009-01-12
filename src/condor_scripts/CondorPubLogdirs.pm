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

package CondorPubLogdirs;

use strict;
use Cwd;

my $DEBUG = 1;
my $DEBUGLEVEL = 1;

BEGIN
{
}

sub reset
{
}

sub DebugLevel
{
    my $newlevel = shift;
    $DEBUGLEVEL = $newlevel;
}

sub DebugOn
{
    $DEBUG = 1;
}

sub DebugOff
{
    $DEBUG = 0;
}

sub debug
{
	my $string = shift;
	my $level = shift;
	if(!(defined $level)) {
		print( "", timestamp(), ": $string" ) if $DEBUG;
	} elsif($level <= $DEBUGLEVEL) {
		print( "", timestamp(), ": $string" ) if $DEBUG;
	}
}

sub timestamp {
    return scalar localtime();
}

sub CheckLogServer
{
	my $servername = $ENV["SEND_LOGS"];
	print "Log Server seems to be <<$servername>>\n";
	if($servername =~ /^LogServer\d+$/){
		print "LogServer running{env clue}\n";
	} elsif(-f "LogServerHandle"){
		print "LogServer running{handle file(LogServerHandle)}\n";
	} else {
		print "Starting LogServer\n";
		StartLogServer();
	}
}

sub StartLogServer
{
	my $servername = "LogServer" . $$;
	my $storehandle = "LogServerHandle";
	print "Log Server name <<$servername>>\n";
	$ENV["SEND_LOGS"] = $servername;
	open(LS,">$storehandle") or die "Can not store <<$storehandle>>:$!\n";
	print LS "$servername\n";
	close(LS);
	system("rm -f LogDirs");
	defined(my $pid = fork) or die "Cannot fork: $!";
	unless ($pid)
	{
		#child runs and ceases to exist
		exec "./x_general_server.pl $servername LogDirs";
		die "can not exec dumpcore!\n";
	}
}

sub StopLogServer
{
	my $storehandle = "LogServerHandle";
	my $servername = $ENV["SEND_LOGS"];
	my $line = "";
	print "Log Server seems to be <<$servername>>\n";
	if($servername =~ /^LogServer\d+$/){
		print "LogServer running{env clue}\n";
	} elsif(-f "LogServerHandle"){
		print "LogServer running{handle file(LogServerHandle)}\n";
		open(KLS,"<$storehandle") or die "Can not open $storehandle:$!\n";
		while(<KLS>) {
			chomp();
			$line = $_;
			if($line =~ /^(LogServer\d+)$/){
				debug("LogServer handle of proper form\n",1);
				system("./x_general_client.pl $1 quit");
				system("rm -f $storehandle");
				$ENV["SEND_LOGS"] = "notdefined";
			} else {
				debug("LogServer handle of improper form, HELP\n",1);
			}
		}
	} else {
		print "Can not find evidence of log dir server\n";
	}
	
}
1;
