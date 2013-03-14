#! /usr/bin/env perl

#TEMPTEMP -- check whether system command fails?
system("condor_q -l $ARGV[0] | grep DAG_");

print "Node $ARGV[1] succeeded\n";
