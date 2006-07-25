#!/usr/bin/env perl
use IO::Socket;
use IO::Handle;
use Socket;

my $SockAddr = $ARGV[0];
my $LogFile = $ARGV[1];

open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!";
open(STDERR, ">&STDOUT");
select(STDERR); $| = 1;
select(STDOUT); $| = 1;

unlink($SockAddr);

my $server = IO::Socket::UNIX->new(Local => $SockAddr,
								Type  => SOCK_DGRAM)
or die "Can't bind socket: $!\n";

$server->setsockopt(SOL_SOCKET, SO_RCVBUF, 65440);


while ( 1 )
{
	my $newmsg;
	my $MAXLEN = 1024;
	#$server->recv($newmsg,$MAXLEN) || die "Recv: $!";
	$server->recv($newmsg,$MAXLEN);
	print "$newmsg\n";
	if($newmsg eq "quit")
	{
		exit(0);
	}
}

my $stat = 0;
my $returnval = shift;
print "Server exiting\n";
