#! /usr/bin/env perl

use CondorTest;
use CondorUtils;

my $log = $ARGV[0];
my $daglog = 'job_dagman_repeat_holds.dag.nodes.log';

# Get our directory
die "TEMP is not defined!\n" if(! exists $ENV{'TEMP'});

# Reset CONDOR_CONFIG so we get the schedd here
my $tmp = $ENV{'TEMP'};
$tmp =~ s/(\.saveme).*/\1/;
$ENV{'CONDOR_CONFIG'} = "$tmp/condor_config";

# Poll until the log file exists
while(1) {
	last if(-e $log);
	sleep 5;
}

# Poll until we see a job executed event
WAIT_FOR_EXECUTE: while(1) {
	open EVENTS, "x_read_joblog.exe $log TRACE|" || die "Could not run x_read_joblog\n";
	while(<EVENTS>) {
		if(/Job Executed/) {
			print "Saw Job Executed event\n";
			last WAIT_FOR_EXECUTE;
		}
	}
	close EVENTS;
	sleep 5;
}

# Now get the cluster Id

open EVENTS, "<$log" || die "Could not open $log for reading\n";
my $jobid;
while(<EVENTS>) {
	if(/^000 \((.+?)\)/) {
		$jobid = $1;
		$jobid =~ s/\....$//;
		$jobid =~ s/^0+//;
		$jobid =~ s/\.00/\./;
		last;
	}
}
close EVENTS;

# Hold the job

my $cluster = $jobid;
$cluster =~ s/\..*//;
runcmd ("condor_hold $cluster");

# Now find the hold event
#
my @repeatthis;
HOLD: while(1) {
		  open EVENTS, "<$log" || die "Could not open $log for reading\n";;
		  my $pushthis = 0;
		  my $read_event = 0;

# Get a copy of the hold event from the log
		  while(<EVENTS>) {
			  if($read_event == 0 && /^012/) { $pushthis = 1; }
			  if($pushthis != 0) {
				  push @repeatthis,$_;
			  }
			  if($pushthis != 0 && /^\.\.\.$/) {
				  $pushthis = 0;
				  $read_event = 1;
				  last;
			  }
		  }
		  close EVENTS;

		  # Never saw the terminator
		  if($pushthis != 0) {
		    @repeatthis = ();
		    $pushthis = 0;
		  }
		  last if($read_event == 1);
		  sleep 5;
	  }


# Write a copy of the hold event into the log
open EVENTS, ">>$daglog" || die "Could not open $daglog for appending\n";
print EVENTS (join '',@repeatthis);
close EVENTS;

sleep 5;

runcmd ("condor_release $cluster");
exit 0;
