#! /usr/bin/env perl

# Create the DAG for this node...
$file = ">job_dagman_pre_subdag-A-lower.dag";
open(OUT, ">$file") or die "Can't open $file\n";

print OUT "Job LA job_dagman_pre_subdag-A-node.cmd\n";
print OUT "Vars LA nodename = \"\$(JOB)\"\n\n";

print OUT "Job LB job_dagman_pre_subdag-A-node.cmd\n";
print OUT "Vars LB nodename = \"\$(JOB)\"\n\n";

print OUT "Job LC job_dagman_pre_subdag-A-node.cmd\n";
print OUT "Vars LC nodename = \"\$(JOB)\"\n\n";

print OUT "Job LD job_dagman_pre_subdag-A-node.cmd\n";
print OUT "Vars LD nodename = \"\$(JOB)\"\n\n";

print OUT "PARENT LA CHILD LB LC\n";
print OUT "PARENT LB LC CHILD LD\n";

close(OUT);
