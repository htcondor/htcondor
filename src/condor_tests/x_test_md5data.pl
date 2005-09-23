#!/usr/bin/env perl
use File::Copy;
use File::Path;
use Getopt::Long;
use Digest::MD5;


GetOptions (
		'help' => \$help,
		'oldfile=s' => \$oldfile,
		'newfile=s' => \$newfile,
);

if ( $help )    { help() and exit(0); }

if(!$oldfile)
{
}

if(!$newfile)
{
}

my $oldmd5file = $oldfile . "md5";
my $oldmd5;
open(ORIGMD5,"<$oldmd5file") || die "Can't open md5 checksum file $!\n";
while(<ORIGMD5>)
{
	chomp($_);
	$oldmd5 = $_;
}
close(ORIGMD5);
print "Old MD5 = $oldmd5\n";

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
	print "worked\n";
	#verbose_system("touch WORKED");
	exit(0);
}
else
{
	print "failed\n";
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

