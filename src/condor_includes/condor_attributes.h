/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


// List of attributes used in ClassAds  If you find yourself using anything
// other than ATTR_<blah> to add/lookup expressions/variables *STOP*, add the
// new variable to this file and use the ATTR_<blah> symbolic constant.  --RR

#ifndef __CONDOR_ATTRIBUTES_H__
#define __CONDOR_ATTRIBUTES_H__

const char ATTR_ACCOUNTING_GROUP         [] = "AccountingGroup";
const char ATTR_ACTION_CONSTRAINT		 [] = "ActionConstraint";
const char ATTR_ACTION_IDS				 [] = "ActionIds";
const char ATTR_ACTION_RESULT			 [] = "ActionResult";
const char ATTR_ACTION_RESULT_TYPE		 [] = "ActionResultType";
const char ATTR_ACTIVITY				 [] = "Activity";
const char ATTR_ALLOW_NOTIFICATION_CC    [] = "AllowNotificationCC";
const char ATTR_APPEND_FILES			[] = "AppendFiles";
const char ATTR_ARCH                     [] = "Arch";
const char ATTR_AVAIL_BANDWIDTH_TO_SUBMIT_MACHINE[] = "AvailBandwidthToSubmitMachine";
const char ATTR_AVAIL_BANDWIDTH_TO_LAST_CKPT_SERVER[] = "AvailBandwidthToLastCkptServer";
const char ATTR_AVAIL_BANDWIDTH_TO_CKPT_SERVER [] = "AvailBandwidthToCkptServer";
const char ATTR_AVAIL_SINCE				 [] = "AvailSince";
const char ATTR_AVAIL_TIME				 [] = "AvailTime";
const char ATTR_AVAIL_TIME_ESTIMATE		 [] = "AvailTimeEstimate";
const char ATTR_BANDWIDTH_TO_SUBMIT_MACHINE[] = "BandwidthToSubmitMachine";
const char ATTR_BANDWIDTH_TO_LAST_CKPT_SERVER[] = "BandwidthToLastCkptServer";
const char ATTR_BANDWIDTH_TO_CKPT_SERVER [] = "BandwidthToCkptServer";
const char ATTR_BUFFER_SIZE				 [] = "BufferSize";
const char ATTR_BUFFER_FILES				 [] = "BufferFiles";
const char ATTR_BUFFER_BLOCK_SIZE		 [] = "BufferBlockSize";
const char ATTR_BUFFER_BLOCKS_USED	 [] = "BufferBlocksUsed";
const char ATTR_BUFFER_PREFETCH_SIZE	 [] = "BufferPrefetchSize";
const char ATTR_BYTES_SENT				 [] = "BytesSent";
const char ATTR_BYTES_RECVD				 [] = "BytesRecvd";
const char ATTR_CAPABILITY				 [] = "Capability";
const char ATTR_CKPT_SERVER				 [] = "CkptServer";
const char ATTR_COD_CLAIMS               [] = "CODClaims";
const char ATTR_COMMAND					 [] = "Command";
const char ATTR_COMPRESS_FILES		 [] = "CompressFiles";
const char ATTR_REQUESTED_CAPACITY		 [] = "RequestedCapacity";
const char ATTR_CKPT_ARCH				 [] = "CkptArch";
const char ATTR_CKPT_LAST_READ			 [] = "CkptLastRead";
const char ATTR_CKPT_OPSYS				 [] = "CkptOpSys";
const char ATTR_CLAIM_ID                 [] = "ClaimId";
const char ATTR_CLAIM_STATE              [] = "ClaimState";
const char ATTR_CLAIM_TYPE               [] = "ClaimType";
const char ATTR_CLIENT_MACHINE           [] = "ClientMachine"; 
const char ATTR_CLOCK_DAY                [] = "ClockDay";
const char ATTR_CLOCK_MIN                [] = "ClockMin";
const char ATTR_CLUSTER_ID               [] = "ClusterId";
const char ATTR_AUTO_CLUSTER_ID			 [] = "AutoClusterId";
const char ATTR_COMPLETION_DATE			 [] = "CompletionDate";
#define ATTR_CONDOR_LOAD_AVG			AttrGetName( ATTRE_CONDOR_LOAD_AVG )
#define ATTR_CONDOR_ADMIN				AttrGetName( ATTRE_CONDOR_ADMIN )
const char ATTR_CONSOLE_IDLE			 [] = "ConsoleIdle";
const char ATTR_CONTINUE                 [] = "Continue";
const char ATTR_CORE_SIZE				 [] = "CoreSize";
const char ATTR_CPU_BUSY                 [] = "CpuBusy";
const char ATTR_CPU_BUSY_TIME            [] = "CpuBusyTime";
const char ATTR_CPU_IS_BUSY              [] = "CpuIsBusy";
const char ATTR_CPUS                     [] = "Cpus";
const char ATTR_CURRENT_HOSTS			 [] = "CurrentHosts";
const char ATTR_CURRENT_JOBS_RUNNING     [] = "CurrentJobsRunning";
const char ATTR_CURRENT_RANK			 [] = "CurrentRank";
const char ATTR_DAEMON_START_TIME		 [] = "DaemonStartTime";
const char ATTR_DAG_NODE_NAME			 [] = "DAGNodeName";
const char ATTR_DAGMAN_JOB_ID			 [] = "DAGManJobId";
const char ATTR_DESTINATION				 [] = "Destination";
const char ATTR_DISK                     [] = "Disk";
const char ATTR_DISK_USAGE				 [] = "DiskUsage";
const char ATTR_EMAIL_ATTRIBUTES         [] = "EmailAttributes";
const char ATTR_ENTERED_CURRENT_ACTIVITY [] = "EnteredCurrentActivity";
const char ATTR_ENTERED_CURRENT_STATE	 [] = "EnteredCurrentState";
const char ATTR_ENTERED_CURRENT_STATUS	 [] = "EnteredCurrentStatus";
const char ATTR_ERROR_STRING             [] = "ErrorString";
const char ATTR_EXCEPTION_HIERARCHY      [] = "ExceptionHierarchy";
const char ATTR_EXCEPTION_NAME           [] = "ExceptionName";
const char ATTR_EXCEPTION_TYPE           [] = "ExceptionType";
const char ATTR_EXECUTABLE_SIZE			 [] = "ExecutableSize";
const char ATTR_EXIT_REASON              [] = "ExitReason";
const char ATTR_FETCH_FILES			 [] = "FetchFiles";
const char ATTR_FILE_NAME				 [] = "FileName";
const char ATTR_FILE_SIZE				 [] = "FileSize";
const char ATTR_FILE_SYSTEM_DOMAIN       [] = "FileSystemDomain";
const char ATTR_FILE_REMAPS				 [] = "FileRemaps";
const char ATTR_FILE_READ_COUNT		[] = "FileReadCount";
const char ATTR_FILE_READ_BYTES		[] = "FileReadBytes";
const char ATTR_FILE_WRITE_COUNT	[] = "FileWriteCount";
const char ATTR_FILE_WRITE_BYTES	[] = "FileWriteBytes";
const char ATTR_FILE_SEEK_COUNT		[] = "FileSeekCount";
const char ATTR_FLOCKED_JOBS			 [] = "FlockedJobs";
const char ATTR_FLAVOR                   [] = "Flavor";
const char ATTR_FORCE					 [] = "Force";
const char ATTR_GID						 [] = "Gid";
const char ATTR_GLOBAL_JOB_ID            [] = "GlobalJobId";
const char ATTR_GZIP					 [] = "GZIP";
const char ATTR_GLOBUS_CONTACT_STRING	 [] = "GlobusContactString";
const char ATTR_GLOBUS_DELEGATION_URI	 [] = "GlobusDelegationUri";
const char ATTR_GLOBUS_GRAM_VERSION		 [] = "GlobusGramVersion";
const char ATTR_GLOBUS_RESOURCE			 [] = "GlobusResource";
const char ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME [] = "GlobusResourceUnavailableTime";
const char ATTR_JOB_MUST_EXPAND			 [] = "MustExpand";
const char ATTR_GLOBUS_RSL				 [] = "GlobusRSL";
const char ATTR_GLOBUS_STATUS			 [] = "GlobusStatus";
const char ATTR_X509_USER_PROXY          [] = "x509userproxy";
const char ATTR_X509_USER_PROXY_SUBJECT	 [] = "x509userproxysubject";
const char ATTR_GLOBUS_JOBMANAGER_TYPE   []	= "JobmanagerType";	// for gt4
const char ATTR_GLOBUS_SUBMIT_ID	     [] = "GlobusSubmitId"; // for gt4
// ckireyev myproxy
const char ATTR_MYPROXY_SERVER_DN		 [] = "MyProxyServerDN";
const char ATTR_MYPROXY_HOST_NAME		 [] = "MyProxyHost";
const char ATTR_MYPROXY_PASSWORD		 [] = "MyProxyPassword";
const char ATTR_MYPROXY_CRED_NAME		 [] = "MyProxyCredentialName";
const char ATTR_MYPROXY_REFRESH_THRESHOLD [] = "MyProxyRefreshThreshold";
const char ATTR_MYPROXY_NEW_PROXY_LIFETIME [] = "MyProxyNewProxyLifetime";
// END ckireyev myproxy
const char ATTR_HAS_CHECKPOINTING        [] = "HasCheckpointing";
const char ATTR_HAS_FILE_TRANSFER        [] = "HasFileTransfer";
const char ATTR_HAS_PER_FILE_ENCRYPTION  [] = "HasPerFileEncryption";
const char ATTR_HAS_IO_PROXY             [] = "HasIOProxy";
const char ATTR_HAS_JAVA                 [] = "HasJava";
const char ATTR_HAS_JIC_LOCAL_CONFIG     [] = "HasJICLocalConfig";
const char ATTR_HAS_JIC_LOCAL_STDIN      [] = "HasJICLocalStdin";
const char ATTR_HAS_JOB_AD               [] = "HasJobAd";
const char ATTR_HAS_JOB_AD_FROM_FILE     [] = "HasJobAdFromFile";
const char ATTR_HAS_MPI                  [] = "HasMPI";
const char ATTR_HAS_OLD_VANILLA          [] = "HasOldVanilla";
const char ATTR_HAS_PVM                  [] = "HasPVM";
const char ATTR_HAS_RECONNECT            [] = "HasReconnect";
const char ATTR_HAS_REMOTE_SYSCALLS      [] = "HasRemoteSyscalls";
const char ATTR_HAS_TDP                  [] = "HasTDP";
const char ATTR_HAS_SOAP_API            [] = "HasSOAPInterface";
const char ATTR_HELD_JOBS				 [] = "HeldJobs";
const char ATTR_HOLD_KILL_SIG            [] = "HoldKillSig";
const char ATTR_IDLE_JOBS                [] = "IdleJobs";
const char ATTR_IMAGE_SIZE				 [] = "ImageSize";
const char ATTR_IS_DAEMON_CORE           [] = "IsDaemonCore";
const char ATTR_IS_OWNER                 [] = "IsOwner";
const char ATTR_IS_QUEUE_SUPER_USER      [] = "IsQueueSuperUser";
const char ATTR_INACTIVE                 [] = "Inactive";
const char ATTR_JAR_FILES                [] = "JarFiles";
const char ATTR_JAVA_MFLOPS              [] = "JavaMFlops";
const char ATTR_JAVA_VENDOR              [] = "JavaVendor";
const char ATTR_JAVA_VERSION             [] = "JavaVersion";
const char ATTR_JOB_ACTION               [] = "JobAction";
const char ATTR_JOB_ARGUMENTS			 [] = "Args";
const char ATTR_JOB_CMD					 [] = "Cmd";
const char ATTR_JOB_CMDEXT				 [] = "CmdExt";
const char ATTR_JOB_CORE_DUMPED			 [] = "JobCoreDumped";
const char ATTR_JOB_CURRENT_START_DATE	 [] = "JobCurrentStartDate";
const char ATTR_JOB_DURATION			 [] = "JobDuration";
const char ATTR_JOB_ENVIRONMENT			 [] = "Env";
const char ATTR_JOB_ERROR				 [] = "Err";
const char ATTR_JOB_ERROR_ORIG			 [] = "ErrOrig";
const char ATTR_JOB_ERROR_SIZE			 [] = "ErrSize";
const char ATTR_JOB_KEYWORD              [] = "JobKeyword";
const char ATTR_JOB_LEASE_DURATION       [] = "JobLeaseDuration";
const char ATTR_JOB_SPOOL_EXECUTABLE	 [] = "SpoolExecutable";
const char ATTR_JOB_EXIT_STATUS			 [] = "ExitStatus";
const char ATTR_JOB_EXIT_REQUIREMENTS	 [] = "ExitRequirements";
const char ATTR_JOB_ID					 [] = "JobId";
const char ATTR_JOB_FINISHED_HOOK_DONE   [] = "JobFinishedHookDone";
const char ATTR_JOB_INPUT				 [] = "In";
const char ATTR_JOB_IWD					 [] = "Iwd";
const char ATTR_JOB_JAVA_VM_ARGS         [] = "JavaVMArgs";
const char ATTR_JOB_REMOTE_IWD			 [] = "RemoteIwd";
const char ATTR_JOB_LOCAL_CPU			 [] = "LocalCpu";
const char ATTR_JOB_LOCAL_SYS_CPU		 [] = "LocalSysCpu";
const char ATTR_JOB_LOCAL_USER_CPU		 [] = "LocalUserCpu";
const char ATTR_JOB_MANAGED				 [] = "Managed";
const char ATTR_JOB_MATCHED				 [] = "Matched";
const char ATTR_JOB_NOOP				 [] = "IsNoopJob";
const char ATTR_JOB_NOOP_EXIT_SIGNAL	 [] = "NoopJobExitSignal";
const char ATTR_JOB_NOOP_EXIT_CODE		 [] = "NoopJobExitCode";
const char ATTR_JOB_NOTIFICATION		 [] = "JobNotification";
const char ATTR_JOB_OUTPUT				 [] = "Out";
const char ATTR_JOB_OUTPUT_ORIG			 [] = "OutOrig";
const char ATTR_JOB_OUTPUT_SIZE			 [] = "OutSize";
const char ATTR_JOB_PID                  [] = "JobPid";
const char ATTR_JOB_PRIO                 [] = "JobPrio";
const char ATTR_JOB_COMMITTED_TIME		 [] = "CommittedTime";
const char ATTR_JOB_LANGUAGE             [] = "JobLanguage";
const char ATTR_JOB_LAST_START_DATE		 [] = "JobLastStartDate";
const char ATTR_JOB_LEAVE_IN_QUEUE		 [] = "LeaveJobInQueue";
const char ATTR_JOB_REMOTE_SYS_CPU		 [] = "RemoteSysCpu";
const char ATTR_JOB_REMOTE_USER_CPU		 [] = "RemoteUserCpu";
const char ATTR_JOB_REMOTE_WALL_CLOCK	 [] = "RemoteWallClockTime";
const char ATTR_JOB_ROOT_DIR			 [] = "RootDir";
const char ATTR_JOB_RUN_COUNT			 [] = "JobRunCount";
const char ATTR_JOB_START                [] = "JobStart";
const char ATTR_JOB_START_DATE			 [] = "JobStartDate";
const char ATTR_JOB_STATE                [] = "JobState";
const char ATTR_JOB_STATUS               [] = "JobStatus";
const char ATTR_JOB_STATUS_ON_RELEASE    [] = "JobStatusOnRelease";
const char ATTR_JOB_UNIVERSE             [] = "JobUniverse";
const char ATTR_JOB_GRID_TYPE			 [] = "JobGridType";
const char ATTR_JOB_WALL_CLOCK_CKPT		 [] = "WallClockCheckpoint";
const char ATTR_KEYBOARD_IDLE            [] = "KeyboardIdle";
const char ATTR_KFLOPS                   [] = "KFlops";
const char ATTR_KILL                     [] = "Kill";
const char ATTR_KILL_SIG				 [] = "KillSig";
const char ATTR_LAST_AVAIL_INTERVAL		 [] = "LastAvailInterval";
const char ATTR_LAST_BENCHMARK			 [] = "LastBenchmark";
const char ATTR_LAST_CKPT_SERVER		 [] = "LastCkptServer";
const char ATTR_LAST_CKPT_TIME			 [] = "LastCkptTime";
const char ATTR_LAST_CLAIM_ID            [] = "LastClaimId";
const char ATTR_LAST_CLAIM_STATE         [] = "LastClaimState";
const char ATTR_LAST_VACATE_TIME		 [] = "LastVacateTime";
const char ATTR_LAST_HEARD_FROM          [] = "LastHeardFrom";
const char ATTR_LAST_HOLD_REASON         [] = "LastHoldReason";
const char ATTR_LAST_HOLD_REASON_CODE	 [] = "LastHoldReasonCode";
const char ATTR_LAST_HOLD_REASON_SUBCODE [] = "LastHoldReasonSubCode";
const char ATTR_LAST_JOB_LEASE_RENEWAL   [] = "LastJobLeaseRenewal";
const char ATTR_LAST_MATCH_TIME			 [] = "LastMatchTime";
const char ATTR_LAST_MATCH_LIST_PREFIX   [] = "LastMatchName";
const char ATTR_LAST_MATCH_LIST_LENGTH   [] = "LastMatchListLength";
const char ATTR_LAST_REJ_MATCH_TIME		 [] = "LastRejMatchTime";
const char ATTR_LAST_REJ_MATCH_REASON	 [] = "LastRejMatchReason";
const char ATTR_LAST_PERIODIC_CHECKPOINT [] = "LastPeriodicCheckpoint";
const char ATTR_LAST_RELEASE_REASON      [] = "LastReleaseReason";
const char ATTR_LAST_REMOTE_HOST		 [] = "LastRemoteHost";
const char ATTR_LAST_UPDATE				 [] = "LastUpdate";
const char ATTR_LOCAL_FILES		[] = "LocalFiles";
const char ATTR_LOAD_AVG                 [] = "LoadAvg";
const char ATTR_MACHINE                  [] = "Machine";
const char ATTR_MASTER_IP_ADDR			 [] = "MasterIpAddr";
const char ATTR_MAX_HOSTS				 [] = "MaxHosts";
const char ATTR_MAX_JOB_RETIREMENT_TIME  [] = "MaxJobRetirementTime";
const char ATTR_MAX_JOBS_RUNNING         [] = "MaxJobsRunning";
const char ATTR_MEMORY                   [] = "Memory";
const char ATTR_MIN_HOSTS				 [] = "MinHosts";
const char ATTR_MIPS                     [] = "Mips";
const char ATTR_MIRROR_ACTIVE            [] = "MirrorActive";
const char ATTR_MIRROR_JOB_ID            [] = "MirrorJobId";
const char ATTR_MIRROR_LEASE_TIME        [] = "MirrorLeaseTime";
const char ATTR_MIRROR_RELEASED          [] = "MirrorReleased";
const char ATTR_MIRROR_REMOTE_LEASE_TIME [] = "MirrorRemoteLeaseTime";
const char ATTR_MIRROR_SCHEDD            [] = "MirrorSchedd";
const char ATTR_MIRROR_SUBMITTER_ID      [] = "MirrorSubmitterId";
const char ATTR_MPI_IS_MASTER            [] = "MPIIsMaster";
const char ATTR_MPI_MASTER_ADDR       	 [] = "MPIMasterAddr";
const char ATTR_PARALLEL_IS_MASTER       [] = "ParallelIsMaster";   
const char ATTR_PARALLEL_MASTER_ADDR   	 [] = "ParallelMasterAddr"; 
const char ATTR_MY_TYPE					 [] = "MyType";
const char ATTR_NAME                     [] = "Name";
const char ATTR_NICE_USER			 	 [] = "NiceUser";
const char ATTR_NEGOTIATOR_REQUIREMENTS  [] = "NegotiatorRequirements";
const char ATTR_NEXT_CLUSTER_NUM		 [] = "NextClusterNum";
const char ATTR_NODE					 [] = "Node";
const char ATTR_NOTIFY_USER				 [] = "NotifyUser";
const char ATTR_NOTIFY_JOB_SCHEDULER     [] = "NotifyJobScheduler";
const char ATTR_NT_DOMAIN				 [] = "NTDomain";
const char ATTR_NUM_COD_CLAIMS			 [] = "NumCODClaims";
const char ATTR_NUM_CKPTS				 [] = "NumCkpts";
const char ATTR_NUM_GLOBUS_SUBMITS		 [] = "NumGlobusSubmits";
const char ATTR_NUM_MATCHES				 [] = "NumJobMatches";
const char ATTR_NUM_HOPS_TO_SUBMIT_MACHINE[] = "NumHopsToSubmitMachine";
const char ATTR_NUM_HOPS_TO_LAST_CKPT_SERVER[] = "NumHopsToLastCkptServer";
const char ATTR_NUM_HOPS_TO_CKPT_SERVER	 [] = "NumHopsToCkptServer";
const char ATTR_NUM_PIDS                 [] = "NumPids";
const char ATTR_NUM_RESTARTS			 [] = "NumRestarts";
const char ATTR_NUM_SYSTEM_HOLDS		 [] = "NumSystemHolds";
const char ATTR_NUM_USERS                [] = "NumUsers";
const char ATTR_OPSYS                    [] = "OpSys";
const char ATTR_ORIG_MAX_HOSTS			 [] = "OrigMaxHosts";
const char ATTR_OWNER                    [] = "Owner"; 
const char ATTR_PARALLEL_SCRIPT_SHADOW   [] = "ParallelScriptShadow";  
const char ATTR_PARALLEL_SCRIPT_STARTER  [] = "ParallelScriptStarter"; 
const char ATTR_PERIODIC_CHECKPOINT      [] = "PeriodicCheckpoint";
#define ATTR_PLATFORM					AttrGetName( ATTRE_PLATFORM )
const char ATTR_PREFERENCES				 [] = "Preferences";
const char ATTR_PREV_SEND_ESTIMATE		 [] = "PrevSendEstimate";
const char ATTR_PREV_RECV_ESTIMATE		 [] = "PrevRecvEstimate";
const char ATTR_PRIO                     [] = "Prio";
const char ATTR_PROC_ID                  [] = "ProcId";
const char ATTR_Q_DATE                   [] = "QDate";
const char ATTR_RANK					 [] = "Rank";
const char ATTR_REAL_UID				 [] = "RealUid";
const char ATTR_RELEASE_REASON			 [] = "ReleaseReason";
const char ATTR_REMOTE_HOST				 [] = "RemoteHost";
const char ATTR_REMOTE_JOB_ID			 [] = "RemoteJobId";
const char ATTR_REMOTE_OWNER			 [] = "RemoteOwner";
const char ATTR_REMOTE_POOL				 [] = "RemotePool";
const char ATTR_REMOTE_SCHEDD			 [] = "RemoteSchedd";
const char ATTR_REMOTE_USER              [] = "RemoteUser";
const char ATTR_REMOTE_USER_PRIO         [] = "RemoteUserPrio";
const char ATTR_REMOTE_VIRTUAL_MACHINE_ID [] = "RemoteVirtualMachineID";
const char ATTR_REMOVE_KILL_SIG          [] = "RemoveKillSig";
const char ATTR_REMOVE_REASON            [] = "RemoveReason";
const char ATTR_REQUEUE_REASON           [] = "RequeueReason";
const char ATTR_REQUIREMENTS             [] = "Requirements";
const char ATTR_RESULT                   [] = "Result";
const char ATTR_RSC_BYTES_SENT			 [] = "RSCBytesSent";
const char ATTR_RSC_BYTES_RECVD			 [] = "RSCBytesRecvd";
const char ATTR_RUNNING_JOBS             [] = "RunningJobs";
const char ATTR_RUNNING_COD_JOB          [] = "RunningCODJob";
const char ATTR_RUN_BENCHMARKS			 [] = "RunBenchmarks";
const char ATTR_SHADOW_IP_ADDR			 [] = "ShadowIpAddr";
const char ATTR_MY_ADDRESS               [] = "MyAddress";
const char ATTR_SCHEDD_IP_ADDR           [] = "ScheddIpAddr";
const char ATTR_SCHEDD_NAME				 [] = "ScheddName";
const char ATTR_SCHEDULER				 [] = "Scheduler";
const char ATTR_SHADOW_WAIT_FOR_DEBUG    [] = "ShadowWaitForDebug";
const char ATTR_SOURCE					 [] = "Source";
const char ATTR_STAGE_IN_START           [] = "StageInStart";
const char ATTR_STAGE_IN_FINISH          [] = "StageInFinish";
const char ATTR_STAGE_OUT_START          [] = "StageOutStart";
const char ATTR_STAGE_OUT_FINISH         [] = "StageOutFinish";
const char ATTR_START                    [] = "Start";
const char ATTR_STARTD_IP_ADDR           [] = "StartdIpAddr";
const char ATTR_STATE                    [] = "State";
const char ATTR_STARTER_IP_ADDR          [] = "StarterIpAddr";
const char ATTR_STARTER_ABILITY_LIST     [] = "StarterAbilityList";
const char ATTR_STARTER_ULOG_FILE        [] = "StarterUserLog";
const char ATTR_STARTER_ULOG_USE_XML     [] = "StarterUserLogUseXML";
const char ATTR_STARTER_WAIT_FOR_DEBUG   [] = "StarterWaitForDebug";
const char ATTR_STATUS					 [] = "Status";
const char ATTR_STREAM_INPUT			 [] = "StreamIn";
const char ATTR_STREAM_OUTPUT			 [] = "StreamOut";
const char ATTR_STREAM_ERROR			 [] = "StreamErr";
const char ATTR_SUBMITTER_ID			 [] = "SubmitterId";
const char ATTR_SUBMITTOR_PRIO           [] = "SubmittorPrio";
const char ATTR_SUBNET                   [] = "Subnet";
const char ATTR_SUSPEND                  [] = "Suspend";
const char ATTR_SUSPEND_JOB_AT_EXEC      [] = "SuspendJobAtExec";
const char ATTR_TARGET_TYPE				 [] = "TargetType";
const char ATTR_TIME_TO_LIVE             [] = "TimeToLive";
const char ATTR_TOTAL_CLAIM_RUN_TIME     [] = "TotalClaimRunTime";
const char ATTR_TOTAL_CLAIM_SUSPEND_TIME [] = "TotalClaimSuspendTime";
#define ATTR_TOTAL_CONDOR_LOAD_AVG			AttrGetName( ATTRE_TOTAL_LOAD )

const char ATTR_TOOL_DAEMON_ARGS         [] = "ToolDaemonArgs";
const char ATTR_TOOL_DAEMON_CMD	         [] = "ToolDaemonCmd";
const char ATTR_TOOL_DAEMON_ERROR        [] = "ToolDaemonError";
const char ATTR_TOOL_DAEMON_INPUT        [] = "ToolDaemonInput";
const char ATTR_TOOL_DAEMON_OUTPUT       [] = "ToolDaemonOutput";

const char ATTR_TOTAL_CPUS				 [] = "TotalCpus";
const char ATTR_TOTAL_DISK				 [] = "TotalDisk";
const char ATTR_TOTAL_FLOCKED_JOBS		 [] = "TotalFlockedJobs";
const char ATTR_TOTAL_REMOVED_JOBS		 [] = "TotalRemovedJobs";
const char ATTR_TOTAL_HELD_JOBS			 [] = "TotalHeldJobs";
const char ATTR_TOTAL_IDLE_JOBS			 [] = "TotalIdleJobs";
const char ATTR_TOTAL_JOB_ADS			 [] = "TotalJobAds";
const char ATTR_TOTAL_JOB_RUN_TIME       [] = "TotalJobRunTime";
const char ATTR_TOTAL_JOB_SUSPEND_TIME   [] = "TotalJobSuspendTime";
const char ATTR_TOTAL_LOAD_AVG			 [] = "TotalLoadAvg";
const char ATTR_TOTAL_MEMORY			 [] = "TotalMemory";
const char ATTR_TOTAL_RUNNING_JOBS		 [] = "TotalRunningJobs";
const char ATTR_TOTAL_VIRTUAL_MACHINES	 [] = "TotalVirtualMachines";
const char ATTR_TOTAL_VIRTUAL_MEMORY	 [] = "TotalVirtualMemory";
const char ATTR_UID						 [] = "Uid";
const char ATTR_UID_DOMAIN               [] = "UidDomain";
const char ATTR_ULOG_FILE				 [] = "UserLog";
const char ATTR_ULOG_USE_XML             [] = "UserLogUseXML";
const char ATTR_UPDATE_INTERVAL			 [] = "UpdateInterval";
const char ATTR_UPDATE_PRIO              [] = "UpdatePrio";
const char ATTR_UPDATE_SEQUENCE_NUMBER   [] = "UpdateSequenceNumber";
const char ATTR_USE_GRID_SHELL           [] = "UseGridShell";
const char ATTR_USER					 [] = "User";
const char ATTR_VACATE                   [] = "Vacate";
const char ATTR_VACATE_TYPE              [] = "VacateType";
const char ATTR_VIRTUAL_MEMORY           [] = "VirtualMemory";
const char ATTR_WANT_CHECKPOINT		 	 [] = "WantCheckpoint";
const char ATTR_WANT_CLAIMING			 [] = "WantClaiming";
const char ATTR_WANT_IO_PROXY		[] = "WantIOProxy";
const char ATTR_WANT_MATCH_DIAGNOSTICS	 [] = "WantMatchDiagnostics";
const char ATTR_WANT_REMOTE_SYSCALLS 	 [] = "WantRemoteSyscalls";
const char ATTR_WANT_REMOTE_IO 			 [] = "WantRemoteIO";
const char ATTR_WANT_SUBMIT_NET_STATS	 [] = "WantSubmitNetStats";
const char ATTR_WANT_LAST_CKPT_SERVER_NET_STATS[] = "WantLastCkptServerNetStats";
const char ATTR_WANT_CKPT_SERVER_NET_STATS[] = "WantCkptServerNetStats";
const char ATTR_WANT_AD_REVAULATE        [] = "WantAdRevaluate";
const char ATTR_COLLECTOR_IP_ADDR        [] = "CollectorIpAddr";
const char ATTR_NEGOTIATOR_IP_ADDR		 []	= "NegotiatorIpAddr";
const char ATTR_NUM_HOSTS_TOTAL			 [] = "HostsTotal";
const char ATTR_NUM_HOSTS_CLAIMED		 [] = "HostsClaimed";
const char ATTR_NUM_HOSTS_UNCLAIMED		 [] = "HostsUnclaimed";
const char ATTR_NUM_HOSTS_OWNER			 [] = "HostsOwner";
const char ATTR_MAX_RUNNING_JOBS			 [] = "MaxRunningJobs";
#define ATTR_VERSION					AttrGetName( ATTRE_VERSION )
const char ATTR_SCHEDD_BIRTHDATE		 [] = "ScheddBday";
const char ATTR_SHADOW_VERSION			 [] = "ShadowVersion";
const char ATTR_VIRTUAL_MACHINE_ID		 [] = "VirtualMachineID";
const char ATTR_SHOULD_TRANSFER_FILES    [] = "ShouldTransferFiles";
const char ATTR_WHEN_TO_TRANSFER_OUTPUT  [] = "WhenToTransferOutput";
const char ATTR_TRANSFER_TYPE			 [] = "TransferType";
const char ATTR_TRANSFER_FILES			 [] = "TransferFiles";
const char ATTR_TRANSFER_KEY			 [] = "TransferKey";
const char ATTR_TRANSFER_EXECUTABLE		 [] = "TransferExecutable";
const char ATTR_TRANSFER_INPUT			 [] = "TransferIn";
const char ATTR_TRANSFER_OUTPUT			 [] = "TransferOut";
const char ATTR_TRANSFER_ERROR			 [] = "TransferErr";
const char ATTR_TRANSFER_INPUT_FILES	 [] = "TransferInput";
const char ATTR_TRANSFER_INTERMEDIATE_FILES [] = "TransferIntermediate";
const char ATTR_TRANSFER_OUTPUT_FILES	 [] = "TransferOutput";
const char ATTR_ENCRYPT_INPUT_FILES		 [] = "EncryptInputFiles";
const char ATTR_ENCRYPT_OUTPUT_FILES	 [] = "EncryptOutputFiles";
const char ATTR_DONT_ENCRYPT_INPUT_FILES [] = "DontEncryptInputFiles";
const char ATTR_DONT_ENCRYPT_OUTPUT_FILES[] = "DontEncryptOutputFiles";
const char ATTR_TRANSFER_SOCKET			 [] = "TransferSocket";
const char ATTR_SERVER_TIME				 [] = "ServerTime";
const char ATTR_SHADOW_BIRTHDATE		 [] = "ShadowBday";
const char ATTR_HOLD_REASON				 [] = "HoldReason";
const char ATTR_HOLD_REASON_CODE		 [] = "HoldReasonCode";
const char ATTR_HOLD_REASON_SUBCODE		 [] = "HoldReasonSubCode";
const char ATTR_WANT_MATCHING			 [] = "WantMatching";
const char ATTR_WANT_RESOURCE_AD		 [] = "WantResAd";
const char ATTR_TOTAL_SUSPENSIONS        [] = "TotalSuspensions";
const char ATTR_LAST_SUSPENSION_TIME     [] = "LastSuspensionTime";
const char ATTR_CUMULATIVE_SUSPENSION_TIME[] = "CumulativeSuspensionTime";

const char ATTR_ON_EXIT_BY_SIGNAL        [] = "ExitBySignal";
const char ATTR_ON_EXIT_CODE		     [] = "ExitCode";
const char ATTR_ON_EXIT_HOLD_CHECK		 [] = "OnExitHold";
const char ATTR_ON_EXIT_REMOVE_CHECK	 [] = "OnExitRemove";
const char ATTR_ON_EXIT_SIGNAL		     [] = "ExitSignal";
const char ATTR_PERIODIC_HOLD_CHECK		 [] = "PeriodicHold";
const char ATTR_PERIODIC_RELEASE_CHECK	 [] = "PeriodicRelease";
const char ATTR_PERIODIC_REMOVE_CHECK	 [] = "PeriodicRemove";
const char ATTR_GLOBUS_RESUBMIT_CHECK	 [] = "GlobusResubmit";
const char ATTR_REMATCH_CHECK			 [] = "Rematch";

const char ATTR_SEC_AUTHENTICATION_METHODS_LIST[] = "AuthMethodsList";
const char ATTR_SEC_AUTHENTICATION_METHODS[] = "AuthMethods";
const char ATTR_SEC_CRYPTO_METHODS       [] = "CryptoMethods";
const char ATTR_SEC_AUTHENTICATION       [] = "Authentication";
const char ATTR_SEC_ENCRYPTION           [] = "Encryption";
const char ATTR_SEC_INTEGRITY            [] = "Integrity";
const char ATTR_SEC_ENACT                [] = "Enact";
const char ATTR_SEC_RESPOND              [] = "Respond";
const char ATTR_SEC_COMMAND              [] = "Command";
const char ATTR_SEC_AUTH_COMMAND         [] = "AuthCommand";
const char ATTR_SEC_SID                  [] = "Sid";
const char ATTR_SEC_SUBSYSTEM            [] = "Subsystem";
const char ATTR_SEC_REMOTE_VERSION       [] = "RemoteVersion";
const char ATTR_SEC_SERVER_ENDPOINT      [] = "ServerEndpoint";
const char ATTR_SEC_SERVER_COMMAND_SOCK  [] = "ServerCommandSock";
const char ATTR_SEC_SERVER_PID           [] = "ServerPid";
const char ATTR_SEC_PARENT_UNIQUE_ID     [] = "ParentUniqueID";
const char ATTR_SEC_PACKET_COUNT         [] = "PacketCount";
const char ATTR_SEC_NEGOTIATION          [] = "OutgoingNegotiation";
const char ATTR_SEC_VALID_COMMANDS       [] = "ValidCommands";
const char ATTR_SEC_SESSION_DURATION     [] = "SessionDuration";
const char ATTR_SEC_USER                 [] = "User";
const char ATTR_SEC_NEW_SESSION          [] = "NewSession";
const char ATTR_SEC_USE_SESSION          [] = "UseSession";
const char ATTR_SEC_COOKIE               [] = "Cookie";

const char ATTR_MULTIPLE_TASKS_PER_PVMD  [] = "MultipleTasksPerPvmd";

const char ATTR_UPDATESTATS_TOTAL		 [] = "UpdatesTotal";
const char ATTR_UPDATESTATS_SEQUENCED	 [] = "UpdatesSequenced";
const char ATTR_UPDATESTATS_LOST			 [] = "UpdatesLost";
const char ATTR_UPDATESTATS_HISTORY		 [] = "UpdatesHistory";


// Enumerate the ones that can't be constant strings..
typedef enum
{
	ATTRE_CONDOR_LOAD_AVG = 0,
	ATTRE_CONDOR_ADMIN,
	ATTRE_PLATFORM,
	ATTRE_TOTAL_LOAD,
	ATTRE_VERSION,
	// ....
} CONDOR_ATTR;

// Prototypes
int AttrInit( void );
const char *AttrGetName( CONDOR_ATTR );


// ------------------------------------------------------
// Stuff private to the environment variable manager
// ------------------------------------------------------
#if defined _CONDOR_ATTR_MAIN

// Flags available
typedef enum
{
	ATTR_FLAG_NONE = 0,			// No special treatment
	ATTR_FLAG_DISTRO,			// Plug in the distribution name
	ATTR_FLAG_DISTRO_UC,		// Plug in the UPPER CASE distribution name
	ATTR_FLAG_DISTRO_CAP,		// Plug in the Capitolized distribution name
} CONDOR_ATTR_FLAGS;

// Data on each env variable
typedef struct
{
	CONDOR_ATTR			sanity;		// Used to sanity check
	const char			*string;	// My format string
	CONDOR_ATTR_FLAGS	flag;		// Flags
	const char			*cached;	// Cached answer
} CONDOR_ATTR_ELEM;

// The actual list of variables indexed by CONDOR_ATTR
static CONDOR_ATTR_ELEM CondorAttrList[] =
{
	{ ATTRE_CONDOR_LOAD_AVG,"%sLoadAvg",		ATTR_FLAG_DISTRO_CAP },
	{ ATTRE_CONDOR_ADMIN,	"%sAdmin",			ATTR_FLAG_DISTRO_CAP },
	{ ATTRE_PLATFORM,		"%sPlatform",		ATTR_FLAG_DISTRO_CAP },
	{ ATTRE_TOTAL_LOAD,		"Total%sLoadAvg",	ATTR_FLAG_DISTRO_CAP },
	{ ATTRE_VERSION,		"%sVersion",		ATTR_FLAG_DISTRO_CAP },
	// ....
};
#endif		// _CONDOR_ATTR_MAIN

#endif
