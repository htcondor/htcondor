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
use CondorTest;


GetOptions (
		'help' => \$help,
		'oldfile=s' => \$oldfile,
		'newfile=s' => \$newfile,
);

if ( $help )    { help() and exit(0); }

if(!$oldfile)
{
	die "oldfile=\"somefile\" must be passed in!\n";
}

if(!$newfile)
{
	die "newfile=\"somefile\" must be passed in!\n";
}

my $oldmd5file = $oldfile . "md5";
my $oldmd5;
open(ORIGMD5,"<$oldmd5file") || die "Can't open md5 checksum file $!\n";
while(<ORIGMD5>)
{
	CondorTest::fullchomp($_);
	$oldmd5 = $_;
}
close(ORIGMD5);
CondorTest::debug("Old MD5 = $oldmd5\n",1);

my $newmd5file = $newfile . "md5";
my $datamd5 = Digest::MD5->new;

open(MD5,">$newmd5file") || die "Can't open MD5 output file $!\n";
open(DATA,"<$newfile") || die "Trying to open data file\n" ;
$datamd5->addfile(DATA);
close(DATA);


my $hexmd5 = $datamd5->hexdigest;

print MD5 "$hexmd5\n";
close(MD5);

if($oldmd5 eq $hexmd5)
{
	CondorTest::debug("Check some of $oldfile and $newfile match!\n",1);
	#verbose_system("touch WORKED");
	unlink($oldfile);
	unlink($newfile);
	exit(0);
}
else
{
	CondorTest::debug("failed\n",1);
	#verbose_system("touch FAILED");
	exit(1);
}

# =================================
# print help
# =================================

sub help 
{
    print "Usage: writefile.pl --megs=#
Options:
	[-h/--help]				See this
	[-o/--oldfile]			file which had filemd5 created
	[-n/--newfile]			file which will have filemd5 created
	\n";
}

sub verbose_system 
{

my @args = @_;
my $rc = 0xffff & system @args;

printf "system(%s) returned %#04x: ", @args, $rc;

	if ($rc == 0) 
	{
		CondorTest::debug("ran with normal exit\n",1);
		return $rc;
	}
	elsif ($rc == 0xff00) 
	{
		CondorTest::debug("command failed: $!\n",1);
		return $rc;
	}
	elsif (($rc & 0xff) == 0) 
	{
		$rc >>= 8;
		CondorTest::debug("ran with non-zero exit status $rc\n",1);
		return $rc;
	}
	else 
	{
		CondorTest::debug("ran with ",1);
		if ($rc &   0x80) 
		{
			$rc &= ~0x80;
			CondorTest::debug("coredump from ",1);
			return $rc;
		}
		CondorTest::debug("signal $rc\n",1);
	}
}

