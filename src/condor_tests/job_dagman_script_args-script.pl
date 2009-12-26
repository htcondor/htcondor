#! /usr/bin/env perl

$node = shift;
$script = shift;
$name = "job_dagman_script_args-" . $node . "-" . $script . ".out";

open(OUT, ">$name") or die "Can't open file $name";

foreach $arg (@ARGV) {
	print OUT "<$arg> ";
}
print OUT "\n";

close(OUT);
