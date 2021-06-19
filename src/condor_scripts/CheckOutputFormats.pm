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

package: CheckOutputFormats;
use strict;
use warnings;                                                                                     
use CondorTest;
use CondorUtils;
use CheckOutputFormats;
use Check::SimpleJob;
use POSIX ();
use Sys::Hostname;
use Cwd;
use List::Util;

use Exporter qw(import);
our @EXPORT_OK = qw(dry_run add_status_ads change_clusterid multi_owners create_table various_hold_reasons find_blank_columns split_fields cal_from_raw trim count_job_states make_batch check_heading check_data check_summary emit_dag_files emit_file);                                                    
sub combine_dry_ads {
	my $submit_file = $_[0];
	my $command_arg = $_[1];
	my $counter = 1;
	my $index = 0;
	my %all_keys;
	my %Attr;
	my @lines = `condor_submit $command_arg -dry - $submit_file`;
	my $len = scalar @lines;
	while ($counter < $len){
		if ($lines[$counter] =~ /^.$/){
			$index++;
		} else {
			if ($lines[$counter] =~ /^([A-Za-z0-9_]+)\s*=\s*(.*)$/){
				my $key = $1;
				my $value = $2;
				if (!defined $all_keys{$key}){
					$all_keys{$key} = $key;
				}
				$Attr{$index}{$key} = $value;
			}
		}
		$counter++;
	}
	
	if ($index > 1){
		for my $i (1..$index-1){
			%{$Attr{$i}} = (%{ $Attr{0}}, %{$Attr{$i}});
		}
	}
	return %Attr;
}

sub dry_run_with_batchname {
	my $testname = $_[0];
	my $pid = $_[1];
	my $executable = $_[2];
	my $arguments = $_[3];
	my $batchname = $_[4];
	my $submit_content = 
"executable = $executable
arguments = $arguments
queue 2";
	my $submitfile = "$testname$pid.sub";
	emit_dag_files($testname, $submit_content, $pid);
	my %Attr = combine_dry_ads($submitfile, "-batch-name $batchname");
	return %Attr;
}

# construct a hash containing job ads from dry run
sub dry_run {
	my $testname = $_[0];
	my $pid = $_[1];
	my $ClusterId = $_[2];
	my $executable = $_[3];
	my $submit_file = "$testname$pid.sub";
	my $host = hostname;
	my $time = time();
	my %Attr = combine_dry_ads($submit_file, "");
	my $index = scalar keys %Attr;
	
	# add the additional job ads that would appear in condor_q -long
	for my $i (0..$index-1){
		my $owner = $Attr{$i}{Owner};
		if ($owner eq 'undefined') { $Attr{$i}{Owner} = '"billg"'; $owner = '"billg"'; }
		$owner = substr($owner,0,length($owner)-1);
		my $FileSystemDomain = $Attr{$i}{FileSystemDomain};
		$FileSystemDomain = substr($FileSystemDomain, 1, length($FileSystemDomain)-1);
		$Attr{$i}{User} = $owner."@".$FileSystemDomain;
		$Attr{$i}{Cmd} = "\"$executable\"";
		$Attr{$i}{NumCkpts_RAW} = 0;
		my $QDate = $Attr{$i}{QDate};
		$Attr{$i}{EnteredCurrentStatus} = $QDate + int(rand(100));
		$Attr{$i}{GlobalJobId} = "\"".$host."#".$ClusterId.".".$i."#".$QDate."\"";
		$Attr{$i}{QDate} = $time;
		my $extra_time = int(rand(200));
		$Attr{$i}{ServerTime} = $Attr{$i}{QDate} + $extra_time + int(rand(20));
		$Attr{$i}{ExecutableSize_RAW} = $Attr{$i}{ExecutableSize};
		$Attr{$i}{ExecutableSize} = cal_from_raw($Attr{$i}{ExecutableSize_RAW});
		$Attr{$i}{DiskUsage_RAW} = $Attr{$i}{DiskUsage};
		$Attr{$i}{DiskUsage} = cal_from_raw($Attr{$i}{DiskUsage_RAW});
		$Attr{$i}{ImageSize_RAW} = $Attr{$i}{ImageSize};
		$Attr{$i}{ImageSize} = cal_from_raw($Attr{$i}{ImageSize_RAW});
		$Attr{$i}{JobBatchName} = "\"CMD: $executable\"";
		$Attr{$i}{CumulativeSlotTime} = ($Attr{$i}{EnteredCurrentStatus} - $Attr{$i}{QDate})."\."."0";
		$Attr{$i}{RemoteWallClockTime} = $Attr{$i}{CumulativeSlotTime};
	}
	return %Attr;
}

# function to add additional job ads according to JobStatus
# input: list of raw job ads and jobstatus number
sub add_status_ads{
	my %Attr = %{$_[0]};
	my $host = hostname;
	my $time = time();
	my $total_cnt = scalar keys %Attr;
	for my $i (0..$total_cnt-1){

# HOLD
		if ($Attr{$i}{JobStatus} == 5){
			$Attr{$i}{AutoClusterAttrs} = "\"JobUniverse,LastCheckpointPlatform,NumCkpts,MachineLastMatchTime,ConcurrencyLimits,NiceUser,Rank,Requirements,DiskUsage,FileSystemDomain,ImageSize,MemoryUsage,RequestDisk,RequestMemory,ResidentSetSize\"";
			$Attr{$i}{AutoClusterId} = 10;
			$Attr{$i}{BytesRecvd} = "0.0";
			$Attr{$i}{BytesSent} = "0.0";
			$Attr{$i}{CondorVersion} = "\"\$CondorVerison: 8.5.6 Jun 13 2016 BuildID: UW_development PRE-RELEASE-UWCS \$\"";
			$Attr{$i}{DiskUsage_RAW} = 50;
			$Attr{$i}{DiskUsage} = cal_from_raw($Attr{$i}{DiskUsage_RAW});
			$Attr{$i}{ExecutableSize_RAW} = 50;
			$Attr{$i}{ExecutableSize} = cal_from_raw($Attr{$i}{ExecutableSize_RAW});
			$Attr{$i}{HoldReasonSubCode} = 0;
			$Attr{$i}{ImageSize_RAW} = 4068;
			$Attr{$i}{ImageSize} = cal_from_raw($Attr{$i}{ImageSize_RAW});
			$Attr{$i}{JobStartDate} = $Attr{$i}{QDate};
			$Attr{$i}{JobCurrentStartDate} = $Attr{$i}{JobStartDate};
			$Attr{$i}{JobCurrentStartExecutingDate} = $Attr{$i}{JobStartDate};
			$Attr{$i}{LastJobLeaseRenewal} = $Attr{$i}{EnteredCurrentStatus};
			$Attr{$i}{LastMatchTime} = $Attr{$i}{QDate};
			$Attr{$i}{LastPublicClaimId} = "\"<127.0.0.1:".int(rand(99999)).">#".int(rand(9999999999))."#".int(rand(99))."#..."."\"";
			$Attr{$i}{LastRemoteHost} = "\""."slot".($i+1)."@".$host."\"";
			$Attr{$i}{LastVacateTime} = $Attr{$i}{EnteredCurrentStatus};
			$Attr{$i}{MachineAttrCpus0} = 1;
			$Attr{$i}{MachineAttrSlotWeight0} = 1;
			$Attr{$i}{MemoryUsage} = "((ResidentSetSize + 1023 ) / 1024)";
			$Attr{$i}{NumJobMatches} = 1;
			$Attr{$i}{NumJobStarts} = 1;
			$Attr{$i}{NumShadowStarts} = 1;
			$Attr{$i}{JobRunCount} = $Attr{$i}{NumShadowStarts};
			$Attr{$i}{OrigMaxHosts} = 1;
			$Attr{$i}{ResidentSetSize_RAW} = 488;
			$Attr{$i}{ResidentSetSize} = cal_from_raw($Attr{$i}{ResidentSetSize_RAW});
			$Attr{$i}{StartdPrincipal} = "\""."execute-side\@matchsession/127.0.0.1"."\"";

			if ($Attr{$i}{HoldReasonCode}==15){
				$Attr{$i}{HoldReason} = "\"submitted on hold at user's request\"";
				$Attr{$i}{LastJobStatus} = 1;
			}
			if ($Attr{$i}{HoldReasonCode}==3){
				$Attr{$i}{HoldReason} = "\"PERIODIC_HOLD=TRUE\"";
				$Attr{$i}{LastJobStatus} = 2;
			}
			if ($Attr{$i}{HoldReasonCode}==21){
				$Attr{$i}{HoldReason} = "\"WANT_HOLD=TRUE\"";
				$Attr{$i}{LastJobStatus} = 2;
			}	
			if ($Attr{$i}{HoldReasonCode} == 1){
				$Attr{$i}{HoldReason} = "\"via condor_hold (by user ".substr($Attr{$i}{Owner},1,length($Attr{$i}{Owner})-2).")\"";
				$Attr{$i}{LastJobStatus} = 2;
			}

# RUN
		} elsif ($Attr{$i}{JobStatus} == 2){

			my $ran_num_7 = int(rand(9999999));
			my $ran_num_5 = int(rand(99999));
			$Attr{$i}{AutoClusterAttrs} = "\""."JobUniverse,LastCheckpointPlatform,NumCkpts,MachineLastMatchTime,ConcurrencyLimits,NiceUser,Rank,Requirements,DiskUsage,FileSystemDomain,ImageSize,MemoryUsage,RequestDisk,RequestMemory,ResidentSetSize"."\"";
			$Attr{$i}{AutoClusterId} = 1;
			$Attr{$i}{CurrentHosts} = 0;
			$Attr{$i}{DiskUsage} = 2500000;
			$Attr{$i}{JobCurrentStartDate} = $Attr{$i}{QDate};
			$Attr{$i}{JobStartDate} = $Attr{$i}{QDate};
			$Attr{$i}{LastJobLeaseRenewal} = $Attr{$i}{EnteredCurrentStatus};
			$Attr{$i}{LastJobStatus} = 1;
			$Attr{$i}{LastMatchTime} = $Attr{$i}{EnteredCurrentStatus};
			$Attr{$i}{MachineAttrCpus0} = 1;
			$Attr{$i}{MachineAttrSlotWeight0} = 1;
			$Attr{$i}{MemoryUsage} = "((ResidentSetSize + 1023) / 1024)";
			$Attr{$i}{NumJobMatches} =1;
			$Attr{$i}{NumShadowStarts} = 1;
			$Attr{$i}{OrigMaxHosts} = 1;
			$Attr{$i}{RemoteHost} = "\""."slot".($i+1)."@".$host."\"";
			$Attr{$i}{RemoteSlotId} = $i+1;
			$Attr{$i}{ResidentSetSize_RAW} = 488;
			$Attr{$i}{ResidentSetSize} = cal_from_raw($Attr{$i}{ResidentSetSize_RAW});
			$Attr{$i}{PublicClaimID} = "\""."<127.0.0.1:".$ran_num_5.">#".int(rand(9999999999))."#".int(rand(99))."#..."."\"";
			$Attr{$i}{ShadowBday} = $Attr{$i}{QDate};
			$Attr{$i}{StartdIpAddr} = "\""."<127.0.0.1:".$ran_num_5."?addrs=127.0.0.1-".$ran_num_5."&noUDP&sock=".$ran_num_7."_".rand(0xffff)."_".int(rand(10)).">"."\"";
			$Attr{$i}{StartdPrincipal} = "\""."execute-side#matchsession/127.0.0.1"."\"";

		}
	}
	return %Attr;
}

sub add_io_ads {
	my %Attr = %{$_[0]};
	my $arguments = $_[1];
	my $total_cnt = scalar keys %Attr;
	for my $k (0..$total_cnt-1){
		$Attr{$k}{BytesRecvd} = "2000.0";
		$Attr{$k}{BytesSent} = "2000.0";
		$Attr{$k}{FileReadBytes} = "1.0";
		$Attr{$k}{FileWriteBytes} = "1.0";
		$Attr{$k}{JobStatus} = 6;
		$Attr{$k}{TransferQueued} = "true";
		$Attr{$k}{TransferringInput} = "false";
		$Attr{$k}{TransferringOutput} = "false";
		$Attr{$k}{Arguments} = $arguments;
	}
	return %Attr;
}

sub add_dag_ads{
	my %Attr = %{$_[0]};
	my $testname = $_[1];
	my $host = hostname;
	my $total_cnt = scalar keys %Attr;
	for my $i (0..$total_cnt-1){
		$Attr{$i}{DAGManNodesMask} = "\"0,1,2,4,5,7,9,10,11,12,13,16,17,24,27\"";
		$Attr{$i}{ProcId} = 0;
		$Attr{$i}{UserLog} = "\"/$testname-dag.log\"";
		$Attr{$i}{DAGParentNodeNames} = "\"\"";
		$Attr{$i}{JobUniverse} = 5;
		$Attr{$i}{StreamErr} = "false";
		$Attr{$i}{StreamOut} = "false";
		$Attr{$i}{JobBatchName} = "\"/$testname-dag.dag+48\"";
		$Attr{$i}{DAGManNodesLog} = "\"/$testname-dag.nodes.log\"";
	}
	$Attr{0}{SubmitEventNotes} = "\"DAG Node: A\"";
	$Attr{0}{Cmd} = "\"/$testname-dag.sh\"";
	$Attr{0}{OnExitRemove} = "true";
	$Attr{0}{DAGNodeName} = "\"A\"";
	$Attr{0}{Args} = "\"A 120\"";
	$Attr{0}{Out} = "\"/$testname-dag_A.out\"";
	$Attr{0}{Err} = "\"/$testname-dag_A.err\"";
	$Attr{0}{ClusterId} = 49;
	$Attr{0}{DAGManJobId} = 48;
	$Attr{0}{GlobalJobId} = "\"".$host."#".$Attr{0}{ClusterId}.".".$Attr{0}{ProcId}."#".$Attr{0}{QDate}."\"";

	$Attr{1}{SubmitEventNotes} = "\"DAG Node: B_A\"";
	$Attr{1}{Cmd} = "\"".cwd()."/condor_dagman\"";
	$Attr{1}{OnExitRemove} = "( ExitSignal =?= 11 || ( ExitCode =!= undefined && ExitCode >= 0 && ExitCode <= 2 ) )";
	$Attr{1}{RemoveKillSig} = "\"SIGUSR1\"";
	$Attr{1}{DAG_NodesReady} = 0;
	$Attr{1}{DAG_NodesDone} = 0;
	$Attr{1}{DAGNodeName} = "\"B_A\"";
	$Attr{1}{DAG_NodesTotal} = 1;
	$Attr{1}{DAG_Status} = 0;
	$Attr{1}{DAG_NodesFailed} = 0;
	$Attr{1}{DAG_NodesQueued} = 1;
	$Attr{1}{DAG_NodesPrerun} = 0;
	$Attr{1}{DAG_NodesPostrun} = 0;
	$Attr{1}{DAG_NodeUnready} = 0;
	$Attr{1}{DAG_InRecovery} = 0;
	$Attr{1}{JobUniverse} = 7;
	$Attr{1}{KillSig} = "\"SIGTERM\"";
	$Attr{1}{Out} = "\"/$testname-dag_B_A.out\"";
	$Attr{1}{Err} = "\"/$testname-dag_B_A.err\"";
	$Attr{1}{DAGManJobId} = 50;
	$Attr{1}{Args} = "\"-p 0 -f -l . -Lockfile cmd_q_shows-dag-B_A.dag.lock -AutoRescue 1 -DoRescueFrom 0 -Dag cmd_q_shows-dag-B_A.dag -Suppress_notification -CsdVersion \$CondorVersion:' '8.5.6' 'Jun' '15' '2016' 'BuildID:' '370652' 'PRE-RELEASE-UWCS' '$ -Verbose -Force -Notification never -Dagman /home/shuyang/Desktop/condor/release_dir/bin/condor_dagman -Update_submit\"";
	$Attr{1}{UserLog} = "\"/$testname-dag-B_A.log\"";
	$Attr{1}{EnvDelim} = "\";\"";
	$Attr{1}{ClusterId} = 52;
	$Attr{1}{GlobalJobId} = "\"".$host."#".$Attr{1}{ClusterId}.".".$Attr{1}{ProcId}."#".$Attr{0}{QDate}."\"";

	$Attr{2}{Args} = "\"B_A_A 80\"";
	$Attr{2}{SubmitEventNotes} = "\"DAG Node: B_A_A\"";
	$Attr{2}{Cmd} = "\"/$testname-dag-q.sh\"";
	$Attr{2}{OnExitRemove} = "true";
	$Attr{2}{DAGNodeName} = "\"B_A_A\"";
	$Attr{2}{Out} = "\"/$testname-dag.output\"";
	$Attr{2}{Err} = "\"/$testname-dag.error\"";
	$Attr{2}{DAGManJobId} = 52;
	$Attr{2}{UserLog} = "\"/$testname-dag-B_A.log\"";
	$Attr{2}{ClusterId} = 55;
	$Attr{2}{GlobalJobId} = "\"".$host."#".$Attr{2}{ClusterId}.".".$Attr{2}{ProcId}."#".$Attr{0}{QDate}."\"";

	$Attr{3}{Cmd} = "\"/condor_dagman\"";
	$Attr{3}{OnExitRemove} = "( ExitSignal =?= 11 || ( ExitCode =!= undefined && ExitCode >= 0 && ExitCode <= 2 ) )";
	$Attr{3}{RemoveKillSig} = "\"SIGUSR1\"";
	$Attr{3}{DAG_NodesReady} = 0;
	$Attr{3}{DAG_NodesDone} = 0;
	$Attr{3}{DAG_NodesTotal} = 3;
	$Attr{3}{DAG_Status} = 0;
	$Attr{3}{DAG_NodesFailed} = 0;
	$Attr{3}{DAG_NodesQueued} = 3;
	$Attr{3}{DAG_NodesPrerun} = 0;
	$Attr{3}{DAG_NodesPostrun} = 0;
	$Attr{3}{DAG_NodeUnready} = 0;
	$Attr{3}{DAG_InRecovery} = 0;
	$Attr{3}{JobUniverse} = 7;
	$Attr{3}{KillSig} = "\"SIGTERM\"";
	$Attr{3}{Out} = "\"/$testname-dag.dag.lib.out\"";
	$Attr{3}{Err} = "\"/$testname-dag.dag.lib.err\"";
	$Attr{3}{Args} = "\"-p 0 -f -l . -Lockfile cmd_q_shows-dag.dag.lock -AutoRescue 1 -DoRescueFrom 0 -Dag cmd_q_shows-dag.dag -Suppress_notification -CsdVersion \$CondorVersion:' '8.5.6' 'Jun' '15' '2016' 'BuildID:' '370652' 'PRE-RELEASE-UWCS' '$ -Verbose -Force -Notification never -Dagman /home/shuyang/Desktop/condor/release_dir/bin/condor_dagman\"";
	$Attr{3}{UserLog} = "\"/$testname-dag-dagman.log\"";
	$Attr{3}{EnvDelim} = "\";\"";
	$Attr{3}{OtherJobRemoveRequirements} = "\"DAGManJobId =?= 48\"";
	$Attr{3}{ClusterId} = 48;
	$Attr{3}{GlobalJobId} = "\"".$host."#".$Attr{3}{ClusterId}.".".$Attr{3}{ProcId}."#".$Attr{0}{QDate}."\"";

	$Attr{4}{Args} = "\"C 120\"";
	$Attr{4}{SubmitEventNotes} = "\"DAG Node: C\"";
	$Attr{4}{Cmd} = "\"/$testname-dag-q.sh\"";
	$Attr{4}{OnExitRemove} = "true";
	$Attr{4}{DAGNodeName} = "\"C\"";
	$Attr{4}{Out} = "\"/$testname-dag_C.out\"";
	$Attr{4}{Err} = "\"/$testname-dag_C.err\"";
	$Attr{4}{DAGManJobId} = 48;
	$Attr{4}{UserLog} = "\"/$testname-dag.log\"";
	$Attr{4}{ClusterId} = 51;
	$Attr{4}{GlobalJobId} = "\"".$host."#".$Attr{4}{ClusterId}.".".$Attr{4}{ProcId}."#".$Attr{0}{QDate}."\"";

	$Attr{5}{Args} = "\"B_C 100\"";
	$Attr{5}{SubmitEventNotes} = "\"DAG Node: B_C\"";
	$Attr{5}{Cmd} = "\"/$testname-dag-q.sh\"";
	$Attr{5}{OnExitRemove} = "true";
	$Attr{5}{DAGNodeName} = "\"B_C\"";
	$Attr{5}{Out} = "\"/$testname-dag_B_C.out\"";
	$Attr{5}{Err} = "\"/$testname-dag_B_C.err\"";
	$Attr{5}{DAGManJobId} = 50;
	$Attr{5}{UserLog} = "\"/$testname-dag-B.log\"";
	$Attr{5}{ClusterId} = 54;
	$Attr{5}{GlobalJobId} = "\"".$host."#".$Attr{5}{ClusterId}.".".$Attr{5}{ProcId}."#".$Attr{0}{QDate}."\"";

	$Attr{6}{Cmd} = "\"/condor_dagman\"";
	$Attr{6}{OnExitRemove} = "( ExitSignal =?= 11 || ( ExitCode =!= undefined && ExitCode >= 0 && ExitCode <= 2 ) )";
	$Attr{6}{RemoveKillSig} = "\"SIGUSR1\"";
	$Attr{6}{DAG_NodesReady} = 0;
	$Attr{6}{DAG_NodesDone} = 0;
	$Attr{6}{DAG_NodesTotal} = 3;
	$Attr{6}{DAG_Status} = 0;
	$Attr{6}{DAG_NodesFailed} = 0;
	$Attr{6}{DAG_NodesQueued} = 3;
	$Attr{6}{DAG_NodesPrerun} = 0;
	$Attr{6}{DAG_NodesPostrun} = 0;
	$Attr{6}{DAG_NodeUnready} = 0;
	$Attr{6}{DAG_InRecovery} = 0;
	$Attr{6}{JobUniverse} = 7;
	$Attr{6}{KillSig} = "\"SIGTERM\"";
	$Attr{6}{Out} = "\"/$testname-dag-B.dag.lib.out\"";
	$Attr{6}{Err} = "\"/$testname-dag-B.dag.lib.err\"";
	$Attr{6}{Args} = "\"-p 0 -f -l . -Lockfile cmd_q_shows-dag-B.dag.lock -AutoRescue 1 -DoRescueFrom 0 -Dag cmd_q_shows-dag-B.dag -Suppress_notification -CsdVersion \$CondorVersion:' '8.5.6' 'Jun' '15' '2016' 'BuildID:' '370652' 'PRE-RELEASE-UWCS' '$ -Verbose -Force -Notification never -Dagman /home/shuyang/Desktop/condor/release_dir/bin/condor_dagman -Update_submit\"";
	$Attr{6}{UserLog} = "\"/$testname-dag-B-dagman.log\"";
	$Attr{6}{EnvDelim} = "\";\"";
	$Attr{6}{OtherJobRemoveRequirements} = "\"DAGManJobId =?= 50\"";
	$Attr{6}{SubmitEventNotes} = "\"DAG Node: B\"";
	$Attr{6}{DAGNodeName} = "\"B\"";
	$Attr{6}{ClusterId} = 50;
	$Attr{6}{DAGManJobId} = 48;
	$Attr{6}{GlobalJobId} = "\"".$host."#".$Attr{6}{ClusterId}.".".$Attr{6}{ProcId}."#".$Attr{0}{QDate}."\"";

	$Attr{7}{Args} = "\"B_B 100\"";
	$Attr{7}{SubmitEventNotes} = "\"DAG Node: B_B\"";
	$Attr{7}{Cmd} = "\"/$testname-dag-q.sh\"";
	$Attr{7}{OnExitRemove} = "true";
	$Attr{7}{DAGNodeName} = "\"B_B\"";
	$Attr{7}{Out} = "\"/$testname-dag_B_B.out\"";
	$Attr{7}{Err} = "\"/$testname-dag_B_B.err\"";
	$Attr{7}{DAGManJobId} = 50;
	$Attr{7}{UserLog} = "\"/$testname-dag-B.log\"";
	$Attr{7}{ClusterId} = 53;
	$Attr{7}{GlobalJobId} = "\"".$host."#".$Attr{7}{ClusterId}.".".$Attr{7}{ProcId}."#".$Attr{0}{QDate}."\"";

	return %Attr;
}

sub add_globus_ads{
	my %Attr = %{$_[0]};
	my $total_cnt = scalar keys %Attr;
	my $host = hostname;
	for my $i (0..$total_cnt-1){
		$Attr{$i}{GlobusStatus} = 2; 
		$Attr{$i}{NiceUser} = "false";
	}
	return %Attr;
}

sub add_grid_ads {
	my %Attr = %{$_[0]};
	my $executable = $_[1];
	my $total_cnt = scalar keys %Attr;
	for my $i (0..$total_cnt-1){
		$Attr{$i}{GlobusStatus} = 2;
		$Attr{$i}{JobUniverse} = 9;
		$Attr{$i}{EC2RemoveVirtualMachineName} = "false";
		$Attr{$i}{GridJobId} = 
		$Attr{$i}{GridResource} = 
		$Attr{$i}{NiceUser} = "false";
		$Attr{$i}{ShouldTransferFiles} = "\"YES\"";
		$Attr{$i}{Requirements} = "true";
		$Attr{$i}{Arguments} = "\"\"";
		$Attr{$i}{StreamErr} = "false";
		$Attr{$i}{Cmd} = $executable;
		$Attr{$i}{StreamOut} = "false";
		$Attr{$i}{WantClaiming} = "false";
		$Attr{$i}{Managed} = "\"External\"";
		$Attr{$i}{GridResourceUnavailableTime} = $Attr{$i}{SeverTime}-6;
		$Attr{$i}{GridResource} = "\"condor condorce.example.com condorce.example.com:9619\"";
		$Attr{$i}{KillSig} = "\"SIGTERM\"";
	}
}

# function to change clusterId and GlobalJobId
# Usage: change_clusterId(<LIST_OF_JOB_ADS>,<NUMBER>)
sub change_clusterid{
	my %Attr = %{$_[0]};
	my $total_cnt = scalar keys %Attr;
	my $Id_num = $_[1];
	for my $k (0..($total_cnt-1)){
		$Attr{$k}{ClusterId} = $Id_num;
		$Attr{$k}{ProcId} = $k;
		$Attr{$k}{GlobalJobId} =~ s/#[0-9]+\./#$Id_num\./;
		$Id_num++;
	}
	return %Attr;
}

# function to mimic multiple owners, change owner
# Usage:multi_owner(\%Attr)
sub multi_owners{
	my %Attr = %{$_[0]};
	my $total_cnt = scalar keys %Attr;
	for my $k (0..($total_cnt-1)){
		$Attr{$k}{Owner} = substr($Attr{$k}{Owner}, 0, length($Attr{$k}{Owner})-1).$k."\"";
	}
	return %Attr;
}

sub write_ads {
	my %Attr = %{$_[0]};
	my $testname = $_[1];
	# write the result of print hash to
	my $fname = "$testname.simulated.ads";
	open(FH, ">$fname") || print "ERROR writing to file $fname";
	foreach my $k1 (sort keys %Attr){
		foreach my $k2 (keys %{ $Attr{$k1}}) {
			print FH $k2," = ", $Attr{$k1}{$k2}, "\n";
		}
		print FH "\n";
	}
	close(FH);
	return $fname;
}

sub read_array_content_to_table {
	my @table = @{$_[0]};
	my %other;
	my $cnt = 0;
	until ($cnt == scalar @table || $table[$cnt] =~ /--/){
		$other{$cnt} = $table[$cnt];
		$cnt++;
	}
	my $head_pos = $cnt+1;

	# read everything before the blank line or totals line and store them to $data
	$cnt = 0;
	my %data;
	while(defined $table[$head_pos+$cnt] && $table[$head_pos+$cnt]=~ /\S/ && $table[$head_pos+$cnt] !~ /[0-9]+\s+jobs;/){
		$data{$cnt} = $table[$head_pos+$cnt];
		$cnt++;
	}
	
	my $num = 0;
	if (defined $table[$head_pos + $cnt]) {
		$num = $head_pos + $cnt;
	}
	# read another line of summary
	for (; $num < (scalar @table); $num++) {
		if (($table[$num] =~/[0-9]+\s+jobs;\s+[0-9]+\s+completed,\s+[0-9]+\s+removed,\s+[0-9]+\s+idle,\s+[0-9]+\s+running,\s+[0-9]+\s+held,\s+[0-9]+\s+suspended/)) {
			last;
		}
	}
	my $arg = $table[$num];
	# for 8.7.2 new totals line, strip off the leading words "Total for <ident>:" so it looks (mostly) like the pre 8.7.2 summary line
	if (defined $arg && $arg =~ /^Total for/) { $arg =~ s/^Total for [^:]*:\s*//; }
	my @summary = defined $arg? (split / /, $arg) : " ";
	return (\%other,\%data,\@summary);
}

sub create_table {
	my %Attr = %{$_[0]};
	my $testname = $_[1];
	my $command_arg = $_[2];
	my $format_file;
	my @table;
	if (defined $_[3]){
		$format_file = $_[3];
	}
	# write the result of print hash to
	my $fname = "$testname.simulated.ads";
	open(FH, ">$fname") || print "ERROR writing to file $fname";
	foreach my $k1 (sort keys %Attr){
		foreach my $k2 (keys %{ $Attr{$k1}}) {
			print FH $k2," = ", $Attr{$k1}{$k2}, "\n";
		}
		print FH "\n";
	}
	close(FH);

	if ($command_arg eq "-pr"){
		@table = `condor_q -job $fname $command_arg $format_file`;
	} else {
		@table = `condor_q -job $fname $command_arg`;
	}
	print @table;
	my ($other_ref, $data_ref, $summary_ref) = read_array_content_to_table(\@table);	
	my %other = %{$other_ref};
	my %data = %{$data_ref};
	my @summary = @{$summary_ref};
	# read everything before -- and store them to $other
	return (\%other,\%data,\@summary);	
}		

sub read_status_output {
	my @content = @{$_[0]};
	my %table1;
	my %table2;
	my $index = 0;
	my $index1 = 0;
	my $num_blank = 0;
	for my $i (0..scalar @content -1){
		if ($num_blank < 2){
			if ($content[$i] =~ /\S/){
				$table1{$index} = $content[$i];
				$index++;
			} else {
				$num_blank++;
			}
		} else {
			last;
		}
	}
	for my $k (($index+$num_blank)..scalar @content-1){
		if ($content[$k] =~/\S/){
			$table2{$index1} = $content[$k];
			$index1++;
		}
	}
	return (\%table1,\%table2);
}

sub check_status {
	my @machine_info = @{$_[0]};
	my @summary = @{$_[1]};
	my $option = $_[2];
	my %Attr_old = %{$_[3]};
	my %Attr_new = %{$_[4]};
	my %rules_machine;
	my $result = 0;
	
	if ($option eq 'to_multi_platform' || $option eq 'xml' || $option eq 'json' || $option eq 'compact'){
		%rules_machine = 
(
# $i is $_[0]
'Name' => sub{
	my $machine_name = unquote($Attr_new{$_[0]-1}{Machine});
	if ($_[1] =~ /slot[0-9]+\@$machine_name/){
		return 1;
	} else {
		print "Output is $_[1]\nshould be slotxx\@$machine_name\n";
		return 0;
	}
},
'OpSys' => sub{
	my $machine_name = unquote($Attr_new{$_[0]-1}{Machine});
	if ($machine_name =~ /0[0-8]|1[6-9]/){
		return $_[1] eq 'LINUX';
	} elsif ($machine_name =~ /09|10|11/){
		return $_[1] eq 'WINDOWS';
	} elsif ($machine_name =~ /1[2-5]/){
		return $_[1] eq 'OSX';
	} else {
		return $_[1] eq "FREEBSD";
	}
},
'Arch' => sub{
	my $machine_name = unquote($Attr_new{$_[0]-1}{Machine});
	if ($machine_name =~ /07/){
		return $_[1] eq 'INTEL';
	} else {
		return $_[1] eq 'X86_64';
	}
},
'State' => sub{return $_[1] =~ /Unclaimed|Claimed/;},
'Activity' => sub{return $_[1] eq 'Idle';},
'LoadAv' => sub{
	my $loadavg = sprintf "%.3f",$Attr_new{$_[0]-1}{CondorLoadAvg};
	if ($_[1] eq $loadavg){
		return 1;
	} else {
		print "        ERROR: Output is $_[1], should be $loadavg";
		return 0;
	}
},
'Mem' => sub{
	if ($_[1] eq $Attr_new{$_[0]-1}{Memory}){
		return 1;
	} else {
		print "        ERROR: Output is $_[1], should be $Attr_old{($_[0]-1)%4}{Memory}";
		return 0;
	}
},
'ActvtyTime' => sub{
	my $convert_time;
	if (defined $Attr_new{$_[0]-1}{MyCurrentTime}){
		$convert_time = convert_unix_time($Attr_new{$_[0]-1}{MyCurrentTime}-$Attr_new{$_[0]-1}{EnteredCurrentActivity});
	} else {
		$convert_time = convert_unix_time($Attr_new{$_[0]-1}{LastHeardFrom}-$Attr_new{$_[0]-1}{EnteredCurrentActivity});
	}
	if ($_[1] eq $convert_time){
		return 1;
	} else {
		print "output is $_[1], should be $convert_time\n";
		return 0;
	}
},
'Platform' => sub {
	my $platform = unquote($Attr_new{$_[0]-1}{OpSysShortName});
	if ($_[1] =~ /$platform/) {
		return 1;
	} else {
		print "Error: Output is $_[1], should be x64/$platform\n";
		return 0;
	}
},
'Slots' => sub {
	return $_[1] eq $Attr_new{$_[0]-1}{NumDynamicSlots};
},
'Cpus' => sub {
	my $num = sprintf("%d",$Attr_new{$_[0]-1}{TotalCpus});
	if ($_[1] eq $num){return 1;}
	else {
		print "Error: output is $_[1], should be $num\n";
		return 0;
	}
},
'Gpus' => sub {
	my $num = sprintf("%d",$Attr_new{$_[0]-1}{TotalGpus});
	if ($_[1] eq $num) {
		return 1;
	} else {
		print "        output is $_[1]\n        should be $num\n";
		return 0;
	}
},
'TotalGb' => sub {
	my $num = sprintf("%.2f",($Attr_new{$_[0]-1}{TotalMemory}/1024));
#	return $_[1] eq $num;
	if ($_[1] eq $num) {
		return 1;
	} else {
		print "        output is $_[1]\n        should be $num\n";
		return 0;
	}
},
'FreCpu' => sub {
	my $num = sprintf("%d",$Attr_new{$_[0]-1}{Cpus});
	if ($_[1] eq $num) {
		return 1;
	} else {
		print "        output is $_[1]\n        should be $num\n";
		return 0;
	}
},
'FreeGb' => sub {
	my $num = sprintf("%.2f",($Attr_new{$_[0]-1}{Memory}/1024));
	if ($_[1] eq $num) {
		return 1;
	} else {
		print "        output is $_[1]\n        should be $num\n";
		return 0;
	}
},
'CpuLoad' => sub {
	if ($_[1] =~ /[0-9]+\.[0-9+]/) {
		return 1;
	} else {
		print "output is $_[1], should be a number\n";
		return 0;
	}
},
'ST' => sub {
	my $st = substr($Attr_new{$_[0]-1}{State},1,1).lc(substr($Attr_new{$_[0]-1}{Activity},1,1));
	if ($_[1] eq $st) {
		return 1;
	} else {
		print "        output is $_[1]\n        should be $st\n";
		return 0;
	}
},
'Jobs/Min' => sub {
	my $num = sprintf("%.2f",$Attr_new{$_[0]-1}{RecentJobStarts}/20);
	if ($_[1] eq $num) {
		return 1;
	} else {
		print "        output is $_[1]\n        should be $num\n";
		return 0;
	}
},
'MaxSlotGb' => sub {
	if (defined $Attr_new{$_[0]-1}{'Max(ChildMemory)'}){
		if ($_[1] eq sprintf("%.2f", $Attr_new{$_[0]-1}{'Max(ChildMemory)'})) {
			return 1;
		} else {
			print "        output is $_[1]\n        should be ".sprintf("%.2f", $Attr_new{$_[0]-1}{'Max(ChildMemory)'})."\n";
			return 0;
		}	
	} else {
		if ($_[1] eq '*') {
			return 1;
		} else {
			print "        output is $_[1]\n        should be *\n";
			return 0;
		}	
	}
}

);
		for my $i (0..(scalar @machine_info)-1){
			if (defined $machine_info[$i][1]){
				TLOG("Checking ".$machine_info[$i][0]."\n");
				if (defined $rules_machine{$machine_info[$i][0]}){
					for my $k (1..(scalar @{$machine_info[$i]})-1){
						unless ($rules_machine{$machine_info[$i][0]}($k,$machine_info[$i][$k])){
							return 0;
						}
					}
					print "        PASSED: $machine_info[$i][0] is correct.\n";
				}
			}
		}
		my %table = count_status_state(\%Attr_new, $option);
		my %table1;
		if ($option =~ /compact/){
			for my $key (sort keys %table){
				(my $new = $key) =~ s/X86_64/x64/;
				$table1{$new} = $table{$key};
			}		
			%table = %table1;
		}
		for my $i (1..scalar @{$summary[0]}-2){
			unless ($summary[0][$i] eq (sort keys %table)[$i-1]){
				print "        ERROR: output is $summary[0][$i], should be ".(sort keys %table)[$i-1]."\n";
				return 0;
			}
		}
		if ($summary[0][scalar @{$summary[0]}-1] ne 'Total'){
			print "        ERROR: there should be a 'total' row";
			return 0;
		}
		my $sum;
		for my $i (1..(scalar @summary)-1){
			if (defined $summary[$i][1]){
				TLOG("Checking ".$summary[$i][0]."\n");
				$sum = 0;
				for my $k (1..(scalar @{$summary[0]})-2){
					unless ($table{$summary[0][$k]}{"$summary[$i][0]"} eq $summary[$i][$k]){
						print "       ERROR: output is $summary[$i][$k], should be ".$table{$summary[0][$k]}{"$summary[$i][0]"}."\n";
						return 0;
					} else {
						$sum+=$summary[$i][$k];
					}
				}
				if ($sum ne $summary[$i][scalar @{$summary[0]}-1]){
					print "        ERROR: columns doesn't add upp correctly\n";
					return 0;
				}
			}
		}
		return 1; 
	}
	if ($option eq 'states'){
		TLOG("Checking if states are displayed correctly\n");
		my $counter = 0;
		my $machine_name;
		for my $i (1..scalar @{$machine_info[0]}-1){
			$machine_name = $Attr_new{$i-1}{Machine};
			if ($machine_name =~ /01/){
				$counter++; 
				unless ($machine_info[3][$i] eq 'Unclaimed'){return 0;}}
			if ($machine_name =~ /02/){unless ($machine_info[3][$i] eq 'Claimed'){return 0;}}
			if ($machine_name =~ /03/){unless ($machine_info[3][$i] eq 'Matched'){return 0;}}
			if ($machine_name =~ /04/){unless ($machine_info[3][$i] eq 'Preempting'){return 0;}}
			if ($machine_name =~ /05/){unless ($machine_info[3][$i] eq 'Backfill'){return 0;}}
		}		
		if ($summary[1][1] eq scalar (@{$machine_info[0]})-1 && $summary[1][2] eq (@{$machine_info[0]})-1){
			for my $i (3..7){
				for my $j (1..2){
					unless ($summary[$i][$j] eq $counter){return 0;}
				}
			}
			return 1;
		} else {
			return 0;
		}
	}
	if ($option eq 'avail'){
		TLOG("Checking if unclaimed machines are displayed correctly\n");
		my $counter = 0;
		for my $i (1..(scalar @{$machine_info[0]})-1){
			if (!($machine_info[0][$i] =~ /01|06/)){
				return 0;
			} else {$counter++;}
		}
		if ($summary[1][1] eq $counter && $summary[1][2] eq $counter){
			for my $j (1..2){
				unless ($summary[4][$j] eq $counter){return 0;}
			}
			return 1;
		} else {
			return 0;
		}
	}
	if ($option eq 'claimed'){
		TLOG("Checking if claimed machines are displayed correctly\n");
		my %rules = (
'Name' => sub {
	return $_[1] =~ /02/;},
'OpSys' => sub {return $_[1] eq 'LINUX';},
'Arch' => sub {return $_[1] eq 'X86_64';},
'LoadAv' => sub {
	my $loadavg = sprintf "%.3f",$Attr_old{$_[0]-1}{CondorLoadAvg};
	return $_[1] eq $loadavg;},
'RemoteUser' => sub {return $_[1] eq "foo\@cs.wisc.edu";},
'ClientMachine' => sub {
	my $machine_name = unquote($Attr_new{$_[0]-1}{Machine});
	return ($_[1] eq $machine_name || $_[1] =~/exec02/);},
'Machines' => sub {
	my $num = (scalar @{$machine_info[0]})-1;
	print "$num\n";
	return $_[1] =~ /$num/},
'MIPS' => sub {
	if (defined $Attr_old{0}{Mips}){
		return $_[1] eq $Attr_old{$_[0]-1}{Mips};
	} else {
		return $_[1] eq 0;
	}
},
'KFLOPS' => sub {
	if (defined $Attr_old{0}{KFlops}){
		return $_[1] eq $Attr_old{$_[0]-1}{KFlops};
	} else {
		return $_[1] eq 0;
	}
},
'AvgLoadAvg' => sub {
	my $num = 0;
	for my $i (1..(scalar @{$machine_info[0]})-1){$num += $machine_info[3][$i];}
	$num = sprintf "%.3f", $num/((scalar @{$machine_info[0]})-1);
	return $_[1] eq $num;
}	
	);

		for my $i (0..(scalar @machine_info)-1){
			if (defined $machine_info[$i][1]){
				TLOG("Checking $machine_info[$i][0]\n");
				for my $k (1..(scalar @{$machine_info[$i]})-1){
					unless ($rules{$machine_info[$i][0]}($k,$machine_info[$i][$k])){
						print "        FAILED: output is $machine_info[$i][$k]\n";
						return 0;
					}
				}
				print "        PASSED: $machine_info[$i][0] is correct\n";
			}
		}
		for my $i (0..3){
			if ($summary[$i][0] ne ""){
				TLOG("Checking $summary[$i][0]\n");
				for my $k (1..2){				
					unless ($rules{$summary[$i][0]}($k,$summary[$i][$k])){
						return 0;
					}
				}
				print "       PASSED: $summary[$i][0] is correct\n";
			}
		}
		return 1;
	}
	if ($option eq "constraint"){
		TLOG("Checking if only shows constriant output\n");
		my $counter = 0;
		for my $i (1..(scalar @{$machine_info[0]})-1){
			if ($machine_info[4][$i] ne 'Busy'){
				print "        FAILED: Output is $machine_info[3][$i], should be Busy\n";
				return 0;
			} else {$counter++;}
		}
		if ($summary[1][1] ne $counter || $summary[1][2] ne $counter){
			print "        FAILED: Output is wrong, should be 12\n";
			return 0;
		} else {
			return 1;
		}
	}
	if ($option eq "sort"){
		my $num1;
		my $num2;
		for my $i (1..(scalar @{$machine_info[0]})-2){
			$num1 = ord(substr($machine_info[3][$i],0,1));
			$num2 = ord(substr($machine_info[3][$i+1],0,1));
			if ($num1 > $num2){
				print "        FAILED: Sorting unsuccessful\n";
				return 0;
			}
		}
		return 1;
	}
	if ($option eq "total"){
		TLOG("Checking the summary table\n");
		if ($summary[1][1] ne (scalar keys %Attr_new) || $summary[1][2] ne (scalar keys %Attr_new)){
			print "       FAILED: Total is not correct\n";
			return 0;
		} else {
			for my $i (3..7){
				if ($summary[$i][1] ne (scalar keys %Attr_new)/5 || $summary[$i][2] ne (scalar keys %Attr_new)/5){
					print "        FAILED: Total of different state is incorrect\n";
					return 0;
				}
			}
		}
		return 1;
	}
	if ($option eq 'run'){
		for my $i (0..(scalar @{$machine_info[0]}-2)){unless ($machine_info[0][$i+1] eq unquote($Attr_new{$i}{Name})){
			print "Error: output is $machine_info[0][$i+1], should be ".unquote($Attr_new{$i}{Name})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[1]}-2)){unless ($machine_info[1][$i+1] eq unquote($Attr_new{$i}{TotalJobAds})){
			print "Error: output is $machine_info[1][$i+1], should be ".unquote($Attr_new{$i}{TotalJobAds})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[2]}-2)){unless ($machine_info[2][$i+1] eq unquote($Attr_new{$i}{ShadowsRunning})){
			print "Error: output is $machine_info[2][$i+1], should be ".unquote($Attr_new{$i}{ShadowsRunning})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[3]}-2)){unless ($machine_info[3][$i+1] eq unquote($Attr_new{$i}{TotalSchedulerJobsRunning})){
			print "Error: output is $machine_info[3][$i+1], should be ".unquote($Attr_new{$i}{TotalSchedulerJobsRunning})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[4]}-2)){unless ($machine_info[4][$i+1] eq unquote($Attr_new{$i}{TotalSchedulerJobsIdle})){
			print "Error: output is $machine_info[4][$i+1], should be ".unquote($Attr_new{$i}{TotalSchedulerJobsIdle})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[5]}-2)){unless ($machine_info[5][$i+1] eq unquote($Attr_new{$i}{RecentJobsCompleted})){
			print "Error: output is $machine_info[5][$i+1], should be ".unquote($Attr_new{$i}{RecentJobsCompleted})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[6]}-2)){unless ($machine_info[6][$i+1] eq unquote($Attr_new{$i}{RecentJobsStarted})){
			print "Error: output is $machine_info[6][$i+1], should be ".unquote($Attr_new{$i}{RecentJobsStarted})."\n";
			return 0;}}
		print "        Display of $option is correct!\n";
		return 1;
	}
	if ($option eq 'schedd'){
		my $run_cnt = 0;
		my $idle_cnt = 0;
		my $held_cnt = 0;
		for my $i (0..(scalar @{$machine_info[0]}-2)){unless ($machine_info[0][$i+1] eq unquote($Attr_new{$i}{Name})){
			print "Error: output is $machine_info[0][$i+1], should be ".unquote($Attr_new{$i}{Name})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[1]}-2)){unless ($machine_info[1][$i+1] eq unquote($Attr_new{$i}{Machine})){
			print "Error: output is $machine_info[1][$i+1], should be ".unquote($Attr_new{$i}{Machine})."\n";
			return 0;}}
		for my $i (0..(scalar @{$machine_info[2]}-2)){unless ($machine_info[2][$i+1] eq unquote($Attr_new{$i}{TotalRunningJobs})){
			print "Error: output is $machine_info[2][$i+1], should be ".unquote($Attr_new{$i}{TotalRunningJobs})."\n";
			return 0;} else{$run_cnt += unquote($Attr_new{$i}{TotalRunningJobs})}}
		for my $i (0..(scalar @{$machine_info[3]}-2)){unless ($machine_info[3][$i+1] eq unquote($Attr_new{$i}{TotalIdleJobs})){
			print "Error: output is $machine_info[3][$i+1], should be ".unquote($Attr_new{$i}{TotalIdleJobs})."\n";
			return 0;} else{$idle_cnt += unquote($Attr_new{$i}{TotalIdleJobs})}}
		for my $i (0..(scalar @{$machine_info[4]}-2)){unless ($machine_info[4][$i+1] eq unquote($Attr_new{$i}{TotalHeldJobs})){
			print "Error: output is $machine_info[4][$i+1], should be ".unquote($Attr_new{$i}{TotalHeldJobs})."\n";
			return 0;} else{$held_cnt += unquote($Attr_new{$i}{TotalHeldJobs})}}
		if ($summary[1][1] =~ /$run_cnt/ && $summary[2][1] eq $idle_cnt && $summary[3][1] eq $held_cnt){
			print "        Display of $option is correct!\n";
			return 1;
		} else {
			print "        Error: summary not correct\n";
			return 0;}
	}
	if ($option eq 'negotiator'){
		for my $i (1..(scalar @{$machine_info[0]})-1){
			unless ($machine_info[0][$i] eq unquote($Attr_new{$i-1}{Name})) {
				print "        Output is $machine_info[0][$i]\n        should be ".unquote($Attr_new{$i-1}{Name})."\n";
				return 0;
			}
			my ($date, $hour) = convert_timestamp_date_hour_min($Attr_new{$i-1}{LastNegotiationCycleEnd0});
			unless ($machine_info[1][$i] =~ /$date\s+$hour/) {
				print "        Output is $machine_info[1][$i]\n        should be $date $hour\n";
				return 0;
			}
			unless ($machine_info[2][$i] eq unquote($Attr_new{$i-1}{LastNegotiationCycleDuration0})) {
				print "        Output is $machine_info[2][$i]\n        should be ".unquote($Attr_new{$i-1}{LastNegotiationCycleDuration0 })."\n";
				return 0;
			}
			unless ($machine_info[3][$i] eq unquote($Attr_new{$i-1}{LastNegotiationCycleCandidateSlots0})) {
				print "        Output is $machine_info[3][$i]\n        should be ".unquote($Attr_new{$i-1}{LastNegotiationCycleCandidateSlots0})."\n";
				return 0;
			}
			unless ($machine_info[4][$i] eq unquote($Attr_new{$i-1}{LastNegotiationCycleActiveSubmitterCount0})) {
				print "        Output is $machine_info[4][$i]\n        should be ".unquote($Attr_new{$i-1}{LastNegotiationCycleActiveSubmitterCount0})."\n";
				return 0;
			}
			unless ($machine_info[5][$i] eq unquote($Attr_new{$i-1}{LastNegotiationCycleNumSchedulers0})) {
				print "        Output is $machine_info[5][$i]\n        should be ".unquote($Attr_new{$i-1}{LastNegotiationCycleNumSchedulers0})."\n";
				return 0;
			}
			unless ($machine_info[6][$i] eq unquote($Attr_new{$i-1}{LastNegotiationCycleNumIdleJobs0})) {
				print "        Output is $machine_info[6][$i]\n        should be ".unquote($Attr_new{$i-1}{LastNegotiationCycleNumIdleJobs0})."\n";
				return 0;
			}
			unless ($machine_info[7][$i] eq unquote($Attr_new{$i-1}{LastNegotiationCycleMatches0})) {
				print "        Output is $machine_info[7][$i]\n        should be ".unquote($Attr_new{$i-1}{LastNegotiationCycleMatches0})."\n";
				return 0;
			}
			unless ($machine_info[8][$i] eq unquote($Attr_new{$i-1}{LastNegotiationCycleRejections0})) {
				print "        Output is $machine_info[8][$i]\n        should be ".unquote($Attr_new{$i-1}{LastNegotiationCycleRejections0})."\n";
				return 0;
			}
		}
		return 1;
	}
}

sub convert_timestamp_date_hour_min {
	my $timestamp = $_[0];
	my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime($timestamp);
	$mon = sprintf("%d",$mon + 1);
	$mday = sprintf("%d", $mday);
	$hour = sprintf("%02d", $hour);
	$min = sprintf("%02d", $min);
	return ("$mon/$mday", "$hour:$min");
}

sub how_many_entries {
	my @content = @{$_[0]};
	my $num = $_[1];
	my $index = 0;
	for my $i (0..(scalar @content)-1){
		if ($content[$i] =~ /\S/){
			if ($content[$i] =~ /[0-9]+ jobs;/) {
				# don't count totals lines as entries.
			} else {
				$index++;
			}
		}
	}
	# -2 to account for title and headings
	return ($index - 2);
}


sub count_status_state {
	my %Attr = %{$_[0]};
	my $option = $_[1];
	my %Arch;
	my $arch_os_pair;
	if ($option =~ /compact/){
		for my $i (0.. (scalar keys %Attr) -1){
			$arch_os_pair = substr($Attr{$i}{Arch},1,length($Attr{$i}{Arch})-2)."/".substr($Attr{$i}{OpSysShortName},1,length($Attr{$i}{OpSysShortName})-2);
			unless (defined $Arch{$arch_os_pair}){
				$Arch{$arch_os_pair} = 0;
			}
		}
	} else {
		for my $i (0.. (scalar keys %Attr) -1){
			$arch_os_pair = substr($Attr{$i}{Arch},1,length($Attr{$i}{Arch})-2)."/".substr($Attr{$i}{OpSys},1,length($Attr{$i}{OpSys})-2);
			unless (defined $Arch{$arch_os_pair}){
				$Arch{$arch_os_pair} = 0;
			}
		}
	}
	my %table;
	foreach my $key (sort keys %Arch){
		$table{$key}{"Owner"} = 0;
		$table{$key}{"Claimed"} = 0;
		$table{$key}{"Unclaimed"} = 0;
		$table{$key}{"Matched"} = 0;
		$table{$key}{"Preempting"} = 0;
		$table{$key}{"Backfill"} = 0;
		$table{$key}{"Drain"} = 0;
		for my $i (0..(scalar keys %Attr) -1){
			if ($key eq substr($Attr{$i}{Arch},1,length($Attr{$i}{Arch})-2)."/".substr($Attr{$i}{OpSysShortName},1,length($Attr{$i}{OpSysShortName})-2) || $key eq substr($Attr{$i}{Arch},1,length($Attr{$i}{Arch})-2)."/".substr($Attr{$i}{OpSys},1,length($Attr{$i}{OpSys})-2)){
				$table{$key}{substr($Attr{$i}{State},1,length($Attr{$i}{State})-2)}++;
			}
		}
	$table{$key}{"Total"} = $table{$key}{"Owner"} + $table{$key}{"Claimed"} + $table{$key}{"Unclaimed"} + $table{$key}{"Matched"} + $table{$key}{"Preempting"} + $table{$key}{"Backfill"} + $table{$key}{"Drain"};		
	}
	return %table;
}

sub convert_unix_time {
	my $total = $_[0];
	my $days = int($total/86400);
	my $hours = sprintf "%02d", int(($total-$days*86400)/3600);
	my $minutes = sprintf "%02d", int(($total - $days*86400 - $hours*3600)/60);
	my $seconds = sprintf "%02d", $total - $days*86400 - $hours*3600 - $minutes*60;
	return "$days+$hours:$minutes:$seconds";
}

# function to find the blank column numbers from a hash
# input: a hash
# return: a hash of empty column numbers
sub find_blank_columns{
	my %lines = %{$_[0]};
	my %blank_col;
	my @head_chars = split ("", $lines{0});
	for my $h (0..((scalar @head_chars)-1)){
		if ($head_chars[$h] =~/\s/){
			$blank_col{$h} = $h;           # all the blank column numbers from the FIRST line stored in %blank_col
		}
	}
	for my $i (1..((scalar keys %lines)-1)){            # traverse the arrays and find out whether the following non-blank column number exists
		my @chars = split "", $lines{$i};         # if so delete from %blank_col
			for my $k (0..((scalar @chars)-1)){
				if ($chars[$k] =~ /\S/ && defined $blank_col{$k}){
					delete $blank_col{$k};
				}
			}
	}
	return %blank_col;
}

# function to find edges of columns from a hash, aka where the data starts and ends, and split the hash to fields
# input: a hash
# return: an array of arrays with fields and corresponding data
sub split_fields {

# find the column edges
	my %lines = %{$_[0]};
	my $end_num; # keep track of where the table ends
	my %blank_col;
	my @head_chars = split ("", $lines{0});
	$end_num = (scalar @head_chars)-1;
	my $head_end_num = $end_num; # index of where heading ends
	for my $h (0..$end_num){
		if ($head_chars[$h] =~/\s/){
			$blank_col{$h} = $h;           # all the blank column numbers from the FIRST line stored in %blank_col
		}
	}
	if (scalar keys %lines > 1){
		for my $i (1..((scalar keys %lines)-1)){            # traverse the arrays and find out whether the following non-blank column number exists
			my @chars = split "", $lines{$i};         # if so delete from %blank_col
			$end_num = (scalar @chars)-1 > $end_num? (scalar @chars-1):$end_num;
			for my $k (0..((scalar @chars)-1)){
				if ($chars[$k] =~ /\S/ && defined $blank_col{$k}){
					delete $blank_col{$k};
				}
			}
		}
		my @sorted_keys = sort {$a <=> $b} keys %blank_col;
		my @edges;
		my $index;
		if (!($sorted_keys[0] == 0)){
			$edges[0][0] = 0;
			$edges[0][1] = $sorted_keys[0]-1;
			$index = 1;
		} else {
			$index = 0;
		}
		for my $k (0..scalar @sorted_keys-2){
			if ($sorted_keys[$k+1]-$sorted_keys[$k]!= 1){
				$edges[$index][0] = $sorted_keys[$k]+1;
				$edges[$index][1] = $sorted_keys[$k+1]-1;
				$index++;
			}
		}
		if ($sorted_keys[(scalar @sorted_keys)-1] != $end_num){
			$edges[$index][0] = $sorted_keys[(scalar @sorted_keys)-1];
			$edges[$index][1] = $end_num;
		} else {
			$index--;
		}
# start to split fields
# two dimensional arrays $fields[$i][$j] where $i refers the column number and $j refers to data.
# $j=0 --> heading names. $j>0 --> corresponding data
		my @fields;
		for my $i (0..$index){
			$fields[$i][0] = trim(join("",@head_chars[$edges[$i][0]..($edges[$i][1]<$head_end_num? $edges[$i][1]:$head_end_num)]));
# to avoid the last edge go beyong where heading actually ends
			for my $j (1..((scalar keys %lines)-1)){
				my @chars = split "",$lines{$j};
				if ($edges[$i][1] > (scalar @chars)-1){
					$fields[$i][$j] = trim(join("", @chars[$edges[$i][0]..(scalar @chars)-1]));
				} else {
					$fields[$i][$j] = trim(join("", @chars[$edges[$i][0]..$edges[$i][1]]));
				}
			}
		}
		if ($fields[(scalar @fields)-1][0] eq ''){
			@fields = @fields[0..(scalar @fields) -2];
		}
		return @fields;
	} else {
		my @fields;
		my @sorted_keys = sort {$a <=> $b} keys %blank_col;
		my @edges;
		my $index = 0;
		for my $k (0..scalar @sorted_keys-2){
			if ($sorted_keys[$k+1]-$sorted_keys[$k]!= 1){
				$edges[$index][0] = $sorted_keys[$k]+1;
				$edges[$index][1] = $sorted_keys[$k+1]-1;
				$index++;
			}
		}
		$edges[$index][0] = $sorted_keys[(scalar @sorted_keys)-1]+1;
		$edges[$index][1] = $end_num;

		for my $i (0..$index){
			$fields[$i][0] = trim(join("",@head_chars[$edges[$i][0]..($edges[$i][1]<$head_end_num? $edges[$i][1]:$head_end_num)]));
		}
		return @fields;
	}
}
	
# function to calculate from raw value to actual value
# Usage: cal_from_raw(<RAW_VALUE>);
sub cal_from_raw{
	my $fvalue = shift;
	my $magnitude = int(log(abs($fvalue/5))/log(10)+1);
	$magnitude = int($magnitude);
	my $roundto = (POSIX::pow(10,$magnitude))*0.25;
	my $answer = (POSIX::ceil($fvalue/$roundto))*$roundto;
	return $answer;
}

# function to trim white spaces at the begining and the end of the string
# Reference: perlmaven.com/trim
sub trim{
	my $s = shift;
	$s =~ s/^\s*|\s*$//g;
	return $s;
}

# count how many jobs are in each state seperately, and the total number of jobs, 
# stored in %cnt_num
sub count_job_states {

	defined $_[0] || print "USAGE of count_job_states: count_job_states(<LIST_OF_JOB_ADS>)\n";
	my %Attr = %{$_[0]};
	my $idle_cnt = 0;
	my $run_cnt = 0;
	my $hold_cnt = 0;
	my $cmplt_cnt = 0;
	my $rmd_cnt = 0;
	my $suspnd_cnt = 0;
	my $total_cnt = scalar keys %Attr;
	
	my %cnt_num = (0 => $total_cnt, 1 => $idle_cnt, 2 => $run_cnt, 3 => $rmd_cnt, 4 => $cmplt_cnt, 5 => $hold_cnt, 6 => $suspnd_cnt);

	for my $i (0..$total_cnt-1){
		$cnt_num{$Attr{$i}{JobStatus}}++;
	}
	return %cnt_num;
}

sub make_batch{
	my %Attr = %{$_[0]};
	my $total_cnt = scalar keys %Attr;
	my %cluster_batch;
	my %user_batch;
	my $job_ids;
	for my $i (0..$total_cnt-1){
		if (!defined $cluster_batch{$Attr{$i}{ClusterId}}){
			my $key = $Attr{$i}{ClusterId};
			$cluster_batch{$key} = $i;
		}
	}
	foreach my $k (sort keys %cluster_batch){
		my $first_id = 10000000;
		my $last_id = 0;
		for my $j (0..$total_cnt-1){
			$Attr{$j}{GlobalJobId} =~ /.+#.+\.(.+)#.+/;
			my $temp = $1;
			if ($Attr{$j}{ClusterId}==$k){
				if ($temp > $last_id){
					$last_id = $temp;
				}	
				if ($temp < $first_id){
					$first_id = $temp;
				}
			}	
		}
		my $first_user = $Attr{$first_id}{Owner};
		if ($first_id != $last_id){
			$job_ids = $k.'.'.$first_id.'-'.$last_id;
		} else {
			$job_ids = $k.'.'.$first_id;
		}
		$cluster_batch{$k}=$job_ids;
		$user_batch{$k} = $first_user;

	}
#foreach my $k (sort keys %cluster_batch){print "$k = $cluster_batch{$k}\n";}
	return (\%cluster_batch,\%user_batch);
}

sub various_hold_reasons{
	my %Attr = %{$_[0]};
	my @hold_reasons = (1,15,3,21);
	my $nn = 0;
	for my $i (0..(scalar keys %Attr)-1){
		if ($Attr{$i}{JobStatus} == 5){
			if ($nn < scalar(@hold_reasons)){
				$Attr{$i}{HoldReasonCode} = $hold_reasons[$nn];
				$nn++;
			} else {
				$Attr{$i}{HoldReasonCode} = $hold_reasons[0];
				$nn = 0;
			}
		}
	}
	return %Attr;
}

sub check_if_same_cluster {
	my %Attr = %{$_[0]};
	for my $i (1.. (scalar keys %Attr)-1){
		if ($Attr{$i}{ClusterId} ne $Attr{0}{ClusterId}){
			return 0;
		}
	}
	return 1;
}

sub find_real_heading {
	my $command_arg = $_[0];
	my $real_heading;

	# no command arg -> -batch
	if ($command_arg eq '') {
		$real_heading = '-batch';
	# if there are three command arguments, one has to be -nobatch andone has to be -wide
	} elsif (defined $command_arg && $command_arg =~ /(.+)\s+(.+)\s+(.+)/){
		if (substr($1,0,5) ne '-wide' && $1 ne '-nobatch'){
			$real_heading = $1;
		}
		if (substr($2,0,5) ne '-wide' && $2 ne '-nobatch'){
			$real_heading = $2;
		}	
		if (substr($3,0,5) ne '-wide' && $3 ne '-nobatch'){
			$real_heading = $3;
		}
	# if there are two command arguments, one can be -wide or -nobatch
	} elsif (defined $command_arg && $command_arg =~ /(.+)\s+(.+)/){
		if (('-nobatch' eq $1 || '-nobatch' eq $2) && ('-wide' eq substr($1,0,5) || '-wide' eq substr($2,0,5))){
			$real_heading = '-nobatch';
		} elsif ($1 eq '-nobatch'){
			$real_heading = $2;
		} elsif ($2 eq '-nobatch'){
			$real_heading = $1;
		} elsif (substr($1,0,5) eq '-wide' && $2 ne '-dag' && $2 ne '-run'){
			$real_heading = $2;
		} elsif (substr($2,0,5) eq '-wide' && $1 ne '-dag' && $1 ne '-run'){
			$real_heading = $1;
		} else {
			$real_heading = '-batch';
		}	
	# one command argument, -dag and -run same with -batch
	} elsif (defined $command_arg){
		if (substr($command_arg,0,5) eq '-wide' || $command_arg eq '-run' || $command_arg eq '-dag'){
			$real_heading = '-batch';
		} else {
			$real_heading = $command_arg;
		}
	}
	return $real_heading;
}

sub check_heading {
	my $command_arg = $_[0];
	my %data = %{$_[1]};
	my $real_heading = find_real_heading($command_arg);
	my %rules_heading = (
		'-batch' => sub{ if ($command_arg =~ /-run/){
			return $data{0} =~ /\s*OWNER\s+BATCH_NAME\s+SUBMITTED\s+DONE\s+RUN\s+IDLE\s+TOTAL\s/;} else {
			return $data{0} =~ /\s*OWNER\s+BATCH_NAME\s+SUBMITTED\s+DONE\s+RUN\s+IDLE\s+HOLD\s+TOTAL\s/;}
},
		'-nobatch' => sub {return $data{0} =~ /(\s*)ID(\s+)OWNER(\s+)SUBMITTED(\s+)RUN_TIME(\s+)ST(\s+)PRI(\s+)SIZE(\s+)CMD/;},
		'-run' => sub {return $data{0} =~ /(\s*)ID(\s+)OWNER(\s+)SUBMITTED(\s+)RUN_TIME(\s+)HOST\(S\)/;},
		'-hold' => sub {return $data{0} =~ /\s*ID\s+OWNER\s+HELD_SINCE\s+HOLD_REASON/;},
		'-dag' => sub {return $data{0} =~ /\s*ID\s+OWNER\/NODENAME\s+SUBMITTED\s+RUN_TIME\s+ST\s+PRI\s+SIZE\s+CMD/;},
		'-io' => sub {return $data{0} =~ /\s*ID\s+OWNER\s+RUNS\s+ST\s+INPUT\s+OUTPUT\s+/;},
		'-globus' => sub {return $data{0} =~ /\s*ID\s+OWNER\s+STATUS\s+MANAGER\s+HOST\s+EXECUTABLE/;},
		'-tot' => sub {print "-tot does not have a heading\n";return 1;},
		'status_machine' => sub {return $data{0} =~ /\s*Name\s+OpSys\s+Arch\s+State\s+Activity\s+LoadAv\s+Mem\s+ActvtyTime/;},
		'status_summary' => sub {return $data{0} =~ /\s+Total\s+Owner\s+Claimed\s+Unclaimed\s+Matched\s+Preempting\s+Backfill\s+Drain/;},
		'status_claimed_machine' => sub {return $data{0} =~ /\s*Name\s+OpSys\s+Arch\s+LoadAv\s+RemoteUser\s+ClientMachine/;},
		'status_claimed_summary' => sub {return $data{0} =~ /\s*Machines\s+MIPS\s+KFLOPS\s+AvgLoadAvg/;},
		'status_compact_machine' => sub {return $data{0} =~ /\s*Machine\s+Platform\s+Slots\s+Cpus\s+Gpus\s+TotalGb\s+FreCpu\s+FreeGb\s+CpuLoad\s+ST\s+Jobs\/Min\s+MaxSlotGb/;},
		'status_run_machine' => sub {return $data{0} =~ /\s*Name\s+TotalJobs\s+Shadows\s+ActiveDAGs\s+IdleDAGs\s+RcntDone\s+RcntStart/;},
		'status_schedd_machine' => sub {return $data{0} =~ /\s*Name\s+Machine\s+RunningJobs\s+IdleJobs\s+HeldJobs/;},
		'status_schedd_summary' => sub {return $data{0} =~ /\s*TotalRunningJobs\s+TotalIdleJobs\s+TotalHeldJobs/;},
		'status_negotiator' => sub {return $data{0} =~ /\s*Name\s+LastCycleEnd\s+\(Sec\)\s+Slots\s+Submitrs\s+Schedds\s+Jobs\s+Matches\s+Rejections/;},
		'status_server_machine' => sub {return $data{0} =~ /\s*Machine\s+Platform\s+Slots\s+Cpus\s+Gpus\s+TotalGb\s+Mips\s+KFlops\s+CpuLoad\s+ST\s+Jobs\/Min/;},
		'status_server_summary' => sub {return $data{0} =~ /\s*Machines\s+Avail\s+Memory\s+Disk\s+MIPS\s+KFLOPS/;}
	);

	TLOG("Checking heading format of the output file\n");
	if ($rules_heading{$real_heading}()){
		print "        PASSED: Headings are correct\n";
		return 1;
	} else{
		print "        ERROR: Headings are not correct\n";
		return 0;
	}

}

sub check_data {
	my @fields = @{$_[0]};
	my %Attr = %{$_[1]};
	my $command_arg = $_[2];
	my %cluster_batch = %{$_[3]};
	my %user_batch = %{$_[4]};
	my $real_heading = find_real_heading($command_arg);
	my $same_cluster = check_if_same_cluster(\%Attr);
	my %st_num = (1 => 'I', 2=>'R', 3=>'X', 4=>'C', 5=> 'H', 6=>'>');
	my %rules_data = (
'ID' => sub {
	if (defined $real_heading && $real_heading ne '-dag'){
		if ($Attr{$_[0]}{GlobalJobId} =~ /.+#(.+)#.+/){
			return $_[1] eq $1;
		} else {
			return 0;
		}
	} else {
		my @dag_id_list = qw(48.0 49.0 50.0 52.0 55.0 53.0 54.0 51.0);
		return $_[1] eq $dag_id_list[$_[0]];
	}
},
'OWNER' => sub {
	if (defined $real_heading && $real_heading ne '-batch'){
		return substr($Attr{$_[0]}{Owner},1, length($Attr{$_[0]}{Owner})-2) eq $_[1];
	} else {
		return 1
	}
},
'OWNER/NODENAME' => sub {
	$fields[0][$_[0]+1] =~ /(.+)\.(.+)/;
	my $temp_id = $1;
	my $temp_node;
	if ($_[1] =~ /\|-(.+)/){
		$temp_node = $1;
	}
	for my $key (0..((scalar keys %Attr)-1)){
		if ($Attr{$key}{ClusterId} eq $temp_id && defined $temp_node){
			return $temp_node eq substr($Attr{$key}{DAGNodeName},1, length($Attr{$key}{DAGNodeName})-2);
		}elsif($Attr{$key}{ClusterId} eq $temp_id && !defined $temp_node){
			return $_[1] eq substr($Attr{$key}{Owner},1,length($Attr{$key}{Owner})-2);
		}
	}
},
'SUBMITTED' => sub{return $_[1]  =~ /[0-9]+\/[0-9]+\s+[0-9]+:[0-9]+/;},
'RUN_TIME' => sub{return $_[1] =~ /[0-9]\+[0-9][0-9]:[0-9][0-9]:[0-9][0-9]/;},
'ST' => sub{
	if ($real_heading ne '-dag' && $real_heading ne '-io'){
		if (defined $st_num{$Attr{$_[0]}{JobStatus}} && $_[1] eq $st_num{$Attr{$_[0]}{JobStatus}}){
			return 1;
		} else {
			print "        ERROR: JobStatus is $Attr{$_[0]}{JobStatus}, should be $_[1].\n";
			return 0;
		}
	} elsif ($real_heading eq '-io') {
		if ($Attr{$_[0]}{TransferQueued} eq 'true'){
			if ($Attr{$_[0]}{TransferringOutput} eq 'true'){
				return $_[1] eq 'q>';
			} elsif ($Attr{$_[0]}{TransferringInput} eq 'true'){
				return $_[1] eq '<q'
			} else {
				return $_[1] eq 'q>';
			}
		} else {
			if ($Attr{$_[0]}{TransferringOutput} eq 'true'){
				return $_[1] eq '>';
			} else {
				return $_[1] eq '<';
			}
		}
	} else {
		$fields[0][$_[0]+1] =~ /(.+)\..+/;
		my $temp_id = $1;
		for my $k (0..(scalar keys %Attr)-1){
			if ($Attr{$k}{ClusterId} eq $temp_id){
				return $_[1] eq $st_num{$Attr{$k}{JobStatus}};
			}
		}
		return 0;
	}
},
'HOST' => sub {return $_[1] eq substr($Attr{$_[0]}{RemoteHost},1, length($Attr{$_[0]}{RemoteHost})-2);},
'HOST(S)' => sub {return $_[1] eq substr($Attr{$_[0]}{RemoteHost},1, length($Attr{$_[0]}{RemoteHost})-2);},
'BATCH_NAME' => sub {
if ($command_arg ne '-batch'){
	return $_[1] eq substr($Attr{$_[0]}{JobBatchName},1,length($Attr{$_[0]}{JobBatchName})-2);
} else {
	return $_[1] eq "CMD: ".substr($Attr{$_[0]}{Cmd},1,length($Attr{$_[0]}{Cmd})-2);
}
},
'DONE' => sub {
if ($command_arg =~ /-dag/ && !($command_arg =~ /-nobatch/)){
	return $_[1] eq '_';
} else {
	my $batch_done = 0;
	$fields[(scalar @fields)-1][$_[0]+1] =~ /(.+)\.(.*)/;
	my $temp_id = $1;
	for my $i (0..((scalar keys %Attr)-1)){
		if ($Attr{$i}{ClusterId} eq $temp_id){
			if (substr($Attr{$i}{JobBatchName},1,length($Attr{$i}{JobBatchName})-2) eq $fields[1][$_[0]+1] && $Attr{$i}{JobStatus}==4){
				$batch_done++;
			}
		}
	}
	if ($batch_done > 0){
		return $batch_done eq $_[1];
	} else {return $_[1] eq '_';}

}
},
'RUN' => sub {
if ($command_arg =~ /-dag/ && !($command_arg =~ /-nobatch/)){
	return $_[1] eq 1;
} else {
	my $batch_run = 0;
	$fields[(scalar @fields)-1][$_[0]+1] =~ /(.+)\.(.*)/;
	my $temp_id = $1;
	for my $i (0..((scalar keys %Attr)-1)){
		if ($Attr{$i}{ClusterId} eq $temp_id){
			if (substr($Attr{$i}{JobBatchName},1,length($Attr{$i}{JobBatchName})-2) eq $fields[1][$_[0]+1] && $Attr{$i}{JobStatus}==2){
				$batch_run++;
			}
		}
	}
	if ($batch_run > 0){
		return $batch_run eq $_[1];
	} else {return $_[1] eq '_';}

}
},
'IDLE' => sub {
if ($command_arg =~ /-dag/ && !($command_arg =~ /-nobatch/)){
	return $_[1] eq '_';
#} elsif ($same_cluster) {
#	my $batch_idle = 0;
#	for my $i (0..(scalar keys %Attr)-1){
#		if (substr($Attr{$i}{JobBatchName},1,length($Attr{$i}{JobBatchName})-2) eq $fields[1][$_[0]+1] && $Attr{$i}{JobStatus}==1){
#		$batch_idle++;
#		}
#	}
#	if ($batch_idle > 0){
#		return $batch_idle eq $_[1];
#	} else {return $_[1] eq '_';}
} else {
	my $batch_idle = 0;
	$fields[(scalar @fields)-1][$_[0]+1] =~ /(.+)\.(.*)/;
	my $temp_id = $1;
	for my $i (0..((scalar keys %Attr)-1)){
		if ($Attr{$i}{ClusterId} eq $temp_id){
			if (substr($Attr{$i}{JobBatchName},1,length($Attr{$i}{JobBatchName})-2) eq $fields[1][$_[0]+1] && $Attr{$i}{JobStatus}==1){
				$batch_idle++;
			}
		}
	}
	if ($batch_idle > 0){
		return $batch_idle eq $_[1];
	} else {return $_[1] eq '_';}

}
},
'HOLD' => sub {
if ($command_arg =~ /-dag/ && !($command_arg =~ /-nobatch/)){
	return $_[1] eq 4;
} else {
	my $batch_hold = 0;
	$fields[(scalar @fields)-1][$_[0]+1] =~ /(.+)\.(.*)/;
	my $temp_id = $1;
	for my $i (0..((scalar keys %Attr)-1)){
		if ($Attr{$i}{ClusterId} eq $temp_id){
			if (substr($Attr{$i}{JobBatchName},1,length($Attr{$i}{JobBatchName})-2) eq $fields[1][$_[0]+1] && $Attr{$i}{JobStatus}==5){
				$batch_hold++;
			}
		}
	}
	if ($batch_hold > 0){
		return $batch_hold eq $_[1];
	} else {return $_[1] eq '_';}

}
},
'TOTAL' => sub {
if ($command_arg =~ /-dag/ && !($command_arg =~ /-nobatch/)){
	return $_[1] eq 7;
} else {
	return $_[1] eq '_';
}
},
'JOB_IDS' => sub {
if ($command_arg =~ /-dag/ && !($command_arg =~ /-nobatch/)){
	return $_[1] eq "49.0 ... 55.0";
} elsif ($_[1] =~ /(.+)\.(.+)/){
	my $temp = $1;
	return $_[1] eq $cluster_batch{$temp};
} else { return 0; }
},
'HELD_SINCE' => sub {return $_[1] =~ /[0-9]+\/[0-9]+\s+[0-9]+:[0-9]+/;},
'HOLD_REASON' => sub {return $_[1] eq substr($Attr{$_[0]}{HoldReason},1, length($Attr{$_[0]}{HoldReason})-2);},
'SIZE' => sub {
	if (defined $real_heading && $real_heading ne '-dag'){
		return $_[1] eq sprintf('%.1f',int(($Attr{$_[0]}{ResidentSetSize} + 1023)/1024));
	} else {
		$fields[0][$_[0]+1] =~ /(.+)\..+/;
		my $temp_id = $1;
		for my $k (0..(scalar keys %Attr)-1){
			if ($Attr{$k}{ClusterId} eq $temp_id){
				return $_[1] eq sprintf('%.1f', int(($Attr{$k}{ResidentSetSize}+1023)/1024));
			}
		}
	}
},
'PRI' => sub {return $_[1] eq $Attr{$_[0]}{JobPrio};},
'CMD' => sub {
	$fields[0][$_[0]+1] =~ /(.+)\..+/;
	my $temp_id = $1;
	for my $k (0..(scalar keys %Attr)-1){
		if ($Attr{$k}{ClusterId} eq $temp_id){
			my $cmd;
			if ($Attr{$k}{Cmd} =~ /.*\/+(.*)/){
				$cmd = $1;
				$cmd = substr($cmd,0,length($cmd)-1);
			} else {
				$cmd = $Attr{$k}{Cmd};
				$cmd = substr($cmd,1,length($cmd)-2);
			}
			$cmd .= " ".substr($Attr{$k}{Args},1,length($Attr{$k}{Args})-2);
			return $cmd =~ /$_[1]/;
		}
	}
},
'RUNS' => sub {
	if ($Attr{$_[0]}{JobUniverse} == 1){
		return $_[1] eq $Attr{$_[0]{NumCkpts_RAW}};
	} else {
		return $_[1] eq $Attr{$_[0]}{NumJobStarts};
	}
},
'INPUT' => sub{
	if ($Attr{$_[0]}{JobUniverse} == 1){
		return $_[1] eq $Attr{$_[0]{FileReadBytes}};
	} else {
		return $_[1] eq $Attr{$_[0]}{BytesRecvd};
	}
},
'OUTPUT' => sub {
	if ($Attr{$_[0]}{JobUniverse} == 1){
		return $_[1] eq $Attr{$_[0]{FileWriteBytes}};
	} else {
		return $_[1] eq $Attr{$_[0]}{BytesSent};
	}
},
'RATE' => sub {
	if ($Attr{$_[0]}{JobUniverse} == 1){
		return $_[1] eq sprintf('%.1f',$Attr{$_[0]{FileReadBytes}} + $Attr{$_[0]}{FileWriteBytes});
	} else {
		return $_[1] eq sprintf('%.1f',$Attr{$_[0]}{BytesRecvd} + $Attr{$_[0]}{BytesSent});
	}
},
'MISC' => sub {return $_[1] eq $Attr{$_[0]}{JobUniverse};}
);
	my $succeeded;
	for my $i (0..(scalar @fields)-1){
		if (defined $fields[$i][1]){
			TLOG("Checking ".$fields[$i][0]."\n");
			if (defined $rules_data{$fields[$i][0]}){
				for my $k (1..(scalar @{$fields[$i]})-1){
					if ($rules_data{$fields[$i][0]}($k-1, $fields[$i][$k])){
						$succeeded = 1;
					} else {
						print "        ERROR: $fields[$i][0] is not correct.\n";
						return $succeeded = 0;
					}
	
				}
				print "        PASSED: $fields[$i][0] is correct.\n";
			} else {
				next;
			}
		}
	}
	return $succeeded = 1;

}

# check if summary statement is correct
# input: command line arg, summary line from condor_q and result of count_job_states() 
sub check_summary {
	defined $_[2] || print "USAGE of check_summary: check_summary(<COMMAND_ARG>,\\<SUMMARY>,\\<NUMS_OF_JOBS>)\n"; 
	my $command_arg = $_[0];
	my @summary = @{$_[1]};
	my %num_of_jobs = %{$_[2]};
	my $real_heading = find_real_heading($command_arg); 
	my %have_summary_line = ('have_sum'=>1,'-nobatch'=>1,'-batch'=>0,'-run'=>0,'-hold'=>1,'-grid'=>0,'-globus'=>0,'-io'=>0,'-tot'=>1,'-totals'=>1,'-dag'=>1);
	if ($have_summary_line{$real_heading} ){
		TLOG("Checking summary statement.\n");
		if ($summary[0]!= $num_of_jobs{0} || $summary[2]!=$num_of_jobs{4} || $summary[4]!=$num_of_jobs{3} || $summary[6]!=$num_of_jobs{1} || $summary[8]!=$num_of_jobs{2} || $summary[10]!= $num_of_jobs{5} || $summary[12]!=$num_of_jobs{6}){
			print "		ERROR: Some states don't add up correctly\n";
			print "		Output is @summary";
			return 0;
		} else{
			print "        PASSED: Summary statement correct\n";
			return 1;
		}
	} else {
		print "        There is no summary statement\n";
		return 1;
	}
}

sub unquote{
	if ($_[0] =~ /^"(.*)"$/){
		return $1;
	} else {
		return $_[0];
	}
}

sub check_af{
	my %Attr = %{$_[0]};
	my @table = @{$_[1]};
	my $option = $_[2];
	my $sel0 = $_[3];
	my $sel1 = $_[4];
	my $sel2 = $_[5];
	my $sel3 = $_[6];
	my %data;
	for my $i (0..(scalar @table) -1){
		$data{$i} = $table[$i];
	}
	my %Attr1;
	if (!defined $Attr{0}){
		%{$Attr1{0}} = %Attr;
		%Attr = %Attr1;
	}

	for my $i (0..(scalar keys %Attr)-1){
		if ($option =~ /r/) {
			if (!(defined $Attr{$i}{$sel0})){ $Attr{$i}{$sel0} = $sel0; }
			if (!(defined $Attr{$i}{$sel1})){ $Attr{$i}{$sel1} = $sel1; }
			if (!(defined $Attr{$i}{$sel2})){ $Attr{$i}{$sel2} = $sel2; }
			if (!(defined $Attr{$i}{$sel3})){ $Attr{$i}{$sel3} = $sel3; }
		} else {
			if (!(defined $Attr{$i}{$sel0})){ $Attr{$i}{$sel0} = "undefined"; }
			if (!(defined $Attr{$i}{$sel1})){ $Attr{$i}{$sel1} = "undefined"; }
			if (!(defined $Attr{$i}{$sel2})){ $Attr{$i}{$sel2} = "undefined"; }
			if (!(defined $Attr{$i}{$sel3})){ $Attr{$i}{$sel3} = "undefined"; }
		}
	}

my %af_rules = (
	':,' => sub{
		my @words;
		for my $i (0..(scalar @table)-1){
			@words = split(", ",$table[$i]);
			if ($words[0] ne unquote($Attr{$i}{$sel0}) || $words[1] ne unquote($Attr{$i}{$sel1}) || $words[2] ne unquote($Attr{$i}{$sel2}) || $words[3] ne unquote($Attr{$i}{$sel3})."\n") {
				print "        ERROR: Incorrect output\n        $words[0]--". unquote($Attr{$i}{$sel0})."\n        $words[1]--".unquote($Attr{$i}{$sel1})."\n        $words[2]--".unquote($Attr{$i}{$sel2})."\n        $words[3]--".unquote($Attr{$i}{$sel3})."\n";
				return 0;
			}
		}
		return 1;
	},
	':h' => sub{
		if ($data{0} =~ /$sel0\s+$sel1\s+$sel2\s+$sel3/){
			my @head = split("",$data{0});
			my @chars;
			my %blank_col;
			my @edges;
			my @fields;
			for my $h (0..scalar @head-1){
				if ($head[$h] =~/\s/){
					$blank_col{$h} = $h;
				}
			}
			my @sorted_keys = sort {$a <=> $b} keys %blank_col;
			my $index = 1;
			$edges[0] = 0;
			for my $k (0..scalar @sorted_keys-2){
				if ($sorted_keys[$k+1]-$sorted_keys[$k]!= 1){
					$edges[$index] = $sorted_keys[$k]+1;
					$index++;
				}
			}
			for my $k (0..(scalar @table)-1){
				@chars = split("",$table[$k]);
				for my $i (0..(scalar @edges) -2){
					$fields[$i][$k] = trim(join("",@chars[$edges[$i]..($edges[$i+1]-1)]));
				}
				$fields[scalar @edges -1][$k] = trim(join("",@chars[$edges[scalar @edges-1]..(scalar @chars -1)]));
			}
			
			for my $i (1..(scalar @{$fields[0]})-1){
				if ($fields[0][$i] ne unquote($Attr{$i-1}{$sel0}) || $fields[1][$i] ne unquote($Attr{$i-1}{$sel1}) || $fields[2][$i] ne unquote($Attr{$i-1}{$sel2}) || $fields[3][$i] ne unquote($Attr{$i-1}{$sel3})){
					print "ERROR: Some item in the following list does not match\n","$fields[0][$i]--".unquote($Attr{$i-1}{$sel0})."\n$fields[1][$i]--".unquote($Attr{$i-1}{$sel1})."\n$fields[2][$i]--".unquote($Attr{$i-1}{$sel2})."\n$fields[3][$i]--".unquote($Attr{$i-1}{$sel3});
					return 0;
				}
			}
			return 1;
		} else {
			print "        ERROR: The heading is not correct\n";
			return 0;
		}
	},
	':ln' => sub{
		for my $i (0..(scalar keys %Attr)-1){
			print scalar keys %Attr;
			if (trim($table[$i*4]) ne trim("$sel0 = ".unquote($Attr{$i}{$sel0})) || trim($table[($i*4)+1]) ne trim("$sel1 = ".unquote($Attr{$i}{$sel1})) || trim($table[$i*4+2]) ne trim("$sel2 = ".unquote($Attr{$i}{$sel2})) || trim($table[$i*4+3]) ne trim("$sel3 = ".unquote($Attr{$i}{$sel3}))){
				print "ERROR: Some item in the following list does not match\n", trim($table[$i*4]),"\n",trim("$sel0 = ".unquote($Attr{$i}{$sel0})),"\n",trim($table[$i*4+1]),"\n", trim("$sel1 = ".unquote($Attr{$i}{$sel1})),"\n",trim($table[$i*4+2]),"\n",trim("$sel2 = ".unquote($Attr{$i}{$sel2})),"\n",trim($table[$i*4+3]),"\n",trim("$sel3 = ".unquote($Attr{$i}{$sel3})),"\n";
				return 0;
			}
		}
		return 1;
	},
	':lrng' => sub {
		for my $i (0..(scalar keys %Attr)-1){
			if ($table[$i*5] ne "\n" || $table[$i*5+1] ne "$sel0 = $Attr{$i}{$sel0}\n" || $table[$i*5+2] ne "$sel1 = $Attr{$i}{$sel1}\n" || $table[$i*5+3] ne "$sel2 = $Attr{$i}{$sel2}\n" || $table[$i*5+4] ne "$sel3 = $Attr{$i}{$sel3}\n") { 
				print "ERROR: Some item in the following list does not match\n", $table[$i*5+1],"\n","$sel0 = $Attr{$i}{$sel0}\n", $table[$i*5+2],"\n","$sel1 = $Attr{$i}{$sel1}\n",$table[$i*5+3],"\n","$sel2 = $Attr{$i}{$sel2}\n",$table[$i*5+4],"\n","$sel3 = $Attr{$i}{$sel3}\n";
				return 0;
			}
		}
		return 1;	
	},
	':j' => sub {
		if ($table[0] eq $Attr{0}{ClusterId}.".".$Attr{0}{ProcId}." ".unquote($Attr{0}{$sel0})." ".unquote($Attr{0}{$sel1})." ".unquote($Attr{0}{$sel2})." ".unquote($Attr{0}{$sel3})."\n"){
			return 1;
		} else {
			print "ERROR: Some item in the following list does not match\n",$table[0],"\n",$Attr{0}{ClusterId}.".".$Attr{0}{ProcId}." ".unquote($Attr{0}{$sel0})." ".unquote($Attr{0}{$sel1})." ".unquote($Attr{0}{$sel2})." ".unquote($Attr{0}{$sel3})."\n"
		}
	},
	':t' => sub {
		for my $i (0..(scalar @table)-1){
			if  (trim($table[$i]) ne trim(unquote($Attr{$i}{$sel0})."\t".unquote($Attr{$i}{$sel1})."\t".unquote($Attr{$i}{$sel2})."\t".unquote($Attr{$i}{$sel3}))){
				print "ERROR: Some item in the following list does not match\n",trim($table[$i]),"\n", trim(unquote($Attr{$i}{$sel0})."\t".unquote($Attr{$i}{$sel1})."\t".unquote($Attr{$i}{$sel2})."\t".unquote($Attr{$i}{$sel3}));
				return 0;
			}
		}
		return 1;
	}
);
	TLOG("Checking -af$option\n");
	if (defined $af_rules{$option}){
		return $af_rules{$option}();
	} elsif (!defined $af_rules{$option}){
		for my $i (0..(scalar @table)-1){
print $i;
			if ($table[$i] ne unquote($Attr{$i}{$sel0})." ".unquote($Attr{$i}{$sel1})." ".unquote($Attr{$i}{$sel2})." ".unquote($Attr{$i}{$sel3})."\n"){
				return 0;
			}
		}
		return 1;
	} else {
		return 0;
	}
}

sub check_type {
	my %Attr = %{$_[0]};
	my $option = $_[1];
	my @table = @{$_[2]};
	my $sel0 = $_[3];
	my %format_rules;
	my %Attr1;
	if (!(defined $Attr{0})){
		%{$Attr1{0}} = %Attr;
		%Attr = %Attr1;
	}
	%format_rules = (
' %v' => sub{
	my $line = "";
	for my $i (0..(scalar keys %Attr)-1){
		if (!(defined $Attr{$i}{$sel0})){
			$line .= "undefined";
		} else {
			$line.= unquote($Attr{$i}{$sel0});
		}
	}
	if ($table[0] eq $line){
		return 1;
	} else {
		print "Output is $table[0], shoule be $line\n";
		return 0;
	}
},
' %V' => sub{
	my $line = "";
	for my $i (0..(scalar keys %Attr)-1){
		if (defined $Attr{$i}{$sel0}){
			$line.= $Attr{$i}{$sel0};
		} else {
			$line .= "undefined";
		}
	}
	if ($table[0] eq $line){
		return 1;
	} else {
		print "Output is $table[0], shoule be $line\n";
		return 0;
	}
},
' %d' => sub{
	my $line = "";
	for my $i (0..(scalar keys %Attr)-1){
		if (!(defined $table[0])){return 1;}
		if (defined $Attr{$i}{$sel0}) {
			if (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /([0-9])\.[0-9]+E(.+)/){
				my $integer = $1;
				my $multi = $2;
				if ($multi < 0){
					$line .= 0;
				} else {
					$line .= $integer;
				}
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^[0-9]+$/){
				$line .= unquote($Attr{$i}{$sel0});
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^-([0-9]+)/){
				$line .= unquote($Attr{$i}{$sel0});
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^([0-9]+)\.[0-9]+$/){
				unquote($Attr{$i}{$sel0}) =~ /^([0-9]+)\.[0-9]+$/;
				$line .= $1;
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^true$/i){
				$line .= 1;
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^false$/i){
				$line .= 0;
			} elsif (!(defined $Attr{$i}{$sel0})){
				$line = $line;
			} else {
				print "Should not have any output\n";
				return 0;
			}
		}
	}
	if ($table[0] eq $line){return 1;}
	else {print $table[0],"\n$line\n";return 0;}
},
' %f' => sub{
	my $line = "";
	for my $i (0..(scalar keys %Attr)-1){
		if (!(defined $table[0])){return 1;}
		if (defined $Attr{$i}{$sel0}) {
			if (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /([0-9])\.[0-9]+E(.+)/){
				my $integer = $1;
				my $multi = $2;
				if ($multi < -6){
					$line .= "0.000000";
				} else {
					$line .= sprintf("%.6f",unquote($Attr{$i}{$sel0}));
				}
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^[0-9]+\.([0-9]+)$/){
				my $len = length($1);
				if ($len<6){
					$line .= unquote($Attr{$i}{$sel0})."0"x(6-$len);
				} elsif ($len > 6){		
					$line .= sprintf("%.6f",unquote($Attr{$i}{$sel0}));
				} else {
					$line .= unquote($Attr{$i}{$sel0});
				}
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^[0-9]+$/){
				$line .= unquote($Attr{$i}{$sel0})."."."0"x6;
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^-[0-9]+.*/){
				$line .= unquote($Attr{$i}{$sel0})."."."0"x6;
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^true$/i){
				$line .= "1.000000";
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^false$/i){
				$line .= "0.000000";
			} else {
				print $table[0]," Should not have any output\n";
				return 0;
			}
		}
	}
	if ($line eq $table[0]){return 1;}
	else {print $table[0], "\n$line\n"; return 0;}
},
' %.2f' => sub {
	my $line = "";
	for my $i (0..(scalar keys %Attr)-1){
		if (!(defined $table[0])){return 1;}
		if (defined $Attr{$i}{$sel0}) {
			if (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /([0-9])\.[0-9]+E(.+)/){
				my $integer = $1;
				my $multi = $2;
				if ($multi < -2){
					$line .= "0.00";
				} else {
					$line .= sprintf("%.2f",unquote($Attr{$i}{$sel0}));
				}
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^[0-9]+\.([0-9]+)$/){
				my $len = length($1);
				if ($len<2){
					$line .= unquote($Attr{$i}{$sel0})."0"x(2-$len);
				} elsif ($len > 2){		
					$line .= sprintf("%.2f",unquote($Attr{$i}{$sel0}));
				} else {
					$line .= unquote($Attr{$i}{$sel0});
				}
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^[0-9]+$/){
				$line .= unquote($Attr{$i}{$sel0})."."."0"x2;
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^-[0-9]+.*/){
				$line .= unquote($Attr{$i}{$sel0})."."."0"x2;
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^true$/i){
				$line .= "1.00";
			} elsif (defined $table[0] && unquote($Attr{$i}{$sel0}) =~ /^false$/i){
				$line .= "0.00";
			} else {
				print $table[0]," Should not have any output\n";
				return 0;
			}
		}
	}
	if ($line eq $table[0]){return 1;}
	else {print $table[0], "\n$line\n"; return 0;}
},
' %s' => sub{
	my $line = "";
	for my $i (0..(scalar keys %Attr)-1){
		$line .= unquote($Attr{$i}{$sel0});
	}
	if ($line eq $table[0]){
		return 1;
	} else {
		print "Output is $table[0], should be $line\n";
		return 0;
	}
},
' "%6.2f;"' => sub {
	my $line = "";
	for my $i (0..(scalar keys %Attr)-1) {
		$line .= sprintf("%6.2f", unquote($Attr{$i}{Rank}));
		$line .= ";";
	}
	if ($line eq $table[0]) {return 1;}
	else {
		print $table[0], " Should be $line\n";
		return 0;
	}
}
	);
	TLOG("Checking -format$option $sel0\n");
	if (defined $option && defined $format_rules{$option}){
		return $format_rules{$option}();
	} else {
		print "Needs to have a format option and a class ad\n";
		return 0;
	}
}

sub check_pr {
	my %data = %{$_[0]};
	my $option = $_[1];
	my %Attr;
	if (defined $_[2]){
		%Attr = %{$_[2]};
	}
	TLOG("Checking the print format\n");
	if ($option eq 'normal'){
		if (!($data{0} =~ /ID\s+OWNER\s+SUBMITTED\s+RUN_TIME\s+ST\s+PRI\s+SIZE\s+CMD/) || length($data{0}) > 80){ 
			print "        FAILED: heading is not correct\n";
			return 0;
		} else {
			return 1;
		}
	}
	if ($option eq 'random'){
		if (length($data{0}) > 89){
			print "        FAILED: length is not correct\n";
			return 0;
		} else {
			return 1;
		}
	}
	if ($option eq 'short'){
		my @fields = split_fields(\%data);
		for my $i (1..8){
			if (length($fields[0][$i]) > 5 || length($fields[1][$i]) > 2 || length($fields[2][$i]) > 4 || length($fields[3][$i]) > 3){
				print "        FAILED: length is not correct\n";
				return 0;
			}
		}
		return 1;
	}
	if ($option eq 'where2'){
		my @fields = split_fields(\%data);
		for my $i (1..4){
			if ($fields[4][$i] ne 'H'){
				print "        FAILED: selected incorrect jobs\n";
				return 0;
			}
		}
		return 1;
	}
	if ($option eq 'printf'){
		if (length($data{0}) ne 16){
			print "        FAILED: length is not correct\n";
			return 0;
		} else {
			for my $i (1..8){
				if ($data{$i} ne " $Attr{$i-1}{ClusterId}  $Attr{$i-1}{NumJobStarts}    $Attr{$i-1}{ProcId}\.00\n"){ 
					print "        FAILED: Output is:  $data{$i}\n        Should be:          $Attr{$i-1}{ClusterId}  $Attr{$i-1}{NumJobStarts}    $Attr{$i-1}{ProcId}\.00\n";
					return 0;
				}
			}
			return 1;	
		}
	}
	if ($option eq 'ifthenelse'){
		for my $i (1..8){
			my $string = substr($Attr{$i-1}{Owner}, 1, length($Attr{$i-1}{Owner})-2);
			unless ($data{$i} =~ /0.0\s+$string/){
				print "        FAILED: Output is: $data{$i}\n";
				return 0;
			}
		}
		return 1;
	}
}

sub check_transform {
	my $out_file = $_[0];
	my $option = $_[1];
	my %Attr;
	my $index = 0;
	my $result = 0;
	open (my $HANDLER, $out_file) or die "ERROR OPENING THE FILE $out_file";
	while (<$HANDLER>){
		if ($_ =~ /\S/){
			$_ =~ /^([A-Za-z0-9_]+)\s*=\s*(.*)$/;
	               	my $key = $1;
	                my $value = $2;
               		$Attr{$index}{$key} = $value;
        	} else {                
                	$index++;
        	}
	}
	TLOG("checking $option\n");	
	if ($index<1){
		print "ERROR, output nothing. Might be a seg fault\n";
		return 0;
	} else {
	if ($option eq 'general'){
		for my $i (0..$index-1){
			if ($Attr{$i}{ClusterId} ne 200 || $Attr{$i}{Fooo} ne "\"test\"" || $Attr{$i}{TransferIn} ne $Attr{$i}{OnExitRemove} || defined $Attr{$i}{TransferInputSizeMB} || defined $Attr{$i}{TransferErr} || !(defined $Attr{$i}{Err})){
				return 0;
			}
		}
		return 1;
	}
	if ($option eq 'EVAL'){
		for my $i (0..$index-1){
			my $temp = $Attr{$i}{DiskUsage}*2;
			if ($Attr{$i}{MemoryUsage} ne 5 || !($Attr{$i}{RequestDisk} =~ /(\s*$temp\s*\/\s*1024\s*)/)){
				print "MemoryUsage is $Attr{$i}{MemoryUsage}. should be 5\n";
				print "RequestDisk is $Attr{$i}{RequestDisk}. should be ($temp / 1024)";
				return 0;
			}
		}
		return 1;
	}
	if ($option eq "transform_num"){
		return scalar keys %Attr eq 40;
	}
	if ($option eq 'regex'){
		for my $i (0..$index-1){
			if ($Attr{$i}{TotalSuspensions} ne $Attr{$i}{MaxHosts} || defined $Attr{$i}{NiceUser} || defined $Attr{$i}{RemoteUser} || defined $Attr{$i}{PeriodicHold} || defined $Attr{$i}{PeriodicRelease} || defined $Attr{$i}{PeriodicRemove}){
				return 0;
			} elsif (defined $Attr{$i}{User} && defined $Attr{$i}{TestRemoteUser} && defined $Attr{$i}{TimeHold} && defined $Attr{$i}{TimeRelease} && defined $Attr{$i}{TimeRemove}){
				return 1;
			} else {return 0;}
		}
	}
	if ($option eq 'transform_in1'){
		for my $i (0..$index-1){
			if ((($i % 3) == 0 && $Attr{$i}{ARG1} ne '2.0') || (($i % 3) ==1 && $Attr{$i}{ARG1} ne 100) || (($i % 3) ==2 && $Attr{$i}{ARG1} ne "\"test\"")){
				if ($i%3 == 0){
					print "Index is $i, output is $Attr{$i}{ARG1}, should be 2.0\n";
				}
				if ($i%3 == 1){
					print "Index is $i, output is $Attr{$i}{ARG1}, should be 100\n";
				}
				if ($i%3 == 2){
					print "Index is $i, output is $Attr{$i}{ARG1}, should be \"test\"\n";
				}
				return 0;
			}
		}
		return 1;
	}
	if ($option eq 'transform_in2'){
		for my $i (0..$index-1){
			if ((($i % 6 == 0 || $i%6==1) && ($Attr{$i}{ARG1} ne '2.0')) || (($i % 6==2||$i%6==3) && ($Attr{$i}{ARG1} ne 100)) || (($i %6==4 || $i %6==5) && ($Attr{$i}{ARG1} ne "\"test\""))){
				if ($i%6 == 0 || $i%6==1){
					print "Index is $i, output is $Attr{$i}{ARG1}, should be 2.0\n";
				}
				if ($i%6 == 2 || $i%6 == 3){
					print "Index is $i, output is $Attr{$i}{ARG1}, should be 100\n";
				}
				if ($i%6 == 4 || $i%6==5){
					print "Index is $i, output is $Attr{$i}{ARG1}, should be \"test\"\n";
				}
				return 0;
			}
		}
		return 1;
	}
	if ($option eq 'transform_from1'|| $option eq 'transform_from2'){
		for my $i (0..$index-1){
			if ((($i % 2) ==0 && $Attr{$i}{SIZE_IMAGE} ne 1000) || (($i % 2) ==0 && $Attr{$i}{SIZE_DISK} ne '120.0') || (($i % 2) ==1 && $Attr{$i}{SIZE_IMAGE} ne 2000) || (($i % 2) ==1 && $Attr{$i}{SIZE_DISK} ne '128.0')){
				if ($i%2 == 0){
					print "Index is $i, SIZE_IMAGE is $Attr{$i}{SIZE_IMAGE}, should be 1000; SIZE_DISK is $Attr{$i}{SIZE_IMAGE}, should be 120.0\n";
				}
				if ($i%2 == 1){
					print "Index is $i, SIZE_IMAGE is $Attr{$i}{SIZE_IMAGE}, should be 2000; SIZE_DISK is $Attr{$i}{SIZE_IMAGE}, should be 128.0\n";
				}
				return 0;
			}
		}
		return 1;
	}
	if ($option eq 'transform_from3'){
		for my $i (0..$index-1){
			if ((($i % 4 ==0 || $i%4==1) && ($Attr{$i}{SIZE_IMAGE} ne 1000)) || (($i % 4 ==0 || $i%4==1) && ($Attr{$i}{SIZE_DISK} ne '120.0')) || (($i % 4 ==2 || $i%4==3) && ($Attr{$i}{SIZE_IMAGE} ne 2000)) || (($i % 4 ==2 || $i%4==3) && ($Attr{$i}{SIZE_DISK} ne '128.0'))){
				if ($i%4 == 0 || $i%4==1){
					print "Index is $i, SIZE_IMAGE is $Attr{$i}{SIZE_IMAGE}, should be 1000; SIZE_DISK is $Attr{$i}{SIZE_IMAGE}, should be 120.0\n";
				}
				if ($i%4 == 2 || $i%4==3){
					print "Index is $i, SIZE_IMAGE is $Attr{$i}{SIZE_IMAGE}, should be 2000; SIZE_DISK is $Attr{$i}{SIZE_IMAGE}, should be 128.0\n";
				}
				return 0;
			}
		}
		return 1;
	}
}	
}

sub read_attr {
	my $filename = $_[0];
	my %Attr;
	my $index = 0;
	open (my $HANDLER, $filename) or die "ERROR OPENING THE FILE $filename";
	while (<$HANDLER>){
        	if ($_ =~ /\S/){
	               	$_ =~ /^([A-Za-z0-9_]+)\s*=\s*(.*)$/;
	                my $key = $1;
	                my $value = $2;
	                $Attr{$index}{$key} = $value;
        	} else {
                	$index++;
        	}
	}
	return %Attr;
}

sub write_ads_to_file {
	my $testname = $_[0];
	my %Attr = %{$_[1]};
	my $fname = "$testname.simulated.ads";
	open(FH, ">$fname") || print "FAILED writing to file $fname";
	foreach my $k1 (sort keys %Attr){
		print FH $k1," = ", $Attr{$k1}, "\n";
	}
	close(FH);
	return $fname;
}

sub emit_dag_files {
        my ($testname,$submit_content,$pid) = @_;                                                      
        defined $pid || print "USAGE of emit_dag_files: emit_dag_files(<TESTNAME>,<SUBMIT_CONTENT>,<PROCESSID>)\n";
	my $dag_content = "JOBSTATE_LOG $testname.jobstate.log                                    
                Job A_A ${testname}_A.cmd
                SCRIPT PRE A_A ${testname}_A_pre.sh                                               
                SCRIPT POST A_A ${testname}_A_post.sh
                ";

        my $pre_script_sh_content = "#! /usr/bin/env sh\necho \"PRE A_A running\"\nexit 1\n";     
        my $pre_script_bat_content = "\@echo PRE A_A running\n\@exit /b 1\n";                     
        my $post_script_sh_content = "#! /usr/bin/env sh\necho \"POST A_A running\"\nexit 1\n";   
        my $post_script_bat_content = "\@echo POST A_A running\n\@exit /b 1\n";                   
                                                                                                  
        emit_file(".dag",$dag_content,$testname);                                                           
        emit_file("$pid.sub",$submit_content,$testname);                                                    
}

sub emit_file {                                                                                   
        my $namex = shift;                                                                        
        my $content = shift;   
	my $testname = shift;                                                                   
        my $fname = $testname . $namex;                                                           
        open (FH, ">$fname") || print "error writing to $fname: $!\n";                            
        print FH $content;
        close(FH);
        chmod(0755,$fname);                                                                       
}

sub wait_for_reconfig {
	my $count = 120;
	my $iswindows = CondorUtils::is_windows();
	my $file = `condor_config_val log`;
	$file =~ s/\n//;
	if ($iswindows) {
		$file = "$file\\SchedLog";
	} else {
		$file = "$file/SchedLog";
	}
	my @log;
	while ($count > 0) {
		if (is_mac()) {
			@log = `tail -n 80 $file`;
		} else {
			@log = `tail $file --lines=80`;
		}
		for my $i (0..(scalar @log)-1) {
			if ($log[$i] =~ /SIGHUP/) {
				return 1;
			}
		}
		sleep(1);
		$count--;
	}
	return 0;
}

sub is_mac {
	if ($^O =~ /MacOS/i || $^O =~ /rhapsody/i || $^O =~ /darwin/i) {
		return 1;
	} else {
		return 0;
	}
}
