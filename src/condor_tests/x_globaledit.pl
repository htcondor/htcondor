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


#use Condor;

#Condor::DebugOn();
#Condor::DebugLevel(1);
#Condor::debug("Basic Test\n",1);
#Condor::debug("Basic Test level 2\n",2);
#exit(0);

#./process file lo hi
#my $file = $ARGV[0];
#my $lo = $ARGV[1];
#my $hi = $ARGV[2];

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

############################################################################

#$newfile = $file . ".new";
#open(TAR,"<$file") or die "Can't open $file:$!\n";
#open(HERE,">$newfile") or die "Can't open $newfile:$!\n";
#my $line = "";
#while(<TAR>) {
	#chomp();
	#$line = $_;
	#if($line =~ /^(\s*)#(\s*debug\s*\(.*)\)\s*;.*$/){
		#print HERE "$1 $2,$hi);\n";
	#} elsif($line =~ /^(\s*\s*debug\s*\(.*)\)\s*;.*$/){
		#print HERE "$1,$lo);\n";
	#} else {
		#print HERE "$line\n";
	#}
#}
#close(TAR);
#close(HERE);



############################################################################

foreach $file (@ARGV)
{	
	$foundcondortest = 0;
	my $line = "";
	next if $file =~ /^\.\.?$/;
	print "Currently scanning $file for system date usage\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	print "$file opened OK!\n";
	while(<FILE>)	
	{		
		chomp();
		$line = $_;
		if($line =~ /^(\s*)system\(\"date\"\)(.*)$/) {
			# do nothing if going to a file
			#print "Found <$1system(\"date\")$2>\n";
			print NEW "$1print scalar localtime() . \"\\n\";\n";
		} else {
			print NEW "$line\n";
		};
	}	
	close(FILE);
	close(NEW);
	system("cp $file.tmp $file");
	system("chmod 755 $file");
	system("rm -f $file.tmp");
}
exit(0);

foreach $file (@ARGV)
{	
	$foundcondortest = 0;
	my $line = "";
	next if $file =~ /^\.\.?$/;
	next if $file eq $dbtimelog;
	next if $file eq "x_striplines.pl";
	next if $file eq "x_globaledit.pl";
	print "Currently scanning $file for CondorTest and prints\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	print "$file opened OK!\n";
	while(<FILE>)	
	{		
		chomp();
		$line = $_;
		if($line =~ /^\s*print\s+\w+\s+\".*\".*$/) {
			# do nothing if going to a file
			print NEW "$line\n";
		} elsif($line =~ /^\s*use\s+CondorTest.*$/) {
			print "File uses CondorTest!\n";
			$foundcondortest = 1;
			print NEW "$line\n";
		} elsif($line =~ /^(\s*)print\s+.*(\".*\").*$/) {
			#print NEW "$_";
			if($foundcondortest == 1) {
				print NEW "$1CondorTest::debug($2,1);\n";
			} else {
				print NEW "$line\n";
			}
		} elsif($line =~ /.*::DebugOff\(\).*$/) {
			# do nothing ie strip out
			print "strip in $file found $line\n"
		} else {
			print NEW "$line\n";
		}
	}	
	close(FILE);
	close(NEW);
	system("cp $file.tmp $file");
	system("chmod 755 $file");
}
############################################################################

my $targetdir = '.';
print "Directory to change is $targetdir\n";
opendir DH, $targetdir or die "Can not open $dir:$!\n";
foreach $file (readdir DH)
{	
	my $line = "";
	next if $file =~ /^\.\.?$/;
	next if $file eq $dbtimelog;
	next if $file eq "x_striplines.pl";
	next if $file eq "x_globaledit.pl";
	print "Currently scanning $file for name DebugOff()\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	#print "$file opened OK!\n";
	while(<FILE>)	
	{		
		$line = $_;
		if($line =~ /^.*print.*$/) {
			print NEW "$_";
		} elsif($line =~ /.*::DebugOff\(\).*$/) {
			# do nothing ie strip out
			print "strip in $file found $line\n"
		} else {
			print NEW "$_";
		}
	}	
	close(FILE);
	close(NEW);
	system("mv $file.tmp $file");
}
