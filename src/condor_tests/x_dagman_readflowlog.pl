#! /usr/bin/env perl

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
	chomp;
	$line = $_;
	#print "--$line--\n";
	if( $line =~ /^\s*(open)\s*$/ )
	{
		$count++;
		#print "Count now $count\n";
		if($count > $limit)
		{
			#print "$count exceeds $limit\n";
			print	"$count";
			exit($count);
		}
	}
	if( $line =~ /^\s*(close)\s*$/ )
	{
		$count--;
		#print "Count now $count\n";
	}
}

close(OLDOUT);
print "$count";
exit(0);
