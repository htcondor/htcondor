#! /usr/bin/env perl
use CondorTests;

runToolNTimes("condor_q -l $ARGV[0] | grep DAG_",1,1);

print "Node $ARGV[1] succeeded\n";
