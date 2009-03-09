#!/usr/bin/env perl
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
use Expect;
use Socket;
use Sys::Hostname;

my $prefix = $ARGV[0];
my $installlog = $ARGV[1];
my $pgsql_dir = $ARGV[2];
my $condorconfigadd = $ARGV[3];
my $morelibs = $ARGV[4];
my $testsrc = $ARGV[5];

CondorTest::debug("$prefix $installlog $pgsql_dir $condorconfigadd $morelibs $testsrc \n",1);

my $startpostmasterport = 5432;
my $postmasterinc = int(rand(100) + 13);
$currenthost = CondorTest::getFqdnHost();
$domain = `dnsdomainname`;
CondorTest::fullchomp($currenthost);
CondorTest::fullchomp($domain);

# for quill this must be the fully qualified name which
# is what quill uses to builds its search string for the
# db writing password.

@domainparts = () ;
#if($currenthost =~ /^(.*)\.cs\.wisc\.edu$/) {
if($currenthost =~ /^(.*)/) {
	# did we get a fully qualified name.....
	@domainparts = split /\./, $currenthost;
	if($#domainparts == 0) {
		$currenthost = $currenthost . "." . $domain;
	}
	CondorTest::debug("Fully qualified name already<<$currenthost>>\n",1);
} else {
	# fine add our domain for madison
	#$currenthost = $currenthost . ".cs.wisc.edu";
	$currenthost = $currenthost;
	die "Why Here?????<<$currenthost/$domain>>\n";
}

CondorTest::debug("Original port request is <<$startpostmasterport>>\n",1);
CondorTest::debug("Random increment if needed will be <<$postmasterinc>>\n",1);

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

my $condorchanges = "$condorconfigadd" . ".template";
my $newcondorchanges = "$condorconfigadd";

my $mypgfile = $prefix . "/postgres-data/pg_hba.conf";
my $mynewpgfile = $prefix . "/postgres-data/pg_hba.new";
my $pgpass = $prefix . "/../pgpass";

CondorTest::debug("Saving PGPASS date to <<$pgpass>>\n",1);

chdir("$prefix");

system("mkdir $prefix/postgres-data");

$ENV{"LD_LIBRARY_PATH"} = $pgsql_dir . "/lib";

CondorTest::debug("Create db\n",1);
$initdb = system("$pgsql_dir/bin/initdb -D $prefix/postgres-data") ;
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
print NEW "host test all 0.0.0.0 0.0.0.0 md5\n";
close(NEW);

system("rm -f $mypgfile");
$mmvconf = system("cp $mynewpgfile $mypgfile");
if($mvconf != 0) {
	die "INstalling modified configuration file <<$mypgfile>> failed\n";
}

my $dbup = "no";
my $trylimit = 1;
# start db

CondorTest::debug("Setting PGDATA to $prefix/postgres-data\n",1);
$ENV{PGDATA} = "$prefix/postgres-data";

while (( $dbup ne "yes") && ($trylimit <= 10) ) {
	$pmaster_start = system("$pgsql_dir/bin/pg_ctl -D $prefix/postgres-data -l $prefix/postgres-data/logfile start");
	CondorTest::debug("starting postmaster result<<<<<$pmaster_start>>>>>\n",1);
	print scalar localtime() . "\n";
	sleep(10);
	if(!(-f "$prefix/postgres-data/postmaster.pid")) {
		$trylimit = $trylimit + 1;
		open(OLD,"<$mypostgresqlconf") || die "6 Can not open original conf file: $mypostgresqlconf($!)\n";
		open(NEW,">$mynewpostgresqlconf") || die "7 Can not open original conf file: $mynewpostgresqlconf($!)\n";
		while(<OLD>) {
			if( $_ =~ /^port\s+=\s+/ ){
				$startpostmasterport = $startpostmasterport + $postmasterinc;
				print NEW "port = $startpostmasterport\n";
				CondorTest::debug("Trying port $startpostmasterport($postmasterinc)\n",1);
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
		CondorTest::debug("Looking for listen on port....\n",1);
		my @statports = `netstat -a`;
		foreach $netstatport (@statports) {
			chomp($netstatport);
			if( $netstatport =~ /^.*:$startpostmasterport.*LISTEN.*$/) {
				CondorTest::debug("Yup its listening: <<<$netstatport>>>\n",1);
				$dbup = "yes";
				last;
			} elsif( $netstatport =~ /^.*\.postgresql.*LISTEN.*$/) {
				CondorTest::debug("Yup its listening: <<<$netstatport>>>\n",1);
				$dbup = "yes";
				last;
			} elsif($netstatport =~ /^.*LISTEN.*$startpostmasterport.*$/) {
				CondorTest::debug("Yup its listening: <<<$netstatport>>>\n",1);
				$dbup = "yes";
				last;
			} else {
				CondorTest::debug("skip: $netstatport\n",1);
			}
		}
		if($dbup eq "no") {
				CondorTest::debug("They lied.... nothing on port (($startpostmasterport))\n",1);
		}
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
			CondorTest::debug("Trying port $startpostmasterport\n",1);
		} else {
			print NEW ;
		}
	}
	close(OLD);
	close(NEW);
}


print scalar localtime() . "\n";
CondorTest::debug("Configuring Postgres for Quil\n",1);

# add quillreader and quillwriter
CondorTest::debug("Create quillreader\n",1);

#$Expect::Log_Stdout=1;
#$Expect::Debug=3;
#$Expect::Exp_Internal=1;

$command = Expect->spawn("$pgsql_dir/bin/createuser quillreader --port $startpostmasterport --no-createdb --no-adduser --no-createrole --pwprompt")||
	die "Could not start program: $!\n";
#$command->log_stdout(1);
unless($command->expect(10,"Enter password for new role: ")) {
	die "Quillreader Password request never happened\n";
}
print $command "condor4me#\n";

unless($command->expect(10,"Enter it again: ")) {
	die "Password confirmation never happened\n";
}
print $command "condor4me#\n";

#unless($command->expect(10,"Shall the new role be allowed to create more new roles? (y/n)")) {
	#die "Password request never happened\n";
#}
print $command "n\n";
$command->soft_close();

CondorTest::debug("Create quillwriter\n",1);
$command = Expect->spawn("$pgsql_dir/bin/createuser quillwriter --port $startpostmasterport --createdb --no-adduser --no-createrole --pwprompt")||
	die "Could not start program: $!\n";
$command->log_stdout(0);
unless($command->expect(10,"Enter password for new role: ")) {
	die "Quillwriter Password request never happened\n";
}
print $command "condor4me#\n";

unless($command->expect(10,"Enter it again: ")) {
	die "Password confirmation never happened\n";
}
print $command "condor4me#\n";

#unless($command->expect(10,"Shall the new role be allowed to create more new roles? (y/n)")) {
	#die "Password request never happened\n";
#}
print $command "n\n";
$command->soft_close();

#save PGPASS date for Condor to grab......
open(PG,">$pgpass") || die "Could not open file<$pgpass> for .pgpass data!:$!\n";
print PG "$currenthost:$startpostmasterport:test:quillwriter:condor4me#\n";
close(PG);

print scalar localtime() . "\n";
CondorTest::debug("Create DB for Quil\n",1);

$docreatedb = system("$pgsql_dir/bin/createdb --port $startpostmasterport test");
if($docreatedb != 0) {
	die "Failed to create test db\n";
}
print scalar localtime() . "\n";

$docreatelang = system("$pgsql_dir/bin/createlang plpgsql test --port $startpostmasterport");
#$docreatelang = system("$pgsql_dir/bin/createlang plpgsql test");
if($docreatelang != 0) {
	print "$pgsql_dir/bin/createlang plpgsql FAILED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
	die "Failed to createlang plpgsql\n";
} else {
	print "$pgsql_dir/bin/createlang plpgsql Worked!!\n";
}

print scalar localtime() . "\n";


CondorTest::debug("Run psql program\n",1);
$command = Expect->spawn("$pgsql_dir/bin/psql test quillwriter --port $startpostmasterport")||
	die "Could not start program: $!\n";
#$command->debug(1);
$command->log_stdout(0);
unless($command->expect(10,"test=>")) {
	die "Expected psql prompt for test db\n";
}
print $command "\\i $testsrc/../condor_tt/common_createddl.sql\n";

unless($command->expect(180,"test=>")) {
	die "Prompt after attempted install of common schema failed\n";
}
print $command "\\i $testsrc/../condor_tt/pgsql_createddl.sql\n";

unless($command->expect(180,"test=>")) {
	die "Prompt after attempted install of postgress schema failed\n";
}
print $command "\quit\n";
$command->soft_close();

CondorTest::debug("Postgress built and set up: ",1);
print scalar localtime() . "\n";


exit(0);
