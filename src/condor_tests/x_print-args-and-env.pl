#! /usr/bin/env perl


my $argcnt = $#ARGV;
my $thisarg = 0;
my $actualcount = ($argcnt + 1);
print "There are $actualcount args to process\n";
my $arg;


while($thisarg <= $argcnt)
{
	$arg = $ARGV[$thisarg++];
	print "$arg\n";
}

foreach my $EnvVar ( sort keys % ENV )
{
	print  "$EnvVar = $ENV{$EnvVar}\n";
}


