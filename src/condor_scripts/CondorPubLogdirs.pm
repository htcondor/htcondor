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
use warnings;
use Cwd;


my $DEBUG = 1;
my $DEBUGLEVEL = 1;
my $publishedlogdirs = "LogDirs";
my %published;

BEGIN
{
	$DEBUG = 1;
	$DEBUGLEVEL = 1;
	$publishedlogdirs = "LogDirs";
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
	my $servername = $ENV{SEND_LOGS};
	my @handle;
	if(exists $ENV{SEND_LOGS}) {
		debug( "Log Server seems to be <<$servername>> we are in:\n",2);
		if($servername =~ /^(.*\/LogServer\d+)$/){
			debug( "LogServer running{env clue}<<$1>>\n",2);
		}
	} elsif(-f "LogServerHandle"){
		debug( "LogServer running{handle file(LogServerHandle)}\n",2);
		system("cat LogServerHandle");
		open(HAN,"<LogServerHandle") or die "Can not read LogServerHandle:$!\n";
		@handle = <HAN>;
		$servername = $handle[0];
		fullchomp($servername);
	} else {
		debug( "Starting LogServer\n",1);
		$servername = StartLogServer();
	}
	return($servername);
}

sub StartLogServer
{
	my $current = getcwd();
	my $servername = "LogServer" . $$;
	my $storehandle = "LogServerHandle";
	$current = $current . "/" . $servername;
	debug( "Log Server name <<$servername>>\n",1);
	$ENV{SEND_LOGS} = $current;
	open(LS,">$storehandle") or die "Can not store <<$storehandle>>:$!\n";
	print LS "$current\n";
	close(LS);
	system("rm -f LogDirs");
	defined(my $pid = fork) or die "Cannot fork: $!";
	unless ($pid)
	{
		#child runs and ceases to exist
		exec "./x_general_server.pl $current LogDirs";
		die "can not exec dumpcore!\n";
	}
	debug( "Log Server returning as <<$current>>\n",2);
	return($current);
}

sub PublishLogDir
{
	my $test = shift;
	my $logdir = shift;
	my $logstring = $test . "," . $logdir;
	my $servername = $ENV{SEND_LOGS};
	debug("PublishLogDir: server name to publish to is<<$servername>>\n",2);
	if($servername =~ /^(.*\/LogServer\d+)$/){
		system("./x_general_client.pl $servername $logstring");
		debug("Publishing <<$logstring>>\n",1) ;
	} else {
		print "env{SEND_LOGS} yields<<<<$servername>>>>\n";
		die "We should always find server when in wrap test mode\n";
	}
}

sub StopLogServer
{
	my $storehandle = "LogServerHandle";
	my $servername = $ENV{SEND_LOGS};
	my $line = "";
	if(exists $ENV{SEND_LOGS}){
		debug( "Log Server seems to be <<$servername>>\n",2);
		debug( "LogServer running{env clue}\n",2);
		ShutdownServer( $servername, $storehandle);
	} elsif(-f "LogServerHandle"){
		debug( "LogServer running{handle file(LogServerHandle)}\n",2);
		open(KLS,"<$storehandle") or die "Can not open $storehandle:$!\n";
		while(<KLS>) {
			chomp();
			$line = $_;
			if($line =~ /^(.*\/LogServer\d+)$/){
				debug("LogServer handle of proper form<<$line>>\n",2);
				ShutdownServer( $1, $storehandle);
			} else {
				debug("LogServer handle of improper form, HELP<<$line>>\n",2);
			}
		}
	} else {
		debug( "Can not find evidence of log dir server\n",2);
	}
}

sub ShutdownServer
{
	my $socket = shift;
	my $storehandle =  shift;
	system("./x_general_client.pl $socket quit");
	debug("ShutdownServer: about to remove <<$storehandle>>\n",2);
	system("rm -f $storehandle");
	$ENV{SEND_LOGS} = "notdefined";
}

sub LoadPublished
{
	my $line = "";
	# empty hash before filling
	%published = ();
	open(EE,"<$publishedlogdirs") or die "Can not open $publishedlogdirs:$!\n";
	while(<EE>) {
    	chomp();
    	$line = $_;
    	debug( $line . "\n",2);
    	(my $test, my $logdir) = split /,/, $line;
    	if(exists $published{$test}) {
        	push @{$published{$test}}, $logdir;
    	} else {
        	$published{$test} = ();
        	push @{$published{$test}}, $logdir;
    	}
	}
	#DropPublished();
}

my @emptyreturn;
my @goodarray;

sub ReturnPublished
{
	my $test = shift;
	if( exists $published{$test}){
		@goodarray = @{$published{$test}};
		debug("ReturnPublished returning valid array size $#goodarray\n",2);
		return (\@goodarray);
	} else {
		debug("ReturnPublished returning empty array size $#emptyreturn\n",2);
		return (\@emptyreturn);
	}
}

sub DropPublished
{
	foreach my $key (sort keys %published) {
    	debug( "$key\n",1);
    	my @array = @{$published{$key}};
    	foreach my $p (@array) {
        	debug( "$p\n",1);
    	}
	}
}

#
# Cygwin's perl chomp does not remove cntrl-m but this one will
# and linux and windows can share the same code. The real chomp
# totals the number or changes but I currently return the modified
# array. bt 10/06
#

sub fullchomp
{
	push (@_,$_) if( scalar(@_) == 0);
	foreach my $arg (@_) {
		$arg =~ s/\012+$//;
		$arg =~ s/\015+$//;
	}
	return(0);
}

1;
