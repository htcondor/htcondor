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

open(DATA,">job_chirp_io.txtdata") || die "Can't open output file $!\n";

GetOptions (
		'help' => \$help,
		'message=s' => \$message,
);

my $rows = 4;
my $rowsz = 1024;
my $start_char = "a";

if ( $help )    { help() and exit(0); }

die "Need message defined\n" unless defined $message;

my $rowchar = $seed_char++;
my $row = "";


fill_row("a");
add_row();
$row = "";

fill_row("b");
add_row();
$row = "";

fill_row("c");
add_row();
$row = "";

fill_row("d");
add_row();
$row = "";

print DATA "$message";

close(DATA);

open(DATA,"<job_chirp_io.txtdata") || die "Can't open input file $!\n";
open(MD5,">job_chirp_io.txtdatamd5") || die "Can't open MD5 output file $!\n";
my $datamd5 = Digest::MD5->new;

$datamd5->addfile(DATA);
close(DATA);


my $hexmd5 = $datamd5->hexdigest;

print MD5 "$hexmd5\n";
close(MD5);

sub fill_row 
{
	my $char = shift @_;
	foreach (1..$rowsz)
	{
		$row .=  $char;
	}
}

sub add_row
{
	print DATA "$row";
}

# =================================
# print help
# =================================

sub help 
{
    print "Usage: writefile.pl --megs=#
Options:
	[-h/--help]				See this
	[-m/--megs]				Number of megs to make file
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

