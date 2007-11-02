#! /usr/bin/env perl

open(IN,"<job_condorc_xxx_van.in") || die "can not open job_condorc_xxx_van.in:$!\n";
open(OUT,">outfile1") || die "can not open outfile1:$!\n";
while(<IN>) {
	print OUT "$_\n";
}
close(IN);
$cmd = `pwd`;
print OUT "$cmd\n";
print OUT "Done\n";
close(OUT);
