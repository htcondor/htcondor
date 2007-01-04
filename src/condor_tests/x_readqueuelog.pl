#! /usr/bin/env perl
use CondorTest;

my $log = $ARGV[0];
my $option = $ARGV[1];
#my $limit = 2;
my $limit = $ARGV[2];
my $count = 0;
my $line;

#print "Log is $log, Option is $option and limit is $limit\n";

open(OLDOUT, "<$log");
while(<OLDOUT>)
{
	CondorTest::fullchomp($_);
	$line = $_;
	#print "--$line--\n";
	if( $line =~ /^\s*(hello)\s*$/ )
	{
		$count++;
		#print "Count now $count\n";
	}
}

close(OLDOUT);

if($count != $limit)
{
	print "$count";
}
else
{
	print "0\n";
}

exit(0);
