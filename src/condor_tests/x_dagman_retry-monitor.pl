#!/usr/bin/perl

use File::Copy;
use Cwd;


#my $LogFile = "dag_ret.log";

#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR); $| = 1;
#select(STDOUT); $| = 1;

my $stat = 0;
my $returnval = shift;
print "Node status passed in is $returnval\n";
if($returnval != 0)
{
	exit($returnval)
}
else
{
	exit(0);
}
