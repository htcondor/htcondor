#! /usr/bin/env perl

system("condor_q -l $ARGV[0] | grep DAG_");

print "Node $ARGV[1] succeeded\n";
