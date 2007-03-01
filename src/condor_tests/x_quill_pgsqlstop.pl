#!/usr/bin/env perl

my $prefix = $ARGV[0];

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

$stipit = system("$prefix/bin/pg_ctl -D $prefix/postgres-data stop");
if( $stipit != 0 ) {
	die "Stop of postgress failed\n";
} 

print "Stop of Postgress was good......\n";
exit(o);
