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

my $file_target = $ARGV[0];
my $new_name = $ARGV[1];

#my $dbtimelog = "x_change.log";
#print "Trying to open logfile... $dbtimelog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">>$dbtimelog") or die "Could not open $dbtimelog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
#select(STDOUT);
 #$| = 1;

my $targetdir = '.';
CondorTest::debug("Directory to change is $targetdir\n",1);
opendir DH, $targetdir or die "Can not open $dir:$!\n";
foreach $file (readdir DH)
{	
	my $line = "";
	next if $file =~ /^\.\.?$/;
	next if $file eq $dbtimelog;
	next if $file eq "x_renametool.pl";
	#print "Currently scanning $file for name $file_target being renamed $new_name\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	#print "$file opened OK!\n";
	while(<FILE>)	
	{		
		CondorTest::fullchomp($_);
		$line = $_;
		s/$file_target/$new_name/g;
		print NEW "$_\n";
	}	
	close(FILE);
	close(NEW);
	system("mv $file.tmp $file");
}
closedir DH;
system("mv $file_target $new_name");
	
