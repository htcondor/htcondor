#!/usr/bin/env perl
use File::Copy;
use File::Path;
use Getopt::Long;
#use Digest::MD5;

my $outputfile = $ARGV[0];


GetOptions (
		'help' => \$help,
		'megs=i' => \$megs,
);


verbose_system("pwd;ls");
verbose_system("mv data $outputfile");
verbose_system("touch $outputfile");
verbose_system("pwd;ls");

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

