#! /usr/bin/env perl
use CondorTest;
use CondorPersonal;

$cmd_template = "job_condorc_abc_van.template";
$testname = 'job_condorc_abc_van - Chained Condor-C A&B&C with Vanilla job';
$corename = "job_condorc_abc_van";

# where are we running
$dummyhost = "gamgee.cs.wisc.edu";
$currenthost = `hostname`;
chomp($currenthost);
print "Running on $currenthost\n";

$mypid = $$;
$mysubmitnm = $corename . $mypid . ".cmd";
$mysubmitnmout = $corename . $mypid . ".cmd.out";
$mysubmitnmlog = $corename . $mypid . ".log";
$mycorenamecmd = $corename . ".cmd";
$mycorenamecmdout = $corename . ".cmd.out";
$mycorenamelog = $corename . ".log";

print "Master PID is $mypid\n";
print "Master PID submit file is $mysubmitnm\n";

#my $configrem = CondorPersonal::DebugOn();

# get a remote scheduler running (side c)
my $configrem = CondorPersonal::StartCondor("x_param.condorcremote" ,"remote");
my @remote = split /:/, $configrem;
my $remconfig = shift @remote;
my $remport = shift @remote;

print "---Remote config(c) is $remconfig and remote port is $remport---\n";

