#!/usr/bin/env perl

######################################################################
# $Id: remote_post_always.pl,v 1.1.2.9 2004-06-25 03:04:47 wright Exp $
# post script for Condor testsuite that is run regardless of the
# testsuite end status  
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";

######################################################################
# kill test suite personal condor daemons
######################################################################

$pid_file = "$BaseDir/condor_master_pid";
if( ! -f $pid_file ) {
    # if there's no pid_file, there must be no personal condor running
    # which we'd have to kill.  this would be caused by an empty
    # tasklist.  so, make sure the tasklist is empty.  if so, we can
    # exit with success.  if there are tasks but no pid_file, that's a
    # weird fatal error and we should die...

    if( ! -f "tasklist.nmi" || -z "tasklist.nmi" ) {
	# our tasklist is empty, good.
	print "tasklist.nmi is empty and there's no condor_master_pid " .
	    "file.\nNothing to cleanup, returning SUCCESS.\n";
	exit 0;
    } else {
	die "tasklist.nmi contains data but condor_master_pid " .
	    "does not exist!\n";
    }
}

    
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
