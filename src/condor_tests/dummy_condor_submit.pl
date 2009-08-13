#! /usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

# This is a dummy version of condor_submit intended for DAGMan testing.
# It simulates a condor_submit by writing specified events to the
# job log file (no Condor job is actually submitted).
# This allows us to test DAGMan with more jobs, and with more
# repeatability, than submitting "real" node jobs.

# Note that this script will not work correctly if there are any
# macros in relevant submit file values.

# --------------------------------------------------------------

# Get the submit file (last argument).
my $submitfile = $ARGV[$#ARGV];

# Get the DAG node name from the command line (if it's specified)
# for submit event.
my $dagNode = "";
foreach $arg (@ARGV) {
	if ($arg =~ /dag_node_name\s*=\s*(.*)/) {
		$dagNode = $1;
	}
}


# Get the log file and event list from the submit file.
my $logfile = "";
my $events = "";
open(SUBFILE, "<$submitfile") or die "Can't open submit file $submitfile $!\n";
while(<SUBFILE>) {
	chomp();
	$line = $_;
	if ($line =~ /log\s*=\s*(.*)/) {
		$logfile = $1;
	}
	if ($line =~ /events\s*=\s*(.*)/) {
		$events = $1;
	}
}
close(SUBFILE);

die "No log file specified in submit file $submitfile\n" if ($logfile eq "");


# Call the "real" condor_submit if no events are specified in the
# submit file.
if ($events eq "") {
	my @subargs = ("condor_submit");
	push @subargs, @ARGV;
	if (system(@subargs) != 0) {
		die "'Real' condor_submit failed\n";
	}
}


# Get the cluster ID (incremented each time this is run).
my $clusterId = GetClusterId();

# Open the log file and actually write the events.
open(LOGFILE, ">>$logfile") or die "Can't open/create log file $logfile $!\n";

my @eventlist = split /\s+/, $events;
foreach $event (@eventlist) {
	if ($event eq "sub") {
		WriteSubmit($clusterId, $dagNode);

	} elsif ($event eq "exec") {
		WriteExecute($clusterId);

	} elsif ($event eq "term") {
		WriteTerminated($clusterId);
	}
}

close(LOGFILE);


# Note: there's an obvious race condition here if more than one
# copy of this script is run at once in the same directory.
sub GetClusterId
{
	my $idfile = "dummy_condor_submit.id";
	# Start cluster ids at 1000001 to avoid possible conflict with
	# "real" cluster ids.
	my $cluster = 1000000;
	open(IDFILE, "<$idfile");
	while(<IDFILE>) {
		chomp();
		$cluster = $_;
	}
	close(IDFILE);

	$cluster++;

	open(IDFILE, ">$idfile") or die "Can't open ID file $idfile $!\n";
	print IDFILE "$cluster\n";
	close(IDFILE);

	return $cluster;
}

sub WriteSubmit
{
	print "Submitting job(s).\n";

	my $id = shift;
	my $node = shift;
	my $timestamp = GetTimestamp();

	print LOGFILE "000 ($id.000.000) $timestamp Job submitted from " .
				"host: <127.0.0.1:41632>\n";
	if ($node ne "") {
		print LOGFILE "    DAG Node: $node\n";
	}
	print LOGFILE "...\n";

	print "Logging submit event(s).\n";
	print "1 job(s) submitted to cluster $id\n";
}

sub WriteExecute
{
	my $id = shift;
	my $timestamp = GetTimestamp();
	print LOGFILE "001 ($id.000.000) $timestamp Job executing on " .
				"host: <127.0.0.1:41632>\n";
	print LOGFILE "...\n";
}

sub WriteTerminated
{
	my $id = shift;
	my $timestamp = GetTimestamp();
	print LOGFILE "005 ($id.000.000) $timestamp Job terminated.\n";
	print LOGFILE "        (1) Normal termination (return value 0)\n";
	print LOGFILE "                Usr 0 00:00:00, Sys 0 00:00:00" .
				"  -  Run Remote Usage\n";
	print LOGFILE "                Usr 0 00:00:00, Sys 0 00:00:00" .
				"  -  Run Local Usage\n";
	print LOGFILE "                Usr 0 00:00:00, Sys 0 00:00:00" .
				"  -  Total Remote Usage\n";
	print LOGFILE "                Usr 0 00:00:00, Sys 0 00:00:00" .
				"  -  Total Local Usage\n";
	print LOGFILE "        0  -  Run Bytes Sent By Job\n";
	print LOGFILE "        0  -  Run Bytes Received By Job\n";
	print LOGFILE "        0  -  Total Bytes Sent By Job\n";
	print LOGFILE "        0  -  Total Bytes Received By Job\n";
	print LOGFILE "...\n";
}

sub GetTimestamp
{
	($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime();
	$mon++; # start at 1
	my $result = sprintf "%2.2d/%2.2d %2.2d:%2.2d:%2.2d", $mon, $mday,
				$hour, $min, $sec;
	return $result;
}
