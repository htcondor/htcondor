#!/usr/bin/env perl
use CondorTest;
use Expect;
use Socket;
use Sys::Hostname;

my $prefix = $ARGV[0];
my $installlog = $ARGV[1];
my $tarfile = $ARGV[2];
my $rdlnversion = $ARGV[3];

print "Prefix=<<$prefix>> installlog=<<$installlog>> tarfile=<<$tarfile>> rdlnversion=<<$rdlnversion>>\n";

my $startpostmasterport = 5432;
my $postmasterinc = int(rand(100));

#my $dbinstalllog =  $installlog;
#print "Trying to open logfile... $dbinstalllog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">>$dbinstalllog") or die "Could not open $dbinstalllog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
#select(STDOUT);
 #$| = 1;

# need the hostname for a name to ipaddr conversion
print "Moving to this dir <<$prefix>>\n";

system("pwd");
if( -f $tarfile  ){
	system("tar -zxvf ./$tarfile");
	chdir("$rdlnversion");
	system("pwd");
	system("./configure --prefix=$prefix");
	system("make");
	system("make install");
	chdir("..");
} else {
	print "<<$tarfile>> does not exist here :-(\n";
}

exit;
