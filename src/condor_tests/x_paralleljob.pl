#! /usr/bin/env perl
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
print "current directory is $curcwd\n";
print "current directory is $newsocketname\n";

if( $nodenum == 0 ) { 
	print "Looking for $mygoal messages in node 0 job....\n";
	print "socket is $newsocketname\n";
	print "Node <0> waiting.....\n"; 
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
		print "$newmsg\n";
		$mymesgcnt = $mymesgcnt + 1;
		print "Node 0 has seen << $mymesgcnt >> messages\n";
		if($mymesgcnt == $mygoal)
		{
			print "Expected messages all in\n";
			exit(0);
		}
	}
} else {
	print "Node <$nodenum>\n"; 
	print "socket is $newsocketname\n";
	chdir("$mypid");
	my $count = 0;
	my $client;
	while( !(-S "$newsocketname")) {
		sleep 6;
		print "waiting for socket\n";
	}
	while($count < 10){
		$client = IO::Socket::UNIX->new(Peer => "$newsocketname",
								Type  => SOCK_DGRAM,
								Timeout => 10);
		if($client) {
			print "Ready to write my message<< node $nodenum>>\n";
			last;
		}
		sleep (2 * $count);
	}
	if($count == 10) {
		die "Could never open handle to server\n";
	}

	$client->send("hello world");
}
