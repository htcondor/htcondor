#! /usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

# Creates a large DAG for testing -- node jobs don't really do anything
# except for the last node, which does condor_hold/condor_release on the
# DAGMan job to force DAGMan into recovery mode.

# This script was mainly created to make sure the new "lazy log file"
# code works correctly on a large DAG with many log files.

my $usage = "Usage: create_large_dag <name> <width> <depth> " .
			"<unique logs> <maxjobs>";

if ($ARGV[0] eq "-usage") {
	print "$usage\n";
	exit 0;
}

if ($#ARGV != 4) {
	print "Incorrect number of arguments!\n";
	die "$usage\n";
}

my $testname = shift;
my $width = shift;
my $depth = shift;
my $unique_logs = shift;
my $maxjobs = shift;

my $dagfile = $testname . ".dag";

print "Writing $width x $depth DAG to $dagfile; ";
if ($unique_logs) {
	print "using unique log files\n";
} else {
	print "using common log file\n";
}

#
# Create the configuration file.
#
my $configfile = $testname . ".config";

open(CONFIGFILE, ">$configfile") or die "Can't open/create config file " .
			"$configfile $!\n";
print CONFIGFILE "DAGMAN_MAX_JOBS_SUBMITTED = $maxjobs\n";
print CONFIGFILE "DAGMAN_MAX_SUBMITS_PER_INTERVAL = 1000\n";
# This only works in 7.3.2 and later.
print CONFIGFILE "DAGMAN_USER_LOG_SCAN_INTERVAL = 1\n";
# print CONFIGFILE "DAGMAN_CONDOR_SUBMIT_EXE = dummy_condor_submit.pl\n";
# Turning off userlog fsync() makes the test run an order of magnitude
# faster, at least on RHEL 6.4.
print CONFIGFILE "ENABLE_USERLOG_FSYNC = false\n";
close(CONFIGFILE);

#
# Create the DAG file.
#
open(DAGFILE, ">$dagfile") or die "Can't open/create DAG file $dagfile $!\n";

# Configuration file reference.
print DAGFILE "CONFIG $configfile\n";

# Node status file.
my $statusfile = $testname . ".status";
print DAGFILE "NODE_STATUS_FILE $statusfile\n";

# Initial node.
my $nodename = $testname . "-nodeA";
my $submitfile = $nodename . ".cmd";
WriteNode($nodename, $submitfile, 1);
WriteSubmit($testname, $submitfile, $nodename, $unique_logs, 0);
my $firstnodename = $nodename;

# Final node -- written here to make parent/child lines easier to write.
$nodename = $testname . "-nodeC";
$submitfile = $nodename . ".cmd";
WriteNode($nodename, $submitfile, 0);
WriteSubmit($testname, $submitfile, $nodename, $unique_logs, 1);
my $lastnodename = $nodename;

# "Main" nodes.
if ($unique_logs == 0) {
	# Write the "master" submit file.
	$nodename = $testname . "-nodeB";
	$submitfile = $testname . "-nodeB" . ".cmd";
	WriteSubmit($testname, $submitfile, $nodename, $unique_logs, 0);
}

my $windex;
my $dindex;
for ($windex = 0; $windex < $width; $windex++) {
	my $prevnodename;
	for ($dindex = 0; $dindex < $depth; $dindex++) {
		$nodename = $testname . "-nodeB" . $windex . "_" . $dindex;
		if ($unique_logs != 0) {
			$submitfile = $nodename . ".cmd";
			WriteSubmit($testname, $submitfile, $nodename, $unique_logs, 0);
		}
		WriteNode($nodename, $submitfile, 1);
		if ($dindex < 1) {
			print DAGFILE "PARENT $firstnodename CHILD $nodename\n";
		} else {
			print DAGFILE "PARENT $prevnodename CHILD $nodename\n";
		}
		$prevnodename = $nodename;
	}
	print DAGFILE "PARENT $prevnodename CHILD $lastnodename\n";

}

close(DAGFILE);


# -----------------------------------------------------------------
# Write a node in the DAG -- write the JOB and SCRIPT lines
# in the DAG file.
sub WriteNode
{
	my $nodename = shift;
	my $submitfile = shift;
	my $noop = shift;

	my $jobline = "JOB $nodename $submitfile";
	if ($noop) {
		$jobline = $jobline . " NOOP";
	}
	print DAGFILE "\n$jobline\n";
	# print DAGFILE "SCRIPT PRE $nodename job_dagman_large_dag-node.pl " .
				# "Args: PRE $nodename\n";
	# print DAGFILE "SCRIPT POST $nodename job_dagman_large_dag-node.pl " .
				# "Args: POST $nodename\n";
}

# -----------------------------------------------------------------
# Write a submit file.
sub WriteSubmit
{
	my $testname = shift;
	my $submitfile = shift;
	my $nodename = shift;
	my $unique_logs = shift;
	my $dohold = shift; # Whether node does condor_hold/condor_release.

	open(SUBMITFILE, ">$submitfile") or die "Can't open/create submit " .
				"file $submitfile $!\n";

	my $jobfile = $submitfile;
	$jobfile =~ s/\.cmd/.pl/;
	my $jobfile = "./" . $jobfile;
	if ($dohold) {
		print SUBMITFILE "executable = $jobfile\n";
		print SUBMITFILE "# Note: we need getenv = true for the node " .
					"job to talk to the schedd of\n";
		print SUBMITFILE "# the personal condor that's running the test.\n";
		print SUBMITFILE "getenv = true\n";
		print SUBMITFILE "arguments = \$(DAGManJobId)\n";

	} else {
		print SUBMITFILE "executable = job_dagman_large_dag-node.pl\n";
		print SUBMITFILE "arguments = JOB $nodename\n";
		# print SUBMITFILE "events = sub exec term\n";
	}
	print SUBMITFILE "universe = scheduler\n";
	my $logfile;
	if ($unique_logs) {
		$logfile = $nodename . ".log";
		print SUBMITFILE "log = $logfile\n";
	} else {
		# Use default log file.
		# $logfile = $testname . ".log";
	}
	# print SUBMITFILE "log = $logfile\n";
	print SUBMITFILE "notification = NEVER\n";
	my $outfile = $nodename . ".out";
	print SUBMITFILE "output = $outfile\n";
	my $errfile = $nodename . ".err";
	print SUBMITFILE "error = $errfile\n";
	print SUBMITFILE "queue\n";

	close(SUBMITFILE);

	if ($dohold) {
		open(JOBFILE, ">$jobfile") or die "Can't open/create  " .
					"file $jobfile $!\n";
		print JOBFILE "#! /usr/bin/env perl\n";
		print JOBFILE "\n";
		print JOBFILE "open (OUTPUT, \"condor_hold \$ARGV[0] 2>&1 |\") " .
					"or die \"Can't fork: \$!\";\n";
		print JOBFILE "while (<OUTPUT>) {\n";
		print JOBFILE "	print \"\$_\";\n";
		print JOBFILE "}\n";
		print JOBFILE "close (OUTPUT) or die \"Condor_hold failed: \$?\";\n";
		print JOBFILE "\n";
		print JOBFILE "sleep 60;\n";
		print JOBFILE "\n";
		print JOBFILE "open (OUTPUT, \"condor_release \$ARGV[0] 2>&1 |\") " .
					"or die \"Can't fork: \$!\";\n";
		print JOBFILE "while (<OUTPUT>) {\n";
		print JOBFILE "	print \"\$_\";\n";
		print JOBFILE "}\n";
		print JOBFILE "close (OUTPUT) or die \"Condor_release failed: \$?\";\n";
		print JOBFILE "\n";
		print JOBFILE "print \"Node C succeeded\\n\";\n";
		close(JOBFILE);

		chmod 0755, $jobfile;
	}
}
