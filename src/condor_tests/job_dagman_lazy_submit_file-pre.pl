#! /usr/bin/env perl

$file = "job_dagman_lazy_submit_file-node.cmd";
open(OUT, ">$file") or die "Can't open $file\n";

print OUT "executable		= /bin/echo\n";
print OUT "arguments		= Cadel Evans\n";
print OUT "universe		= scheduler\n";
print OUT "log				= job_dagman_lazy_submit_file-node.log\n";
print OUT "notification	= NEVER\n";
print OUT "output			= job_dagman_lazy_submit_file-node.out\n";
print OUT "error			= job_dagman_lazy_submit_file-node.err\n";
print OUT "queue\n";

close(OUT);
