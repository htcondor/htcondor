#!/usr/bin/env perl
use CondorTest;
use Expect;
use Socket;
use Sys::Hostname;

my $prefix = $ARGV[0];
my $installlog = $ARGV[1];
my $tarfile = $ARGV[2];
my $pgsqlversion = $ARGV[3];
my $condorconfigadd = $ARGV[4];
my $morelibs = $ARGV[5];
my $testsrc = $ARGV[6];

my $startpostmasterport = 5432;
my $postmasterinc = int(rand(100));

print "Original port request is <<$startpostmasterport>>\n";
print "Random increment if needed will be <<$postmasterinc>>\n";

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

# for when we configure postgress for quill
my $mypostgresqlconf = $prefix . "/postgres-data/postgresql.conf";
my $mynewpostgresqlconf = $prefix . "/postgres-data/postgresql.new";
my $myquillchanges = $testsrc . "/x_postgress_quill.conf";

my $condorchanges = "$condorconfigadd";
my $newcondorchanges = "$condorconfigadd" . ".new";

my $mypgfile = $prefix . "/postgres-data/pg_hba.conf";
my $mynewpgfile = $prefix . "/postgres-data/pg_hba.new";


chdir("$prefix");
$dotar = system("tar -zxvf $tarfile");
if($dotar != 0) {
	die "TAR EXTRACTION OF $tarfile FAILED\n";
}
chdir("$pgsqlversion");
$doconfigure = system("./configure --prefix=$prefix --without-readline --disable-shared");
if($doconfigure != 0) {
	die "Configure failed\n";
}
$domake = system("make");
if($domake != 0) {
	die "Make failed\n";
}
$doinstall = system("make install");
if($doinstall != 0) {
	die "Install failed\n";
}
system("mkdir $prefix/postgres-data");

print "Create db\n";
$initdb = system("$prefix/bin/initdb -D $prefix/postgres-data") ;
if($initdb != 0) {
	die "Initdb failed\n";
}

# customize postgress before starting it
open(OLD,"<$mypostgresqlconf") || die "1 Can not open original conf file: $mypostgresqlconf($!)\n";
open(NEW,">$mynewpostgresqlconf") || die "2 Can not open original conf file: $mynewpostgresqlconf($!)\n";
# get original configuration
while(<OLD>) {
	print NEW "$_";
}

open(EXTRA,"<$myquillchanges") || die "3 Can not open quill conf file: $myquillchanges($!)\n";
# add quill changes
while(<EXTRA>) {
	print NEW "$_";
}

print  NEW "port = 5432\n";
close(NEW);
close(OLD);
close(EXTRA);

$mvconf = system("mv $mynewpostgresqlconf $mypostgresqlconf");
if($mvconf != 0) {
	die "INstalling modified configuration file <<$mypostgresqlconf>> failed\n";
}

open(OLD,"<$mypgfile") || die "4 Can not open original conf file: $mypgfile($!)\n";
open(NEW,">$mynewpgfile") || die "5 Can not open original conf file: $mynewpgfile($!)\n";
# get original configuration
while(<OLD>) {
	print NEW "$_";
}
close(OLD);
print NEW "host all quillreader 0.0.0.0 0.0.0.0 password\n";
print NEW "host all quillwriter 0.0.0.0 0.0.0.0 password\n";
close(NEW);

system("rm -f $mypgfile");
$mmvconf = system("cp $mynewpgfile $mypgfile");
if($mvconf != 0) {
	die "INstalling modified configuration file <<$mypgfile>> failed\n";
}

my $dbup = "no";
my $trylimit = 1;
# start db

while (( $dbup ne "yes") && ($trylimit <= 10) ) {
	$pmaster_start = system("$prefix/bin/pg_ctl -D $prefix/postgres-data -l $prefix/postgres-data/logfile start");
	print "starting postmaster result<<<<<$pmaster_start>>>>>\n";
	sleep(10);
	if(!(-f "$prefix/postgres-data/postmaster.pid")) {
		$trylimit = $trylimit + 1;
		open(OLD,"<$mypostgresqlconf") || die "6 Can not open original conf file: $mypostgresqlconf($!)\n";
		open(NEW,">$mynewpostgresqlconf") || die "7 Can not open original conf file: $mynewpostgresqlconf($!)\n";
		while(<OLD>) {
			if( $_ =~ /^port\s+=\s+/ ){
				$startpostmasterport = $startpostmasterport + $postmasterinc;
				print NEW "port = $startpostmasterport\n";
				print "Trying port $startpostmasterport($postmasterinc)\n";
			} else {
				print NEW ;
			}
		}
		close(OLD);
		close(NEW);
		system("rm -f $mypostgresqlconf");
		$mvconf = system("cp $mynewpostgresqlconf $mypostgresqlconf");
		if($mvconf != 0) {
			die "INstalling modified configuration file <<$mypostgresqlconf>> failed\n";
		}
	} else {
		$dbup = "yes";
	}
}

if($trylimit > 10) {
	die	"Tried 10 times to find a good port!!!!\n";;
} else {
	open(OLD,"<$condorchanges") || die "8 Can not open original conf file: $condorchanges($!)\n";
	open(NEW,">$newcondorchanges") || die "9 Can not open original conf file: $newcondorchanges($!)\n";
	while(<OLD>) {
		if( $_ =~ /^QUILL_DB_IP_ADDR/ ){
			print NEW "QUILL_DB_IP_ADDR = \$(CONDOR_HOST):$startpostmasterport\n";
			print "Trying port $startpostmasterport\n";
		} else {
			print NEW ;
		}
	}
	close(OLD);
	close(NEW);
	system("rm -f $condorchanges");
	$mvconf = system("cp $newcondorchanges $condorchanges");
	if($mvconf != 0) {
		die "INstalling modified configuration file <<$condorchanges>> failed\n";
	}
}

# add quillreader and quillwriter
print "Create quillreader\n";
$command = Expect->spawn("$prefix/bin/createuser quillreader --port $startpostmasterport --no-createdb --no-adduser --pwprompt")||
	die "Could not start program: $!\n";
$command->log_stdout(0);
unless($command->expect(10,"Enter password for new role:")) {
	die "Quillreader Password request never happened\n";
}
print $command "biqN9ihm\n";

unless($command->expect(10,"Enter it again:")) {
	die "Password confirmation never happened\n";
}
print $command "biqN9ihm\n";

unless($command->expect(10,"Shall the new role be allowed to create more new roles? (y/n)")) {
	die "Password request never happened\n";
}
print $command "n\n";
$command->soft_close();

print "Create quillwriter\n";
$command = Expect->spawn("$prefix/bin/createuser quillwriter --port $startpostmasterport --createdb --no-adduser --pwprompt")||
	die "Could not start program: $!\n";
$command->log_stdout(0);
unless($command->expect(10,"Enter password for new role:")) {
	die "Quillwriter Password request never happened\n";
}
print $command "biqN9ihm\n";

unless($command->expect(10,"Enter it again:")) {
	die "Password confirmation never happened\n";
}
print $command "biqN9ihm\n";

unless($command->expect(10,"Shall the new role be allowed to create more new roles? (y/n)")) {
	die "Password request never happened\n";
}
print $command "n\n";
$command->soft_close();

#system("$prefix/bin/createuser quillwriter --createdb --no-adduser --pwprompt");

$docreatedb = system("$prefix/bin/createdb --port $startpostmasterport test");
if($docreatedb != 0) {
	die "Failed to create test db\n";
}

#system("$prefix/bin/psql test");


exit(0);
