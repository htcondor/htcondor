#!/usr/bin/env perl
use CondorTest;

my $prefix = $ARGV[0];
my $installlog = $ARGV[1];
my $tarfile = $ARGV[2];
my $pgsqlversion = $ARGV[3];

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

chdir("$prefix");
system("tar -zxvf ../$tarfile");
chdir("$pgsqlversion");
system("./configure --prefix=$prefix");
system("make");
system("make install");
system("mkdir $prefix/postgres-data");
system("$prefix/bin/initdb -D $prefix/postgres-data ");
system("$prefix/bin/pg_ctl -D $prefix/postgres-data  -l $prefix/postgres-data/logfile start");

system("$prefix/bin/createdb test");
#system("$prefix/bin/psql test");


