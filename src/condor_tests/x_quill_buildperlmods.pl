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

use Cwd;
use CondorTest;

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

$topdir = getcwd();
$installdir = $topdir . "/perllib";
$ENV{PERL5LIB} = $installdir . "/lib/site_perl" . ":" . $installdir . "/lib/perl5/site_perl" . ":" . $installdir . "/lib64/perl5/site_perl" . ":" . $installdir . "/lib64/site_perl" . ":" . $installdir . "/Library/Perl";
CondorTest::debug("Adjusted Perl Library search to $ENV{PERL5LIB}\n",1);

@modules = ( "IO-Tty-1.07", "Expect-1.20" );

foreach $module (@modules) {
	CondorTest::debug("Building $module now\n",1);
	my $tarfile = "x_quill_" . $module . ".tar.gz";
	CondorTest::debug("Extracting  $tarfile\n",1);
	$gettar = system("tar -zxvf $tarfile");
	if($gettar != 0) {
		die "Could not extract $module\n";
	}
	CondorTest::debug("Module is <<$module>>\n",1);
	chdir "$module";
	system("pwd");
	$mkmake = system("perl Makefile.PL PREFIX=$installdir");
	if($mkmake != 0) {
		die "Could not make makefile for $module\n";
	}
	$domake = system("make");
	if($domake != 0) {
		die "Could not make $module\n";
	}
	$dotest = system("make test");
	if($dotest != 0) {
		die "Could not test $module\n";
	}
	$doinstall = system("make install");
	if($doinstall != 0) {
		die "Could not install $module\n";
	}
	chdir "$topdir" ;
}

exit(0);
