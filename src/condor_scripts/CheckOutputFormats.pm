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

use Exporter qw(import);
our @EXPORT_OK = qw(dry_run add_status_ads change_clusterid multi_owners create_table various_hold_reasons find_blank_columns split_fields cal_from_raw trim count_job_states make_batch check_heading check_data check_summary emit_dag_files emit_file);                                                    

# construct a hash containing job ads from dry run
sub dry_run {
	my $testname = $_[0];
	my $pid = $_[1];
	my $ClusterId = $_[2];
	my $executable = $_[3];
	my $submit_file = "$testname$pid.sub";
	my $host = hostname;
	my $time = time();
	my $counter = 1;
	my $index = 0;
	my %all_keys;
	my %Attr;
	my @lines = `condor_submit -dry - $submit_file`;
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
	# add the additional job ads that would appear in condor_q -long
	for my $i (0..$index-1){
		my $owner = $Attr{$i}{Owner};
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
			$Attr{$i}{CumulativeSlotTime} = ($Attr{$i}{EnteredCurrentStatus} - $Attr{$i}{QDate})."\."."0";
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
			$Attr{$i}{MemoryUsage} = "( ( ResidentSetSize + 1023 ) / 1024 )";
			$Attr{$i}{NumJobMatches} = 1;
			$Attr{$i}{NumJobStarts} = 1;
			$Attr{$i}{NumShadowStarts} = 1;
			$Attr{$i}{JobRunCount} = $Attr{$i}{NumShadowStarts};
			$Attr{$i}{OrigMaxHosts} = 1;
			$Attr{$i}{ResidentSetSize_RAW} = 488;
			$Attr{$i}{ResidentSetSize} = cal_from_raw($Attr{$i}{ResidentSetSize_RAW});
			$Attr{$i}{RemoteWallClockTime} = $Attr{$i}{CumulativeSlotTime};
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
			$Attr{$i}{AutoClusterAttrs} = "\""."JobUniverse,LastCheckpointPlatform,NumCkpts,MachineLastMatchTime,ConcurrencyLimits,NiceUser,Rank,Requirements,DiskUsage,FileSystemDomain,ImageSize,RequestDisk,RequestMemory"."\"";
			$Attr{$i}{AutoClusterId} = 1;
			$Attr{$i}{CurrentHosts} = 1;
			$Attr{$i}{DiskUsage} = 2500000;
			$Attr{$i}{JobCurrentStartDate} = $Attr{$i}{QDate};
			$Attr{$i}{JobStartDate} = $Attr{$i}{QDate};
			$Attr{$i}{LastJobLeaseRenewal} = $Attr{$i}{EnteredCurrentStatus};
			$Attr{$i}{LastJobStatus} = 1;
			$Attr{$i}{LastMatchTime} = $Attr{$i}{EnteredCurrentStatus};
			$Attr{$i}{MachineAttrCpus0} = 1;
			$Attr{$i}{MachineAttrSlotWeight0} = 1;
			$Attr{$i}{MemoryUsage} = "( ( ResidentSetSize + 1023 ) / 1024 )";
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

sub create_table {
	my %Attr = %{$_[0]};
	my $testname = $_[1];
	my $command_arg = $_[2];
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
	my @table = `condor_q -job $fname $command_arg`;

# read everything before -- and store them to $other
	my %other;
	my $cnt = 0;
	until ($cnt == scalar @table || $table[$cnt] =~ /--/){
		$other{$cnt} = $table[$cnt];
		$cnt++;
	}
	my $head_pos = $cnt+1;

# read everything before the blank line and store them to $data
	$cnt = 0;
	my %data;
	while(defined $table[$head_pos+$cnt] && $table[$head_pos+$cnt]=~ /\S/){
		$data{$cnt} = $table[$head_pos+$cnt];
		$cnt++;
	}

# read another line of summary
	my $arg = $table[(scalar @table)-1];
	my @summary = split / /, $arg;
	return (\%other,\%data,\@summary);
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

	defined $_[0] || die "USAGE of count_job_states: count_job_states(<LIST_OF_JOB_ADS>)\n";
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
		if ($1 ne '-wide' && $1 ne '-nobatch'){
			$real_heading = $1;
		}
		if ($2 ne '-wide' && $2 ne '-nobatch'){
			$real_heading = $2;
		}	
		if ($3 ne '-wide' && $3 ne '-nobatch'){
			$real_heading = $3;
		}
	# if there are two command arguments, one can be -wide or -nobatch
	} elsif (defined $command_arg && $command_arg =~ /(.+)\s+(.+)/){
		if (('-nobatch' eq $1 || '-nobatch' eq $2) && ('-wide' eq $1 || '-wide' eq $2)){
			$real_heading = '-nobatch';
		} elsif ($1 eq '-nobatch'){
			$real_heading = $2;
		} elsif ($2 eq '-nobatch'){
			$real_heading = $1;
		} elsif ($1 eq '-wide' && $2 ne '-dag' && $2 ne '-run'){
			$real_heading = $2;
		} elsif ($2 eq '-wide' && $1 ne '-dag' && $1 ne '-run'){
			$real_heading = $1;
		} else {
			$real_heading = '-batch';
		}	
	# one command argument, -dag and -run same with -batch
	} elsif (defined $command_arg){
		if ($command_arg eq '-wide' || $command_arg eq '-run' || $command_arg eq '-dag'){
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
		'-tot' => sub {print "-tot does not have a heading\n";return 1;}
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
	defined $_[2] || die "USAGE of check_summary: check_summary(<COMMAND_ARG>,\\<SUMMARY>,\\<NUMS_OF_JOBS>)\n"; 
	my $command_arg = $_[0];
	my @summary = @{$_[1]};
	my %num_of_jobs = %{$_[2]};
	my $real_heading = find_real_heading($command_arg); 
	my %have_summary_line = ('-nobatch'=>1,'-batch'=>0,'-run'=>0,'-hold'=>1,'-grid'=>0,'-globus'=>0,'-io'=>0,'-tot'=>1,'-totals'=>1,'-dag'=>1);
	if ($have_summary_line{$real_heading} ){
		TLOG("Checking summary statement.\n");
		if ($summary[0]!= $num_of_jobs{0} || $summary[2]!=$num_of_jobs{4} || $summary[4]!=$num_of_jobs{3} || $summary[6]!=$num_of_jobs{1} || $summary[8]!=$num_of_jobs{2} || $summary[10]!= $num_of_jobs{5} || $summary[12]!=$num_of_jobs{6}){
			print "        ERROR: Some states don't add up correctly\n";
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
if (!(defined $Attr{$sel0})){ $Attr{$sel0} = "undefined"; }
if (!(defined $Attr{$sel1})){ $Attr{$sel1} = "undefined"; }
if (!(defined $Attr{$sel2})){ $Attr{$sel2} = "undefined"; }
if (!(defined $Attr{$sel3})){ $Attr{$sel3} = "undefined"; }

my %af_rules = (
	':,' => sub{
		my @words = split(", ",$table[0]);
		if ($words[0] eq unquote($Attr{$sel0}) && $words[1] eq unquote($Attr{$sel1}) && $words[2] eq unquote($Attr{$sel2}) && $words[3] eq unquote($Attr{$sel3})."\n") {
			return 1;
		} else {
			print "        ERROR: Incorrect output\n        $words[0]--". unquote($Attr{$sel0})."\n        $words[1]--".unquote($Attr{$sel1})."\n        $words[2]--".unquote($Attr{$sel2})."\n        $words[3]--".unquote($Attr{$sel3})."\n";
			return 0;
		}
	},
	':h' => sub{
		if ($data{0} =~ /$sel0\s+$sel1\s+$sel2\s+$sel3/){
			my @head = split("",$data{0});
			my @chars = split("",$data{1});
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
			for my $i (0..(scalar @edges) -2){
				$fields[$i] = trim(join("",@chars[$edges[$i]..($edges[$i+1]-1)]));
			}
			$fields[scalar @edges -1] = trim(join("",@chars[$edges[scalar @edges-1]..(scalar @chars -1)]));
			if ($fields[0] eq unquote($Attr{$sel0}) && $fields[1] eq unquote($Attr{$sel1}) && $fields[2] eq unquote($Attr{$sel2}) && $fields[3] eq unquote($Attr{$sel3})){
				return 1
			} else {
				print "        ERROR: Incorrect output\n        $fields[0]--".unquote($Attr{$sel0})."\n        $fields[1]--".unquote($Attr{$sel1})."\n        $fields[2]--".unquote($Attr{$sel2})."\n        $fields[3]--".unquote($Attr{$sel3})."\n";
				return 0;
			}
		} else {
			print "        ERROR: The heading is not correct\n";
			return 0;
		}
	},
	':ln' => sub{
		if (trim($table[0]) eq trim("$sel0 = ".unquote($Attr{$sel0})) && trim($table[1]) eq trim("$sel1 = ".unquote($Attr{$sel1})) && trim($table[2]) eq trim("$sel2 = ".unquote($Attr{$sel2})) && trim($table[3]) eq trim("$sel3 = ".unquote($Attr{$sel3}))){
		return 1;
		} else {
			print trim($table[0]),"\n",trim("$sel0 = ".unquote($Attr{$sel0})),"\n",trim($table[1]),"\n", trim("$sel1 = ".unquote($Attr{$sel1})),"\n",trim($table[2]),"\n",trim("$sel2 = ".unquote($Attr{$sel2})),"\n",trim($table[3]),"\n",trim("$sel3 = ".unquote($Attr{$sel3})),"\n";
			return 0;
		}
	},
	':lrng' => sub {
		if ($table[0] eq "\n" && $table[1] eq "$sel0 = $Attr{$sel0}\n" && $table[2] eq "$sel1 = $Attr{$sel1}\n" && $table[3] eq "$sel2 = $Attr{$sel2}\n" && $table[4] eq "$sel3 = $Attr{$sel3}\n") { return 1;} else {
			print $table[0],"\n","\n",$table[1],"\n","$sel0 = $Attr{$sel0}\n", $table[2],"\n","$sel1 = $Attr{$sel1}\n",$table[3],"\n","$sel2 = $Attr{$sel2}\n",$table[4],"\n","$sel3 = $Attr{$sel3}\n";
		}	
	},
	':j' => sub {
		if ($table[0] eq $Attr{ClusterId}.".".$Attr{ProcId}." ".unquote($Attr{$sel0})." ".unquote($Attr{$sel1})." ".unquote($Attr{$sel2})." ".unquote($Attr{$sel3})."\n"){
			return 1;
		} else {
			print $table[0],"\n",$Attr{ClusterId}.".".$Attr{ProcId}." ".unquote($Attr{$sel0})." ".unquote($Attr{$sel1})." ".unquote($Attr{$sel2})." ".unquote($Attr{$sel3})."\n"
		}
	},
	':t' => sub {
		if  (trim($table[0]) eq trim(unquote($Attr{$sel0})."\t".unquote($Attr{$sel1})."\t".unquote($Attr{$sel2})."\t".unquote($Attr{$sel3}))){
			return 1;
		} else {	
			print trim($table[0]),"\n", trim(unquote($Attr{$sel0})."\t".unquote($Attr{$sel1})."\t".unquote($Attr{$sel2})."\t".unquote($Attr{$sel3}));
		}
	}
);
	TLOG("Checking condor_q -af$option\n");
	if (defined $af_rules{$option}){
		return $af_rules{$option}();
	} elsif (!defined $af_rules{$option}){
		return $table[0] eq unquote($Attr{$sel0})." ".unquote($Attr{$sel1})." ".unquote($Attr{$sel2})." ".unquote($Attr{$sel3})."\n";
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
	my $regex = qr/^\s%\.([0-9])f$/;
	%format_rules = (
' %v' => sub{
	if (defined $Attr{$sel0}){
		if ($table[0] eq unquote($Attr{$sel0})){
			return 1;
		} else {
			print $table[0],"\n",unquote($Attr{$sel0}),"\n";
			return 0;
		}
	} else {
		if ($table[0] eq "undefined"){
			return 1;
		} else {
			print "Output is $table[0] where it should be undefined\n";
			return 0;
		}
	}},
' %V' => sub{
	if (defined $Attr{$sel0}){	
		if ($table[0] eq $Attr{$sel0}){
			return 1;
		} else {
			print $table[0],"\n",$Attr{$sel0},"\n";
			return 0;
		}
	} else {
		if ($table[0] eq "undefined"){
			return 1;
		} else {
			print "Output is $table[0] where it should be \"undefined\"\n";
			return 0;
		}
	}},
' %d' => sub{
	if (defined $Attr{$sel0}){
		if (defined $table[0] && unquote($Attr{$sel0}) =~ /^[0-9]+$/){
			if ($table[0] eq unquote($Attr{$sel0})){return 1;}
			else {print $table[0],"\n";return 0;}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) =~ /^([0-9]+)\.[0-9]+$/){
			if ($table[0] eq $1){return 1;}
			else {print $table[0],"\n";return 0;}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) eq "true"){
			if ($table[0] eq 1){return 1;}
			else {print $table[0],"\n";return 0;}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) eq "false"){
			if ($table[0] eq 0) {return 1;}
			else {print $table[0],"\n";return 0;}
		} else {
			if (!(defined $table[0])){return 1;}
			else {print"Output is $table[0]. Should not have any output\n";return 0;}
		}
	} else { return !(defined $table[0]); }
	},
' %f' => sub{
	if (defined $Attr{$sel0}){
		if (defined $table[0] && unquote($Attr{$sel0}) =~ /^[0-9]+\.([0-9])+$/){
			my $len = length($1);
			if ($len<6){
				if ($table[0] eq unquote($Attr{$sel0})."0"x(6-$len)){return 1;}
				else {print $table[0],"\n";return 0;}
			} elsif ($len > 6){
				if ($table[0] eq substr(unquote($Attr{$sel0}),0,length(unquote($Attr{$sel0}))-$len + 6)){return 1;}
				else {print $table[0],"\n";return 0;}
			} else {
				if ($table[0] eq unquote($Attr{$sel0})){return 1;}
				else {print $table[0],"\n";return 0;}
			}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) =~ /^[0-9]+$/){
			if ($table[0] eq unquote($Attr{$sel0})."."."0"x6){return 1;}
			else {print $table[0],"\n";return 0;}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) eq "true"){
			if ($table[0] eq "1.000000"){return 1;}
			else {print $table[0],"\n";return 0;}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) eq "false"){
			if ($table[0] eq "0.000000"){return 1;}
			else {print $table[0],"\n";return 0;}
		} else {
			if (!(defined $table[0])){return 1;}
			else {print $table[0]," Should not have any output\n";return 0;}
		}
	} else {
		if (!(defined $table[0])){return 1;}
		else {print $table[0]," Should not have any output\n";return 0;}
 	}
	},
' %.2f' => sub {
	if (defined $Attr{$sel0}){
		if (defined $table[0] && unquote($Attr{$sel0}) =~ /^[0-9]+\.([0-9])+$/){
			my $len = length($1);
			if ($len<2){
				if ($table[0] eq unquote($Attr{$sel0})."0"x(2-$len)){return 1;}
				else {print $table[0],"\n";return 0;}
			} elsif ($len > 2){
				if ($table[0] eq substr(unquote($Attr{$sel0}),0,length(unquote($Attr{$sel0}))-$len + 2)){return 1;}
				else {print $table[0],"\n";return 0;}
			} else {
				if ($table[0] eq unquote($Attr{$sel0})){return 1;}
				else {print $table[0],"\n";return 0;}
			}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) =~ /^[0-9]+$/){
			if ($table[0] eq unquote($Attr{$sel0})."."."0"x2){return 1;}
			else {print $table[0],"\n";return 0;}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) eq "true"){
			if ($table[0] eq "1.00"){return 1;}
			else {print $table[0],"\n";return 0;}
		} elsif (defined $table[0] && unquote($Attr{$sel0}) eq "false"){
			if ($table[0] eq "0.00"){return 1;}
			else {print $table[0],"\n";return 0;}
		} else {
			if (!(defined $table[0])){return 1;}
			else {print $table[0],"\n";return 0;}
		}
	} else {
		if (!(defined $table[0])){return 1;}
		else {print $table[0]," Should not have any output\n";return 0;}
	}
},
' %s' => sub{
	if (defined $Attr{$sel0}){
		if ($table[0] eq unquote($Attr{$sel0})){return 1;}
		else {print $table[0],"\n";return 0;}
	} else {
		if (!(defined $table[0])){return 1;}
		else {print $table[0]," Should not have any output\n";return 0;}	
	}}
	);
	TLOG("Checking condor_q -format$option\n");
	if (defined $option && defined $format_rules{$option}){
		return $format_rules{$option}();
	} else {
		print "Needs to have a format option and a class ad\n";
		return 0;
	}
}

sub write_ads_to_file {
	my $testname = $_[0];
	my %Attr = %{$_[1]};
	my $fname = "$testname.simulated.ads";
	open(FH, ">$fname") || print "ERROR writing to file $fname";
	foreach my $k1 (sort keys %Attr){
		print FH $k1," = ", $Attr{$k1}, "\n";
	}
	close(FH);
	return $fname;
}

sub emit_dag_files {
        my ($testname,$submit_content,$pid) = @_;                                                      
        defined $pid || die "USAGE of emit_dag_files: emit_dag_files(<TESTNAME>,<SUBMIT_CONTENT>,<PROCESSID>)\n";
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
