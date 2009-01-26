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

use File::Copy;
use File::Path;
use Getopt::Long;
use Digest::MD5;


GetOptions (
		'help' => \$help,
		'megs=i' => \$megs,
		'filenm=s' => \$filenm,
);

open(DATA,">$filenm") || die "Can't open output file $!\n";

my $rows = $megs;
my $rowsz = 1048576;
my $seed_char = int(rand ((ord("~") - ord(" ")) + 1)) + ord(" ");
my $start_char = chr($seed_char);

print "Start char -$start_char-\n";

if ( $help )    { help() and exit(0); }

die "Need megabytes defined\n" unless defined $megs;

if( !$filenm )
{
	die "you must use --filenm=filename for this test to produce output!\n";
}


my $rowchar = $seed_char;
my $row = "";


foreach (1..$rowsz)
{
	$row .= chr($rowchar);
	if($rowchar == ord("~"))
	{	
		$rowchar = ord(" ");
	} else {
		$rowchar++;
	}
}

foreach (1..$rows)
{
	print DATA "$row";
}

close(DATA);

my $outnamemd5 = $filenm . "md5";

open(DATA,"<$filenm") || die "Can't open input file $!\n";
open(MD5,">$outnamemd5") || die "Can't open MD5 output file $!\n";
my $datamd5 = Digest::MD5->new;

$datamd5->addfile(DATA);
close(DATA);


my $hexmd5 = $datamd5->hexdigest;

print MD5 "$hexmd5\n";
close(MD5);



# =================================
# print help
# =================================

sub help 
{
    print "Usage: writefile.pl --megs=#
Options:
	[-h/--help]				See this
	[-m/--megs]				Number of megs to make file
	[-f/--filenm]			output file base name. If fed data produces data and datamd5.
	\n";
}

sub verbose_system 
{

my @args = @_;
my $rc = 0xffff & system @args;

printf "system(%s) returned %#04x: ", @args, $rc;

	if ($rc == 0) 
	{
		print "ran with normal exit\n";
		return $rc;
	}
	elsif ($rc == 0xff00) 
	{
		print "command failed: $!\n";
		return $rc;
	}
	elsif (($rc & 0xff) == 0) 
	{
		$rc >>= 8;
		print "ran with non-zero exit status $rc\n";
		return $rc;
	}
	else 
	{
		print "ran with ";
		if ($rc &   0x80) 
		{
			$rc &= ~0x80;
			print "coredump from ";
			return $rc;
		}
		print "signal $rc\n"
	}
}

