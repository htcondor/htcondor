#! /usr/bin/env perl

my $input = $ARGV[0];
my $newfile = $ARGV[1];

print "Want to read $input and write $newfile\n";

if( ! -e $input )
{
	die "can not read file $input\n";
}

my $outstat = open( TESTOUT, ">$newfile")|| die "Failed open of $newfile : $!\n";
my $stat = open( INPUT, "<$input")|| die "Failed open of $input : $!\n";

binmode(TESTOUT);
binmode(INPUT);

if(!$stat)
{
	die "Could not open $input\n";
}
else
{
	print "file open now.....\n";
}

if(!$outstat)
{
	die "Could not open $newfile\n";
}
else
{
	print "file open now.....\n";
}

my $buffer;
my $count = 0;
my $readcnt = 1;
my $writecnt = 0;
my $total;

while( $readcnt != 0 )
{
	$readcnt = read( INPUT, $buffer, 256 );
	print TESTOUT "$buffer";
	#$writecnt = write( TESTOUT, $buffer, $readcnt )|| die "bad write(cnt=$writecnt): $!\n"; 
	$total = $total + $readcnt;
	#print "Tried to read 256 and actually read $readcnt($total)\n";
}

print "file is $total bytes large\n";
