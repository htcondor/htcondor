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


my $changethis = $ARGV[0];
my $replacewith = $ARGV[1];
my $fileselection = $ARGV[2];

print "replace <$changethis> with <$replacewith> in these files <$fileselection>\n";

#my $dbtimelog = "x_strip.log";
#print "Trying to open logfile... $dbtimelog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">>$dbtimelog") or die "Could not open $dbtimelog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
#select(STDOUT);
 ##$| = 1;

open(TESTS,"ls $fileselection | ");
while(<TESTS>)
{
	print "$_";
	chomp();
	$file = $_;
	#print "Currently scanning $file for name $strippattern\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	print "$file opened OK!\n";
	while(<FILE>)	
	{		
		chomp();
		$line = $_;
		if($line =~ /^(.*)$changethis(.*)$/) {
			# do nothing
			$newline = $1 . $replacewith . $2;
			#print " in $file found $line\n";
			#print "change to: $newline\n";
			print NEW "$newline\n";
		} elsif($line =~ /^;(.*)$/) {
			print NEW "$1\n";
		} else {
			print NEW "$line\n";
		}
	}	
	close(FILE);
	close(NEW);
	system("mv $file.tmp $file");
}
close(TESTS);

exit;
