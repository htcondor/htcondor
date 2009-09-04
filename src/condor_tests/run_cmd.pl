#! /usr/bin/env perl
use strict;
use warnings;
use CondorTest;
use CondorUtils;

my @aarray;
my $cmd = "ls ..";

#my $status = CondorTest::runCondorTool($cmd,\@aarray,2);
my $hashref = runcmd("ls ..",{expect_result=>\&PASS,cmnt=>"bug 23456"});
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

