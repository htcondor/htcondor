#!/usr/bin/env perl
use File::Copy;
use File::Path;
use Getopt::Long;
use Digest::MD5;

open(DATA,">data") || die "Can't open output file $!\n";

GetOptions (
		'help' => \$help,
		'megs=i' => \$megs,
		'filenm=s' => \$filenm,
);

my $rows = $megs;
my $rowsz = 1048576;
my $seed_char = int(rand (ord("~") - ord(" ") + 1)) + ord(" ");
my $start_char = chr($seed_char);

print "Start char -$start_char-\mn";

if ( $help )    { help() and exit(0); }

die "Need megabytes defined\n" unless defined $megs;

if( !$filenm )
{
	die "you must use --filenm=filename for this test to produce output!\n";
}


my $rowchar = $seed_char++;
my $row = "";


if($seed_char == ord("~"))
{
	$seed_char = ord(" ");
}

foreach (1..$rowsz)
{
	$row .= chr($rowchar++);
	if($rowchar == ord("~"))
	{	
		$rowchar = ord(" ");
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

