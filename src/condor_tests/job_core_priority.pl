#! /usr/bin/env perl


my $arg = $ARGV[0];
my $basefile = $ARGV[1];

my $new = $basefile . ".new";
my $old = $basefile;

open(OLDOUT, "<$old");
open(NEWOUT, ">$new");
while(<OLDOUT>)
{
	chomp;
	print NEWOUT "$_\n";
}
print NEWOUT "$arg\n";
# make it run long enough to remove overhead variance
# from matching to running
sleep 40;

close(OLDOUT);
close(NEWOUT);
system("mv $new $old");
