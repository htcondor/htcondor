#!/usr/bin/perl
use strict;
use warnings;
use IO::Socket;
use IO::Handle;
use Socket;

my $MAXLEN = 1024;
my $comchan = $ARGV[0];
my $newmsg = $ARGV[1];

my $client = IO::Socket::UNIX->new(Peer => "$comchan",
								Type  => SOCK_DGRAM,
								Timeout => 10)
or die $@;

$client->send($newmsg);



#my $LogFile = "dag_ret.log";

#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR); $| = 1;
#select(STDOUT); $| = 1;

my $stat = 0;
my $returnval = shift;
print "client exiting\n";
