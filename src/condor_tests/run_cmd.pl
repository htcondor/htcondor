#! /usr/bin/env perl
use strict;
use warnings;
use CondorUtils;

my $hashref = runcmd("touch foo",{expect_result=>\&FAIL});
#print "verbose_system returned\n";
#foreach my $key (keys %{$hashref}) {
	#print "${$hashref}{$key}\n";
	#print "$key\n";
#}
 
#if(exists ${$hashref}{"output"}) {
	#print "Output returned......\n";
#}

#my @output = @{${$hashref}{"output"}};
#print "Return value ${$hashref}{success}\n";
#print "STDOUT:\n", @output, "\n";

