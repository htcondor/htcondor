#!/usr/bin/perl

######################################################################
# $Id: remote_post_always.pl,v 1.1.2.3 2004-06-24 00:58:02 wright Exp $
# post script for Condor testsuite that is run regardless of the
# testsuite end status  
######################################################################

######################################################################
# set up path 
######################################################################

my $HomeDir = $ENV{HOME};
print "PATH is $ENV{PATH}\n";

######################################################################
# kill test suite personal condor daemons
######################################################################

$pid_file = "$HomeDir/condor_master_pid";
# Get master PID from file
open (PIDFILE, "$pid_file") || die "cannot open $pid_file for reading\n";
while (<PIDFILE>){
  chomp;
  $master_pid = $_;
}
close PIDFILE;

# probably try to stop more gracefully, then wait 30 seconds and kill if necessary
print "KILLING Personal Condor master daemon (pid: $master_pid)\n";
kill 15, $master_pid;

sub verbose_system {

  my @args = @_;
  my $rc = 0xffff & system @args;

  printf "system(%s) returned %#04x: ", @args, $rc;

  if ($rc == 0) {
   print "ran with normal exit\n";
   return $rc;
  }
  elsif ($rc == 0xff00) {
   print "command failed: $!\n";
   return $rc;
  }
  elsif (($rc & 0xff) == 0) {
   $rc >>= 8;
   print "ran with non-zero exit status $rc\n";
   return $rc;
  }
  else {
   print "ran with ";
   if ($rc &   0x80) {
       $rc &= ~0x80;
       print "coredump from ";
   return $rc;
   }
   print "signal $rc\n"
  }
}

