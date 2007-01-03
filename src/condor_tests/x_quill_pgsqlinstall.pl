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
my $mypostgresqlconf = "../postgres-data/postgresql.conf";
my $mynewpostgresqlconf = "../postgres-data/postgresql.new";
my $myquillchanges = "../../x_postgress_quill.conf";

my $condorchanges = "../../$condorconfigadd";
my $newcondorchanges = "../../$condorconfigadd" . ".new";

my $mypgfile = "../postgres-data/pg_hba.conf";
my $mynewpgfile = "../postgres-data/pg_hba.new";


chdir("$prefix");
system("tar -zxvf ../$tarfile");
chdir("$pgsqlversion");
system("./configure --prefix=$prefix");
system("make");
system("make install");
system("mkdir $prefix/postgres-data");

print "Create db\n";
#system("$prefix/bin/initdb -D $prefix/postgres-data --auth=password --pwfile=../../x_job_quill_supw") ;
system("$prefix/bin/initdb -D $prefix/postgres-data") ;

# customize postgress before starting it

system("pwd;ls;pwd");

open(OLD,"<$mypostgresqlconf") || die "Can not open original conf file: $mypostgresqlconf($!)\n";
open(NEW,">$mynewpostgresqlconf") || die "Can not open original conf file: $mynewpostgresqlconf($!)\n";
# get original configuration
while(<OLD>) {
	print NEW "$_";
}
open(EXTRA,"<$myquillchanges") || die "Can not open quill conf file: $myquillchanges($!)\n";
# add quill changes
while(<EXTRA>) {
	print NEW "$_";
}
print  NEW "port = 5432\n";
close(NEW);
close(OLD);
close(EXTRA);

system("rm -f $mypostgresqlconf");
system("cp $mynewpostgresqlconf $mypostgresqlconf");

open(OLD,"<$mypgfile") || die "Can not open original conf file: $mypgfile($!)\n";
open(NEW,">$mynewpgfile") || die "Can not open original conf file: $mynewpgfile($!)\n";
# get original configuration
while(<OLD>) {
	print NEW "$_";
}
close(OLD);
print NEW "host all quillreader 0.0.0.0 0.0.0.0 password\n";
print NEW "host all quillwriter 0.0.0.0 0.0.0.0 password\n";
close(NEW);

system("rm -f $mypgfile");
system("cp $mynewpgfile $mypgfile");

my $dbup = "no";
my $trylimit = 1;
# start db

while (( $dbup ne "yes") && ($trylimit <= 10) ) {
	$pmaster_start = system("$prefix/bin/pg_ctl -D $prefix/postgres-data -l $prefix/postgres-data/logfile start");
	print "starting postmaster result<<<<<$pmaster_start>>>>>\n";
	sleep(10);
	if(!(-f "$prefix/postgres-data/postmaster.pid")) {
		$trylimit = $trylimit + 1;
		open(OLD,"<$mypostgresqlconf") || die "Can not open original conf file: $mypostgresqlconf($!)\n";
		open(NEW,">$mynewpostgresqlconf") || die "Can not open original conf file: $mynewpostgresqlconf($!)\n";
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
		system("cp $mynewpostgresqlconf $mypostgresqlconf");
	} else {
		$dbup = "yes";
	}
}

if($trylimit > 10) {
	exit(0);
} else {
	open(OLD,"<$condorchanges") || die "Can not open original conf file: $mypostgresqlconf($!)\n";
	open(NEW,">$newcondorchanges") || die "Can not open original conf file: $mynewpostgresqlconf($!)\n";
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
	system("cp $newcondorchanges $condorchanges");
}

# add quillreader and quillwriter
print "Create quillreader\n";
$command = Expect->spawn("$prefix/bin/createuser quillreader --port $startpostmasterport --no-createdb --no-adduser --pwprompt")||
	die "Could not start program: $!\n";
$command->log_stdout(0);
unless($command->expect(10,"Enter password for new role:")) {
	die "Password request never happened\n";
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
	die "Password request never happened\n";
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

system("$prefix/bin/createdb --port $startpostmasterport test");

#system("$prefix/bin/psql test");


exit($startpostmasterport);
