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


my $dbtimelog = "x_testname.log";
print "Trying to open logfile... $dbtimelog\n";
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$dbtimelog") or die "Could not open $dbtimelog: $!";
open(STDERR, ">&STDOUT");
select(STDERR);
 $| = 1;
select(STDOUT);
 $| = 1;

foreach $file (@ARGV) {
	#print "search $file\n";

	my $line = "";
	#print "Currently scanning $file for name $strippattern\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	#print "$file opened OK!\n";
	print "testscript is <<<$file>>>\n";
	@test = split /\./, $file;
	print "Testname is really $test[0]\n";
	while(<FILE>)	
	{		
		$line = $_;
		if($line =~ /^(.*)\$testname\s*=(.*)\s*$/) {
			# do nothing
			print " in $file found $2\n";
			print NEW "$1\$testdesc = $2\n";
			print NEW "$1\$testdesc =  \"$test[0]\";\n";
			print NEW "$1\$testname = "x_chngtestname";
		} else {
			print NEW "$_";
		}
	}	
	close(FILE);
	close(NEW);
	system("mv $file.tmp $file");
	system("chmod 755 $file");
}

exit(0);
