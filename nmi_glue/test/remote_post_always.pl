#!/usr/bin/env perl

######################################################################
# $Id: remote_post_always.pl,v 1.1.2.7 2004-06-25 02:35:59 wright Exp $
# post script for Condor testsuite that is run regardless of the
# testsuite end status  
######################################################################

######################################################################
# set up path 
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
print "PATH is $ENV{PATH}\n";

######################################################################
# kill test suite personal condor daemons
######################################################################

$pid_file = "$BaseDir/condor_master_pid";
# Get master PID from file
open (PIDFILE, "$pid_file") || die "cannot open $pid_file for reading\n";
while (<PIDFILE>){
  chomp;
  $master_pid = $_;
}
close PIDFILE;

# probably try to stop more gracefully, then wait 30 seconds and kill
# if necessary
print "KILLING Personal Condor master daemon (pid: $master_pid)\n";
kill( 15, $master_pid ) || 
    die "Can't kill condor_master (pid: $master_pid): $!\n";
