#! /usr/bin/env perl
use CondorTest;

my $retries = 4;
my $sleeptime = 2;
my @resultsarray = ();
my @testingarray = ();
my $jobid = $ARGV[0];
my $nodename = $ARGV[1];

@NodeA = ("DAG_Address = (.*)",
	"DAG_AdUpdateTime = (.*)",
	"DAG_InRecovery = 0",
	"DAG_JobsCompleted = 0",
	"DAG_JobsHeld = 0",
	"DAG_JobsIdle = 0",
	"DAG_JobsRunning = 1",
	"DAG_JobsSubmitted = 1",
	"DAG_NodesDone = 0",
	"DAG_NodesFailed = 0",
	"DAG_NodesFutile = 0",
	"DAG_NodesHoldrun = 0",
	"DAG_NodesPostrun = 0",
	"DAG_NodesPrerun = 0",
	"DAG_NodesQueued = 1",
	"DAG_NodesReady = 0",
	"DAG_NodesTotal = 3",
	"DAG_NodesUnready = 2",
	"DAG_Stats = (.*)",
	"DAG_Status = 0");
	#"Node NodeA succeeded");

@NodeB = ("DAG_Address = (.*)",
	"DAG_AdUpdateTime = (.*)",
	"DAG_InRecovery = 0",
	"DAG_JobsCompleted = 1",
	"DAG_JobsHeld = 0",
	"DAG_JobsIdle = 0",
	"DAG_JobsRunning = 1",
	"DAG_JobsSubmitted = 2",
	"DAG_NodesDone = 1",
	"DAG_NodesFailed = 0",
	"DAG_NodesFutile = 0",
	"DAG_NodesHoldrun = 0",
	"DAG_NodesPostrun = 0",
	"DAG_NodesPrerun = 0",
	"DAG_NodesQueued = 1",
	"DAG_NodesReady = 0",
	"DAG_NodesTotal = 3",
	"DAG_NodesUnready = 1",
	"DAG_Stats = (.*)",
	"DAG_Status = 0");
	#"Node NodeB succeeded");

@NodeC = ("DAG_Address = (.*)",
	"DAG_AdUpdateTime = (.*)",
	"DAG_InRecovery = 0",
	"DAG_JobsCompleted = 2",
	"DAG_JobsHeld = 0",
	"DAG_JobsIdle = 0",
	"DAG_JobsRunning = 1",
	"DAG_JobsSubmitted = 3",
	"DAG_NodesDone = 2",
	"DAG_NodesFailed = 0",
	"DAG_NodesFutile = 0",
	"DAG_NodesHoldrun = 0",
	"DAG_NodesPostrun = 0",
	"DAG_NodesPrerun = 0",
	"DAG_NodesQueued = 1",
	"DAG_NodesReady = 0",
	"DAG_NodesTotal = 3",
	"DAG_NodesUnready = 0",
	"DAG_Stats = (.*)",
	"DAG_Status = 0");
	#"Node NodeC succeeded");

if($nodename eq "NodeA") {
	@resultsarray = @NodeA;
}
if($nodename eq "NodeB") {
	@resultsarray = @NodeB;
}
if($nodename eq "NodeC") {
	@resultsarray = @NodeC;
}

my $count = 0;
my $res = 0;
while($count < $retries) {
	runCondorTool("condor_q -l $ARGV[0]",\@testingarray,2,{emit_output=>0});
	#runToolNTimes("condor_q -l $ARGV[0] | grep DAG_",1,1);
	$res = TestNodeState(@testingarray);
	if($res == 0) {
		print "Node $ARGV[1] succeeded\n";
		exit(0);
	} else {
		sleep($sleeptime);
		$count++;
	}
}
if($count == $retries) {
	print "Never found exact set of states in node:$ARGV[1]\n";
	exit(1);
}



sub TestNodeState
{
	my $res = 0;
	foreach my $string (@testingarray) {
		if($string =~ /DAG_/) {
			print "Have:$string";
		}
	}
	foreach my $string (@resultsarray) {
		print "Expect:$string\n";
	}
	foreach my $string (@testingarray) {
		if($string =~ /DAG_/) {
			$res = LookupString($string);
			if($res == 1) {
				return($res);
			}
		}
	}
	return(0);
}
	

sub LookupString
{
	my $search = shift;
	chomp($search);
	# look for string in expected results
	foreach my $maybe (@resultsarray) {
		if($search =~ /$maybe/) {
			return(0);
		}
	}
	#miss
	print "Failed to find:$search\n";
	return(1);
}
