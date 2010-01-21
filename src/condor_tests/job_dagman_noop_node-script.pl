#! /usr/bin/env perl

$node = shift;
$script = shift;
$result = shift;
$name = "job_dagman_noop_node-" . $node . "-" . $script . ".out";

open(OUT, ">$name") or die "Can't open file $name";
print OUT "$node result: <$result> ";
print OUT "\n";

close(OUT);

exit ($result);
