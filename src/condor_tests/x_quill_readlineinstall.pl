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
my $tarfile = $ARGV[2];
my $rdlnversion = $ARGV[3];

CondorTest::debug("Prefix=<<$prefix>> installlog=<<$installlog>> tarfile=<<$tarfile>> rdlnversion=<<$rdlnversion>>\n",1);

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
CondorTest::debug("Moving to this dir <<$prefix>>\n",1);

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
	CondorTest::debug("<<$tarfile>> does not exist here :-(\n",1);
}

exit;
