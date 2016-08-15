#! /usr/bin/env perl

$sleep = shift @ARGV;
sleep($sleep);

$file = shift @ARGV;
open(OUT, ">>$file") or die "Can't open $file: $!";
while ($arg = shift @ARGV) {
	print OUT "$arg\n";
}
close(OUT);
