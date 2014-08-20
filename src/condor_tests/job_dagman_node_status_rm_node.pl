#! /usr/bin/env perl

print "Job for node $ARGV[0]\n";
print "Condor_rm'ing parent DAGMan ($ARGV[1])\n";

system("condor_rm $ARGV[1]");

# Time for condor_rm to take effect before we finish...
sleep(30);

print "OK done with $ARGV[0]\n";
exit(0);
