#! /usr/bin/env perl

my $request = $ARGV[0];
#print "Welcome to factorial tutorial! -- $request --\n";

my $start = time();
my $lapsed = 0;
my $now = 0;

my $res = 0;

while( $lapsed <= $request )
{
	$res = factorial(50000);
	$now = time();
	$lapsed = $now - $start;
}


#print "6 factorial = $res\n";

sub factorial
{
	my $arg = shift @_;

	#print "factorial called with $arg\n";
	if( $arg == 0 ) 
	{
		#print "arg 0 return 1\n";
		return(1);
	}
	if( $arg == 1 ) 
	{
		#print "arg 1 return 1\n";
		return(1);
	}
	else
	{
		return(factorial($arg - 1) * $arg);
	}
}
