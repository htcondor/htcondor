#! /usr/bin/env perl

my $input = $ARGV[0];

#print "Want to read $input\n";

if( ! -e $input )
{
	die "can not read file $input\n";
}

my $stat = sysopen( INPUT, $input, O_RDONLY);

if(!$stat)
{
	die "Could not open $input\n";
}
else
{
	#print "file open now.....\n";
}

my $buffer;
my $count = 0;
my $readcnt = 1;
my $total;

while( $readcnt != 0 )
{
	$readcnt = sysread( INPUT, $buffer, 256 );
	$total = $total + $readcnt;
	#print "Tried to read 256 and actually read $readcnt($total)\n";
}

#print "file is $total bytes large\n";
exit(0);
