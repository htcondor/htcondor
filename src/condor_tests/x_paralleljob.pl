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
use Cwd;
use IO::Socket;
use IO::Handle;
use Socket;

$nodenum = $ARGV[0];
$nodecount = $ARGV[1];
$mypid = $ARGV[2];
$mygoal = $nodecount - 1;
$mymesgcnt = 0;


my $curcwd = getcwd();
my $socketname = "basic_par_socket";
#my $newsocketname = $curcwd . "/" . $mypid . "/job_core_basic_par";
my $newsocketname =  "job_core_basic_par";
CondorTest::debug("current directory is $curcwd\n",1);
CondorTest::debug("current directory is $newsocketname\n",1);

if( $nodenum == 0 ) { 
	CondorTest::debug("Looking for $mygoal messages in node 0 job....\n",1);
	CondorTest::debug("socket is $newsocketname\n",1);
	CondorTest::debug("Node <0> waiting.....\n",1);
	chdir("$mypid");
	my $server = IO::Socket::UNIX->new(Local => $newsocketname,
								Type  => SOCK_DGRAM)
	or die "Can't bind socket: $!\n";

	system("pwd");
	system("ls");
	$server->setsockopt(SOL_SOCKET, SO_RCVBUF, 65440);


	while ( 1 )
	{
		my $newmsg;
		my $MAXLEN = 1024;
		#$server->recv($newmsg,$MAXLEN) || die "Recv: $!";
		$server->recv($newmsg,$MAXLEN);
		CondorTest::debug("$newmsg\n",1);
		$mymesgcnt = $mymesgcnt + 1;
		CondorTest::debug("Node 0 has seen << $mymesgcnt >> messages\n",1);
		if($mymesgcnt == $mygoal)
		{
			CondorTest::debug("Expected messages all in\n",1);
			exit(0);
		}
	}
} else {
	CondorTest::debug("Node <$nodenum>\n",1);
	CondorTest::debug("socket is $newsocketname\n",1);
	chdir("$mypid");
	my $count = 0;
	my $client;
	while( !(-S "$newsocketname")) {
		sleep 6;
		CondorTest::debug("waiting for socket\n",1);
	}
	while($count < 10){
		$client = IO::Socket::UNIX->new(Peer => "$newsocketname",
								Type  => SOCK_DGRAM,
								Timeout => 10);
		if($client) {
			CondorTest::debug("Ready to write my message<< node $nodenum>>\n",1);
			last;
		}
		sleep (2 * $count);
	}
	if($count == 10) {
		die "Could never open handle to server\n";
	}

	$client->send("hello world");
}
