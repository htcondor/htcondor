#! /usr/bin/env perl
use CondorTest;

my $log = $ARGV[0];
my $option = $ARGV[1];
#my $limit = 2;
my $pre = $ARGV[2];
my $post = $ARGV[3];
my $precount = 0;
my $postcount = 0;
my $line;

#print "Log is $log, Option is $option and  Pre is $pre and Post is $post\n";

open(OLDOUT, "<$log");
while(<OLDOUT>)
{
	CondorTest::fullchomp($_);
	$line =  $_;
	#print "--$line--\n";
	if( $line =~ /^\s*(prescript_ran)\s*$/ )
	{
		$precount++;
		#print "Prescript = $precount\n";
	}
	if( $line =~ /^\s*(postscript_ran)\s*$/ )
	{
		$postcount++;
		#print "Postscript = $postcount\n";
	}
}

close(OLDOUT);

if( $pre != $precount )
{
	print "$precount";
	exit(1)
}

if( $post != $postcount )
{
	print "$postcount";
	exit(1)
}

print "0";
exit(0);
