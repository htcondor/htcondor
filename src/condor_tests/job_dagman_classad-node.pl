#! /usr/bin/env perl
use CondorTest;

runToolNTimes("condor_q -l $ARGV[0] | grep DAG_",1,1);

print "Node $ARGV[1] succeeded\n";
