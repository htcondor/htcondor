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

use strict;
use warnings;


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

############################################################################

my $startdir = $ARGV[0];
searchdown($startdir);
print "Done.... doing $startdir and down\n";
exit(0);

sub searchdown
{
	my $targetdir = shift;
	print "Directory to search is $targetdir\n";
	opendir DH, $targetdir or die "Can not open $targetdir:$!\n";
	foreach my $file (readdir DH)
	{	
		next if $file =~ /^\.\.?$/;
		next if $file =~ /^.*code.*$/;
		my $line;
		#print "Currently scanning $targetdir/$file for Botany\n";
		if(-f "$targetdir/$file" ) {
			print "search file $targetdir/$file\n";
			my $tg = "$targetdir/$file";
			my $tgnew = "$targetdir/$file.new";
			my $foundone = 0;
			open(TG,"<$tg") or die "Open err on $targetdir/$file:$!\n";
			open(TGNEW,">$tgnew") or die "Open err on $targetdir/$file.new:$!\n";
			while(<TG>) {
				chomp();
				$line = $_;
				if($line =~ /^(.*)Botany(.*)$/) {
					print TGNEW "$1Soar$2\n";
					$foundone = 1;
				} else {
					print TGNEW "$line\n";
				}
			}
			close(TG);
			close(TGNEW);
			if($foundone == 1) {
				print "Remove .new file\n";
				system("mv $tg $tg.old");
				system("mv $tgnew $tg");
				system("chmod 775 $tg");
				system("rm $tg.old");
			} else {
				system("rm $tgnew");
			}
		} else {
			if(-d "$targetdir/$file") {
				#print "$file is a directory\n";
				searchdown("$targetdir/$file");
			} else {
				die "$targetdir/$file is unknown\n";
			}
		}
	}
}
