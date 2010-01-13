/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_distribution.h"
#define _CONDOR_ATTR_MAIN
#include "condor_attributes.h"

// Initialize our logic
int
AttrInit( void )
{
    unsigned	i;
    for ( i=0;  i<sizeof(CondorAttrList)/sizeof(CONDOR_ATTR_ELEM);  i++ )
    {
		// Sanity check
		if ( (unsigned) CondorAttrList[i].sanity != i ) {
			fprintf( stderr, "Attribute sanity check failed!!\n" );
			return -1;
		}
        CondorAttrList[i].cached = NULL;
    }
	return 0;
}

// Get an attribute variable's name
const char *
AttrGetName( CONDOR_ATTR which )
{
    CONDOR_ATTR_ELEM	*local = &CondorAttrList[which];

	// Simple case first; out of cache
    if ( local->cached ) {
        return local->cached;
    }

	// Otherwise, fill the cache
	char	*tmps;
	switch ( local->flag )
	{
	case  ATTR_FLAG_NONE:
        tmps = (char *) local->string;
		break;
    case ATTR_FLAG_DISTRO:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->Get() );
		break;
    case ATTR_FLAG_DISTRO_UC:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->GetUc() );
		break;
    case ATTR_FLAG_DISTRO_CAP:
		// Yeah, this allocates a couple more bytes than required, but
		// oh well...
        tmps = (char *) malloc( strlen(local->string) + myDistro->GetLen() );
        sprintf( tmps, local->string, myDistro->GetCap() );
		break;
    }

	// Then, return it
	return ( local->cached = (const char * ) tmps );
}

// List of attributes used in ClassAds  If you find yourself using anything
// other than ATTR_<blah> to add/lookup expressions/variables *STOP*, add the
// new variable to this file and use the ATTR_<blah> symbolic constant.  --RR

const char * const ATTR_ACCOUNTING_GROUP         = "AccountingGroup";
const char * const ATTR_ACTION_CONSTRAINT		 = "ActionConstraint";
const char * const ATTR_ACTION_IDS				 = "ActionIds";
const char * const ATTR_ACTION_RESULT			 = "ActionResult";
const char * const ATTR_ACTION_RESULT_TYPE		 = "ActionResultType";
const char * const ATTR_ACTIVITY				 = "Activity";
const char * const ATTR_ALLOW_NOTIFICATION_CC    = "AllowNotificationCC";
const char * const ATTR_ALL_REMOTE_HOSTS	    = "AllRemoteHosts";
const char * const ATTR_APPEND_FILES			= "AppendFiles";
const char * const ATTR_ARCH                     = "Arch";
const char * const ATTR_AVAIL_BANDWIDTH_TO_SUBMIT_MACHINE= "AvailBandwidthToSubmitMachine";
const char * const ATTR_AVAIL_BANDWIDTH_TO_LAST_CKPT_SERVER= "AvailBandwidthToLastCkptServer";
const char * const ATTR_AVAIL_BANDWIDTH_TO_CKPT_SERVER = "AvailBandwidthToCkptServer";
const char * const ATTR_AVAIL_SINCE				 = "AvailSince";
const char * const ATTR_AVAIL_TIME				 = "AvailTime";
const char * const ATTR_AVAIL_TIME_ESTIMATE		 = "AvailTimeEstimate";
const char * const ATTR_BANDWIDTH_TO_SUBMIT_MACHINE= "BandwidthToSubmitMachine";
const char * const ATTR_BANDWIDTH_TO_LAST_CKPT_SERVER= "BandwidthToLastCkptServer";
const char * const ATTR_BANDWIDTH_TO_CKPT_SERVER = "BandwidthToCkptServer";
const char * const ATTR_BUFFER_SIZE				 = "BufferSize";
const char * const ATTR_BUFFER_FILES				 = "BufferFiles";
const char * const ATTR_BUFFER_BLOCK_SIZE		 = "BufferBlockSize";
const char * const ATTR_BUFFER_BLOCKS_USED	 = "BufferBlocksUsed";
const char * const ATTR_BUFFER_PREFETCH_SIZE	 = "BufferPrefetchSize";
const char * const ATTR_BYTES_SENT				 = "BytesSent";
const char * const ATTR_BYTES_RECVD				 = "BytesRecvd";
const char * const ATTR_CAN_HIBERNATE           = "CanHibernate";
const char * const ATTR_CAPABILITY				 = "Capability";
const char * const ATTR_CKPT_SERVER				 = "CkptServer";
const char * const ATTR_COD_CLAIMS               = "CODClaims";
const char * const ATTR_COMMAND					 = "Command";
const char * const ATTR_COMPRESS_FILES		 = "CompressFiles";
const char * const ATTR_REQUESTED_CAPACITY		 = "RequestedCapacity";
const char * const ATTR_CKPT_ARCH				 = "CkptArch";
const char * const ATTR_CKPT_LAST_READ			 = "CkptLastRead";
const char * const ATTR_CKPT_OPSYS				 = "CkptOpSys";
const char * const ATTR_CLAIM_ID                 = "ClaimId";
const char * const ATTR_CLAIM_IDS                = "ClaimIds";
const char * const ATTR_PUBLIC_CLAIM_ID          = "PublicClaimId";
const char * const ATTR_PUBLIC_CLAIM_IDS         = "PublicClaimIds";
const char * const ATTR_CLAIM_STATE              = "ClaimState";
const char * const ATTR_CLAIM_TYPE               = "ClaimType";
const char * const ATTR_CLIENT_MACHINE           = "ClientMachine"; 
const char * const ATTR_CLOCK_DAY                = "ClockDay";
const char * const ATTR_CLOCK_MIN                = "ClockMin";
const char * const ATTR_CLUSTER_ID               = "ClusterId";
const char * const ATTR_AUTO_CLUSTER_ID			 = "AutoClusterId";
const char * const ATTR_AUTO_CLUSTER_ATTRS		 = "AutoClusterAttrs";
const char * const ATTR_COMPLETION_DATE			 = "CompletionDate";
const char * const ATTR_MATCHED_CONCURRENCY_LIMITS = "MatchedConcurrencyLimits";
const char * const ATTR_CONCURRENCY_LIMITS = "ConcurrencyLimits";
const char * const ATTR_PREEMPTING_CONCURRENCY_LIMITS = "PreemptingConcurrencyLimits";
#define ATTR_CONDOR_LOAD_AVG			AttrGetName( ATTRE_CONDOR_LOAD_AVG )
#define ATTR_CONDOR_ADMIN				AttrGetName( ATTRE_CONDOR_ADMIN )
const char * const ATTR_CONSOLE_IDLE			 = "ConsoleIdle";
const char * const ATTR_CONTINUE                 = "Continue";
const char * const ATTR_CORE_SIZE				 = "CoreSize";
const char * const ATTR_CREAM_ATTRIBUTES		 = "CreamAttributes";
const char * const ATTR_CRON_MINUTES			 = "CronMinute";
const char * const ATTR_CRON_HOURS				 = "CronHour";
const char * const ATTR_CRON_DAYS_OF_MONTH		 = "CronDayOfMonth";
const char * const ATTR_CRON_MONTHS				 = "CronMonth";
const char * const ATTR_CRON_DAYS_OF_WEEK		 = "CronDayOfWeek";
const char * const ATTR_CRON_NEXT_RUNTIME		 = "CronNextRunTime";
const char * const ATTR_CRON_CURRENT_TIME_RANGE	 = "CronCurrentTimeRange";
const char * const ATTR_CRON_PREP_TIME			 = "CronPrepTime";
const char * const ATTR_CRON_WINDOW			 = "CronWindow";
const char * const ATTR_CPU_BUSY                 = "CpuBusy";
const char * const ATTR_CPU_BUSY_TIME            = "CpuBusyTime";
const char * const ATTR_CPU_IS_BUSY              = "CpuIsBusy";
const char * const ATTR_CPUS                     = "Cpus";
const char * const ATTR_CURRENT_HOSTS			 = "CurrentHosts";
const char * const ATTR_CURRENT_JOBS_RUNNING     = "CurrentJobsRunning";
const char * const ATTR_CURRENT_RANK			 = "CurrentRank";
const char * const ATTR_CURRENT_STATUS_UNKNOWN  = "CurrentStatusUnknown";
const char * const ATTR_CURRENT_TIME			 = "CurrentTime";
const char * const ATTR_DAEMON_START_TIME		 = "DaemonStartTime";
const char * const ATTR_DAEMON_SHUTDOWN		 = "DaemonShutdown";
const char * const ATTR_DAEMON_SHUTDOWN_FAST	 = "DaemonShutdownFast";
const char * const ATTR_DAG_NODE_NAME			 = "DAGNodeName";
const char * const ATTR_DAG_NODE_NAME_ALT		 = "dag_node_name";
const char * const ATTR_DAGMAN_JOB_ID			 = "DAGManJobId";
const char * const ATTR_DEFERRAL_OFFSET			 = "DeferralOffset";
const char * const ATTR_DEFERRAL_PREP_TIME		 = "DeferralPrepTime";
const char * const ATTR_DEFERRAL_TIME			 = "DeferralTime";
const char * const ATTR_DEFERRAL_WINDOW			 = "DeferralWindow";
const char * const ATTR_DESTINATION				 = "Destination";
const char * const ATTR_DISK                     = "Disk";
const char * const ATTR_DISK_USAGE				 = "DiskUsage";
const char * const ATTR_EMAIL_ATTRIBUTES         = "EmailAttributes";
const char * const ATTR_ENTERED_CURRENT_ACTIVITY = "EnteredCurrentActivity";
const char * const ATTR_ENTERED_CURRENT_STATE	 = "EnteredCurrentState";
const char * const ATTR_ENTERED_CURRENT_STATUS	 = "EnteredCurrentStatus";
const char * const ATTR_ERROR_STRING             = "ErrorString";
const char * const ATTR_EXCEPTION_HIERARCHY      = "ExceptionHierarchy";
const char * const ATTR_EXCEPTION_NAME           = "ExceptionName";
const char * const ATTR_EXCEPTION_TYPE           = "ExceptionType";
const char * const ATTR_EXECUTABLE_SIZE			 = "ExecutableSize";
const char * const ATTR_EXIT_REASON              = "ExitReason";
const char * const ATTR_FETCH_FILES			 = "FetchFiles";
const char * const ATTR_FETCH_WORK_DELAY        = "FetchWorkDelay";
const char * const ATTR_FILE_NAME				 = "FileName";
const char * const ATTR_FILE_SIZE				 = "FileSize";
const char * const ATTR_FILE_SYSTEM_DOMAIN       = "FileSystemDomain";
const char * const ATTR_FILE_REMAPS				 = "FileRemaps";
const char * const ATTR_FILE_READ_COUNT		= "FileReadCount";
const char * const ATTR_FILE_READ_BYTES		= "FileReadBytes";
const char * const ATTR_FILE_WRITE_COUNT	= "FileWriteCount";
const char * const ATTR_FILE_WRITE_BYTES	= "FileWriteBytes";
const char * const ATTR_FILE_SEEK_COUNT		= "FileSeekCount";
const char * const ATTR_FLOCKED_JOBS			 = "FlockedJobs";
const char * const ATTR_FLAVOR                   = "Flavor";
const char * const ATTR_FORCE					 = "Force";
const char * const ATTR_GID						 = "Gid";
const char * const ATTR_GLOBAL_JOB_ID            = "GlobalJobId";
const char * const ATTR_GZIP					 = "GZIP";
const char * const ATTR_GLOBUS_DELEGATION_URI	 = "GlobusDelegationUri";
// Deprecated (cruft) -- no longer used
const char * const ATTR_GLOBUS_GRAM_VERSION		 = "GlobusGramVersion";
const char * const ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME = "GlobusResourceUnavailableTime";
const char * const ATTR_JOB_MUST_EXPAND			 = "MustExpand";
const char * const ATTR_GLOBUS_RSL				 = "GlobusRSL";
const char * const ATTR_GLOBUS_STATUS			 = "GlobusStatus";
const char * const ATTR_GLOBUS_XML				 = "GlobusXML";
const char * const ATTR_X509_USER_PROXY          = "x509userproxy";
const char * const ATTR_X509_USER_PROXY_SUBJECT	 = "x509userproxysubject";
const char * const ATTR_X509_USER_PROXY_VONAME	 = "x509UserProxyVOName";
const char * const ATTR_X509_USER_PROXY_FIRST_FQAN	 = "x509UserProxyFirstFQAN";
const char * const ATTR_X509_USER_PROXY_FQAN	 = "x509UserProxyFQAN";
const char * const ATTR_DELEGATED_PROXY_EXPIRATION = "DelegatedProxyExpiration";
const char * const ATTR_GRIDFTP_SERVER_JOB       = "GridftpServerJob";
const char * const ATTR_GRIDFTP_URL_BASE         = "GridftpUrlBase";
const char * const ATTR_REQUESTED_GRIDFTP_URL_BASE = "RequestedGridftpUrlBase";
const char * const ATTR_GRID_RESOURCE			 = "GridResource";
const char * const ATTR_GRID_RESOURCE_UNAVAILABLE_TIME = "GridResourceUnavailableTime";
const char * const ATTR_GRID_JOB_ID				 = "GridJobId";
const char * const ATTR_GRID_JOB_STATUS			 = "GridJobStatus";
// ckireyev myproxy
const char * const ATTR_MYPROXY_SERVER_DN		 = "MyProxyServerDN";
const char * const ATTR_MYPROXY_HOST_NAME		 = "MyProxyHost";
const char * const ATTR_MYPROXY_PASSWORD		 = "MyProxyPassword";
const char * const ATTR_MYPROXY_PASSWORD_EXISTS	 = "MyProxyPasswordExists";
const char * const ATTR_MYPROXY_CRED_NAME		 = "MyProxyCredentialName";
const char * const ATTR_MYPROXY_REFRESH_THRESHOLD = "MyProxyRefreshThreshold";
const char * const ATTR_MYPROXY_NEW_PROXY_LIFETIME = "MyProxyNewProxyLifetime";
// END ckireyev myproxy
const char * const ATTR_HARDWARE_ADDRESS         = "HardwareAddress";
const char * const ATTR_HAS_CHECKPOINTING        = "HasCheckpointing";
const char * const ATTR_HAS_FILE_TRANSFER        = "HasFileTransfer";
const char * const ATTR_HAS_PER_FILE_ENCRYPTION  = "HasPerFileEncryption";
const char * const ATTR_HAS_IO_PROXY             = "HasIOProxy";
const char * const ATTR_HAS_JAVA                 = "HasJava";
const char * const ATTR_HAS_JIC_LOCAL_CONFIG     = "HasJICLocalConfig";
const char * const ATTR_HAS_JIC_LOCAL_STDIN      = "HasJICLocalStdin";
const char * const ATTR_HAS_JOB_AD               = "HasJobAd";
const char * const ATTR_HAS_JOB_AD_FROM_FILE     = "HasJobAdFromFile";
const char * const ATTR_HAS_JOB_DEFERRAL         = "HasJobDeferral";
const char * const ATTR_HAS_MPI                  = "HasMPI";
const char * const ATTR_HAS_OLD_VANILLA          = "HasOldVanilla";
const char * const ATTR_HAS_PVM                  = "HasPVM";
const char * const ATTR_HAS_RECONNECT            = "HasReconnect";
const char * const ATTR_HAS_REMOTE_SYSCALLS      = "HasRemoteSyscalls";
const char * const ATTR_HAS_TDP                  = "HasTDP";
const char * const ATTR_HAS_SOAP_API            = "HasSOAPInterface";
const char * const ATTR_HAS_VM                   = "HasVM";
const char * const ATTR_HAS_WIN_RUN_AS_OWNER     = "HasWindowsRunAsOwner";
const char * const ATTR_HELD_JOBS				 = "HeldJobs";
const char * const ATTR_HIBERNATION_LEVEL        = "HibernationLevel";
const char * const ATTR_HIBERNATION_STATE        = "HibernationState";
const char * const ATTR_HIBERNATION_SUPPORTED_STATES = "HibernationSupportedStates";
const char * const ATTR_HIBERNATION_RAW_MASK     = "HibernationRawMask";
const char * const ATTR_HIBERNATION_METHOD       = "HibernationMethod";
const char * const ATTR_UNHIBERNATE              = "Unhibernate";
const char * const ATTR_HOLD_KILL_SIG            = "HoldKillSig";
const char * const ATTR_HOOK_KEYWORD             = "HookKeyword";
const char * const ATTR_IDLE_JOBS                = "IdleJobs";
const char * const ATTR_IMAGE_SIZE				 = "ImageSize";
const char * const ATTR_INTERACTIVE			 = "Interactive";
const char * const ATTR_IS_DAEMON_CORE           = "IsDaemonCore";
const char * const ATTR_IS_OWNER                 = "IsOwner";
const char * const ATTR_IS_QUEUE_SUPER_USER      = "IsQueueSuperUser";
const char * const ATTR_IS_WAKEABLE              = "IsWakeAble";
const char * const ATTR_IS_WAKE_SUPPORTED        = "IsWakeOnLanSupported";
const char * const ATTR_WAKE_SUPPORTED_FLAGS     = "WakeOnLanSupportedFlags";
const char * const ATTR_IS_WAKE_ENABLED          = "IsWakeOnLanEnabled";
const char * const ATTR_WAKE_ENABLED_FLAGS       = "WakeOnLanEnabledFlags";
const char * const ATTR_INACTIVE                 = "Inactive";
const char * const ATTR_JAR_FILES                = "JarFiles";
const char * const ATTR_JAVA_MFLOPS              = "JavaMFlops";
const char * const ATTR_JAVA_VENDOR              = "JavaVendor";
const char * const ATTR_JAVA_VERSION             = "JavaVersion";
const char * const ATTR_JOB_ACTION               = "JobAction";
const char * const ATTR_JOB_ARGUMENTS1			 = "Args";
const char * const ATTR_JOB_ARGUMENTS2			 = "Arguments";
const char * const ATTR_JOB_CMD					 = "Cmd";
const char * const ATTR_JOB_CMD_HASH				 = "CmdHash";
const char * const ATTR_JOB_CMD_MD5				 = "CmdMD5";
const char * const ATTR_ORIG_JOB_CMD				= "OrigCmd"; 
const char * const ATTR_JOB_CORE_DUMPED			 = "JobCoreDumped";
const char * const ATTR_JOB_CORE_FILENAME		 = "JobCoreFileName";
const char * const ATTR_JOB_CURRENT_START_DATE	 = "JobCurrentStartDate";
const char * const ATTR_JOB_DURATION			 = "JobDuration";
const char * const ATTR_JOB_ENVIRONMENT1		 = "Env";
const char * const ATTR_JOB_ENVIRONMENT1_DELIM	 = "EnvDelim";
const char * const ATTR_JOB_ENVIRONMENT2		 = "Environment";
const char * const ATTR_JOB_ERROR				 = "Err";
const char * const ATTR_JOB_ERROR_ORIG			 = "ErrOrig";
const char * const ATTR_JOB_ERROR_SIZE			 = "ErrSize";
const char * const ATTR_JOB_KEYWORD              = "JobKeyword";
const char * const ATTR_JOB_LEASE_DURATION       = "JobLeaseDuration";
const char * const ATTR_JOB_LEASE_EXPIRATION     = "JobLeaseExpiration";
const char * const ATTR_JOB_SPOOL_EXECUTABLE	 = "SpoolExecutable";
const char * const ATTR_JOB_EXIT_STATUS			 = "ExitStatus";
const char * const ATTR_JOB_EXIT_REQUIREMENTS	 = "ExitRequirements";
const char * const ATTR_JOB_ID					 = "JobId";
const char * const ATTR_JOB_FINISHED_HOOK_DONE   = "JobFinishedHookDone";
const char * const ATTR_JOB_INPUT				 = "In";
const char * const ATTR_JOB_IWD					 = "Iwd";
const char * const ATTR_JOB_IWD_FLUSH_NFS_CACHE	 = "IwdFlushNFSCache";
const char * const ATTR_JOB_JAVA_VM_ARGS1        = "JavaVMArgs";
const char * const ATTR_JOB_JAVA_VM_ARGS2        = "JavaVMArguments";
const char * const ATTR_ORIG_JOB_IWD				 = "OrigIwd";
const char * const ATTR_JOB_REMOTE_IWD			 = "RemoteIwd";
const char * const ATTR_JOB_LOAD_PROFILE           = "LoadProfile";
const char * const ATTR_JOB_RUNAS_OWNER			 = "RunAsOwner";
const char * const ATTR_JOB_LOAD_USER_PROFILE   = "LoadUserProfile";
const char * const ATTR_JOB_LOCAL_CPU			 = "LocalCpu";
const char * const ATTR_JOB_LOCAL_SYS_CPU		 = "LocalSysCpu";
const char * const ATTR_JOB_LOCAL_USER_CPU		 = "LocalUserCpu";
const char * const ATTR_JOB_MANAGED				 = "Managed";
const char * const ATTR_JOB_MANAGED_MANAGER		 = "ManagedManager";
const char * const ATTR_JOB_MATCHED				 = "Matched";
const char * const ATTR_JOB_NONESSENTIAL		 = "Nonessential";
const char * const ATTR_JOB_NOOP				 = "IsNoopJob";
const char * const ATTR_JOB_NOOP_EXIT_SIGNAL	 = "NoopJobExitSignal";
const char * const ATTR_JOB_NOOP_EXIT_CODE		 = "NoopJobExitCode";
const char * const ATTR_JOB_NOTIFICATION		 = "JobNotification";
const char * const ATTR_JOB_OUTPUT				 = "Out";
const char * const ATTR_JOB_OUTPUT_ORIG			 = "OutOrig";
const char * const ATTR_JOB_OUTPUT_SIZE			 = "OutSize";
const char * const ATTR_JOB_PID                  = "JobPid";
const char * const ATTR_JOB_PRIO                 = "JobPrio";
const char * const ATTR_JOB_COMMITTED_TIME		 = "CommittedTime";
const char * const ATTR_JOB_LANGUAGE             = "JobLanguage";
const char * const ATTR_JOB_LAST_START_DATE		 = "JobLastStartDate";
const char * const ATTR_JOB_LEAVE_IN_QUEUE		 = "LeaveJobInQueue";
const char * const ATTR_JOB_REMOTE_SYS_CPU		 = "RemoteSysCpu";
const char * const ATTR_JOB_REMOTE_USER_CPU		 = "RemoteUserCpu";
const char * const ATTR_JOB_REMOTE_WALL_CLOCK	 = "RemoteWallClockTime";
const char * const ATTR_JOB_ROOT_DIR			 = "RootDir";
const char * const ATTR_JOB_RUN_COUNT			 = "JobRunCount";
const char * const ATTR_JOB_SANDBOX_JOBAD		= "DropJobAdInSandbox";
const char * const ATTR_JOB_SUBMISSION		= "Submission";
const char * const ATTR_JOB_SUBMISSION_ID		= "SubmissionId";
const char * const ATTR_JOB_START                = "JobStart";
const char * const ATTR_JOB_START_DATE			 = "JobStartDate";
const char * const ATTR_JOB_STATE                = "JobState";
const char * const ATTR_JOB_STATUS               = "JobStatus";
const char * const ATTR_LAST_JOB_STATUS               = "LastJobStatus";
const char * const ATTR_JOB_STATUS_ON_RELEASE    = "JobStatusOnRelease";
const char * const ATTR_JOB_UNIVERSE             = "JobUniverse";
const char * const ATTR_JOB_WALL_CLOCK_CKPT		 = "WallClockCheckpoint";
const char * const ATTR_JOB_QUEUE_BIRTHDATE		 = "JobQueueBirthdate";
// VM universe
const char * const ATTR_JOB_VM_TYPE				= "JobVMType";
const char * const ATTR_JOB_VM_MEMORY				= "JobVMMemory";
const char * const ATTR_JOB_VM_CHECKPOINT			= "JobVMCheckpoint";
const char * const ATTR_JOB_VM_NETWORKING			= "JobVMNetworking";
const char * const ATTR_JOB_VM_NETWORKING_TYPE		= "JobVMNetworkingType";
const char * const ATTR_JOB_VM_HARDWARE_VT			= "JobVMHardwareVT";
const char * const  ATTR_JOB_VM_VCPUS = "JobVM_VCPUS";
const char * const  ATTR_JOB_VM_MACADDR = "JobVM_MACADDR";
// End VM universe
const char * const ATTR_KEYBOARD_IDLE            = "KeyboardIdle";
const char * const ATTR_KEYSTORE_FILE            = "KeystoreFile";
const char * const ATTR_KEYSTORE_ALIAS           = "KeystoreAlias";
const char * const ATTR_KEYSTORE_PASSPHRASE_FILE = "KeystorePassphraseFile";
const char * const ATTR_KFLOPS                   = "KFlops";
const char * const ATTR_KILL                     = "Kill";
const char * const ATTR_KILL_SIG				 = "KillSig";
const char * const ATTR_KILL_SIG_TIMEOUT		 = "KillSigTimeout";
const char * const ATTR_LAST_AVAIL_INTERVAL		 = "LastAvailInterval";
const char * const ATTR_LAST_BENCHMARK			 = "LastBenchmark";
const char * const ATTR_LAST_CKPT_SERVER		 = "LastCkptServer";
const char * const ATTR_LAST_CKPT_TIME			 = "LastCkptTime";
const char * const ATTR_LAST_PUBLIC_CLAIM_ID    = "LastPublicClaimId";
const char * const ATTR_LAST_PUBLIC_CLAIM_IDS   = "LastPublicClaimIds";
const char * const ATTR_LAST_CLAIM_STATE         = "LastClaimState";
const char * const ATTR_LAST_FETCH_WORK_SPAWNED   = "LastFetchWorkSpawned";
const char * const ATTR_LAST_FETCH_WORK_COMPLETED = "LastFetchWorkCompleted";
const char * const ATTR_LAST_VACATE_TIME		 = "LastVacateTime";
const char * const ATTR_LAST_HEARD_FROM          = "LastHeardFrom";
const char * const ATTR_LAST_HOLD_REASON         = "LastHoldReason";
const char * const ATTR_LAST_HOLD_REASON_CODE	 = "LastHoldReasonCode";
const char * const ATTR_LAST_HOLD_REASON_SUBCODE = "LastHoldReasonSubCode";
const char * const ATTR_LAST_JOB_LEASE_RENEWAL   = "LastJobLeaseRenewal";
const char * const ATTR_LAST_JOB_LEASE_RENEWAL_FAILED= "LastJobLeaseRenewalFailed";
const char * const ATTR_LAST_MATCH_TIME			 = "LastMatchTime";
const char * const ATTR_MACHINE_LAST_MATCH_TIME			 = "MachineLastMatchTime";
const char * const ATTR_LAST_MATCH_LIST_PREFIX   = "LastMatchName";
const char * const ATTR_LAST_MATCH_LIST_LENGTH   = "LastMatchListLength";
const char * const ATTR_LAST_REJ_MATCH_TIME		 = "LastRejMatchTime";
const char * const ATTR_LAST_REJ_MATCH_REASON	 = "LastRejMatchReason";
const char * const ATTR_LAST_PERIODIC_CHECKPOINT = "LastPeriodicCheckpoint";
const char * const ATTR_LAST_RELEASE_REASON      = "LastReleaseReason";
const char * const ATTR_LAST_REMOTE_HOST		 = "LastRemoteHost";
const char * const ATTR_LAST_REMOTE_STATUS_UPDATE = "LastRemoteStatusUpdate";
const char * const ATTR_LAST_UPDATE				 = "LastUpdate";
const char * const ATTR_LOCAL_CREDD              = "LocalCredd";
const char * const ATTR_LOCAL_FILES		= "LocalFiles";
const char * const ATTR_LOAD_AVG                 = "LoadAvg";
const char * const ATTR_MACHINE                  = "Machine";
const char * const ATTR_MACHINE_COUNT            = "MachineCount";
const char * const ATTR_MASTER_IP_ADDR			 = "MasterIpAddr";
const char * const ATTR_MAX_HOSTS				 = "MaxHosts";
const char * const ATTR_MAX_JOB_RETIREMENT_TIME  = "MaxJobRetirementTime";
const char * const ATTR_MAX_JOBS_RUNNING         = "MaxJobsRunning";
const char * const ATTR_MEMORY                   = "Memory";
const char * const ATTR_MIN_HOSTS				 = "MinHosts";
const char * const ATTR_MIPS                     = "Mips";
const char * const ATTR_MPI_IS_MASTER            = "MPIIsMaster";
const char * const ATTR_MPI_MASTER_ADDR       	 = "MPIMasterAddr";
const char * const ATTR_PARALLEL_IS_MASTER       = "ParallelIsMaster";   
const char * const ATTR_PARALLEL_MASTER_ADDR   	 = "ParallelMasterAddr"; 
const char * const ATTR_PARALLEL_SHUTDOWN_POLICY = "ParallelShutdownPolicy"; 
const char * const ATTR_MY_CURRENT_TIME		 = "MyCurrentTime";
const char * const ATTR_MY_TYPE					 = "MyType";
const char * const ATTR_NAME                     = "Name";
const char * const ATTR_NEVER_CREATE_JOB_SANDBOX = "NeverCreateJobSandbox";
const char * const ATTR_NICE_USER			 	 = "NiceUser";
const char * const ATTR_NEGOTIATOR_REQUIREMENTS  = "NegotiatorRequirements";
const char * const ATTR_NEXT_CLUSTER_NUM		 = "NextClusterNum";
const char * const ATTR_NEXT_FETCH_WORK_DELAY	 = "NextFetchWorkDelay";
const char * const ATTR_NEXT_JOB_START_DELAY	 = "NextJobStartDelay";
const char * const ATTR_NODE					 = "Node";
const char * const ATTR_NORDUGRID_RSL			 = "NordugridRSL";
const char * const ATTR_NOTIFY_USER				 = "NotifyUser";
const char * const ATTR_NOTIFY_JOB_SCHEDULER     = "NotifyJobScheduler";
const char * const ATTR_NT_DOMAIN				 = "NTDomain";
const char * const ATTR_WINDOWS_MAJOR_VERSION   = "WindowsMajorVersion";
const char * const ATTR_WINDOWS_MINOR_VERSION   = "WindowsMinorVersion";
const char * const ATTR_WINDOWS_BUILD_NUMBER    = "WindowsBuildNumber";
const char * const ATTR_WINDOWS_SERVICE_PACK_MAJOR = "WindowsServicePackMajorVersion";
const char * const ATTR_WINDOWS_SERVICE_PACK_MINOR = "WindowsServicePackMinorVersion";
const char * const ATTR_WINDOWS_PRODUCT_TYPE    = "WindowsProductType";
const char * const ATTR_NUM_COD_CLAIMS			 = "NumCODClaims";
const char * const ATTR_NUM_CKPTS				 = "NumCkpts";
const char * const ATTR_NUM_CKPTS_RAW			 = "NumCkpts_RAW";
const char * const ATTR_NUM_GLOBUS_SUBMITS		 = "NumGlobusSubmits";
const char * const ATTR_NUM_MATCHES				 = "NumJobMatches";
const char * const ATTR_NUM_HOPS_TO_SUBMIT_MACHINE= "NumHopsToSubmitMachine";
const char * const ATTR_NUM_HOPS_TO_LAST_CKPT_SERVER= "NumHopsToLastCkptServer";
const char * const ATTR_NUM_HOPS_TO_CKPT_SERVER	 = "NumHopsToCkptServer";
const char * const ATTR_NUM_JOB_STARTS	 = "NumJobStarts";
const char * const ATTR_NUM_JOB_RECONNECTS	 = "NumJobReconnects";
const char * const ATTR_NUM_PIDS                 = "NumPids";
const char * const ATTR_NUM_RESTARTS			 = "NumRestarts";
const char * const ATTR_NUM_SHADOW_EXCEPTIONS = "NumShadowExceptions";
const char * const ATTR_NUM_SHADOW_STARTS	 = "NumShadowStarts";
const char * const ATTR_NUM_SYSTEM_HOLDS		 = "NumSystemHolds";
const char * const ATTR_NUM_USERS                = "NumUsers";
const char * const ATTR_OFFLINE                  ="Offline";
const char * const ATTR_OPSYS                    = "OpSys";
const char * const ATTR_ORIG_MAX_HOSTS			 = "OrigMaxHosts";
const char * const ATTR_OWNER                    = "Owner"; 
const char * const ATTR_PARALLEL_SCHEDULING_GROUP	 = "ParallelSchedulingGroup";
const char * const ATTR_PARALLEL_SCRIPT_SHADOW   = "ParallelScriptShadow";  
const char * const ATTR_PARALLEL_SCRIPT_STARTER  = "ParallelScriptStarter"; 
const char * const ATTR_PERIODIC_CHECKPOINT      = "PeriodicCheckpoint";
#define ATTR_PLATFORM					AttrGetName( ATTRE_PLATFORM )
const char * const ATTR_PREEMPTING_ACCOUNTING_GROUP = "PreemptingAccountingGroup";
const char * const ATTR_PREEMPTING_RANK			 = "PreemptingRank";
const char * const ATTR_PREEMPTING_OWNER		 = "PreemptingOwner";
const char * const ATTR_PREEMPTING_USER          = "PreemptingUser";
const char * const ATTR_PREFERENCES				 = "Preferences";
const char * const ATTR_PREV_SEND_ESTIMATE		 = "PrevSendEstimate";
const char * const ATTR_PREV_RECV_ESTIMATE		 = "PrevRecvEstimate";
const char * const ATTR_PRIO                     = "Prio";
const char * const ATTR_PROC_ID                  = "ProcId";
const char * const ATTR_SUB_PROC_ID              = "SubProcId";
const char * const ATTR_PRIVATE_NETWORK_NAME     = "PrivateNetworkName";
const char * const ATTR_Q_DATE                   = "QDate";
const char * const ATTR_RANK					 = "Rank";
const char * const ATTR_REAL_UID				 = "RealUid";
const char * const ATTR_RELEASE_REASON			 = "ReleaseReason";
const char * const ATTR_REMOTE_GROUP_RESOURCES_IN_USE = "RemoteGroupResourcesInUse";
const char * const ATTR_REMOTE_GROUP_QUOTA		 = "RemoteGroupQuota";
const char * const ATTR_REMOTE_HOST				 = "RemoteHost";
const char * const ATTR_REMOTE_HOSTS			 = "RemoteHosts";
const char * const ATTR_REMOTE_OWNER			 = "RemoteOwner";
const char * const ATTR_REMOTE_POOL				 = "RemotePool";
const char * const ATTR_REMOTE_SLOT_ID          = "RemoteSlotID";
const char * const ATTR_REMOTE_SPOOL_DIR		 = "RemoteSpoolDir";
const char * const ATTR_REMOTE_USER              = "RemoteUser";
const char * const ATTR_REMOTE_USER_PRIO         = "RemoteUserPrio";
const char * const ATTR_REMOTE_USER_RESOURCES_IN_USE = "RemoteUserResourcesInUse";
// Deprecated (cruft) -- use: ATTR_REMOTE_SLOT_ID 
const char * const ATTR_REMOTE_VIRTUAL_MACHINE_ID = "RemoteVirtualMachineID";
const char * const ATTR_REMOVE_KILL_SIG          = "RemoveKillSig";
const char * const ATTR_REMOVE_REASON            = "RemoveReason";
const char * const ATTR_REQUEUE_REASON           = "RequeueReason";
const char * const ATTR_REQUIREMENTS             = "Requirements";
const char * const ATTR_SLOT_WEIGHT              = "SlotWeight";
const char * const ATTR_RESULT                   = "Result";
const char * const ATTR_RSC_BYTES_SENT			 = "RSCBytesSent";
const char * const ATTR_RSC_BYTES_RECVD			 = "RSCBytesRecvd";
const char * const ATTR_RUNNING_JOBS             = "RunningJobs";
const char * const ATTR_RUNNING_COD_JOB          = "RunningCODJob";
const char * const ATTR_RUN_BENCHMARKS			 = "RunBenchmarks";
const char * const ATTR_SHADOW_IP_ADDR			 = "ShadowIpAddr";
const char * const ATTR_MY_ADDRESS               = "MyAddress";
const char * const ATTR_SCHEDD_INTERVAL		= "ScheddInterval";
const char * const ATTR_SCHEDD_IP_ADDR           = "ScheddIpAddr";
const char * const ATTR_SCHEDD_NAME				 = "ScheddName";
const char * const ATTR_SCHEDD_SWAP_EXHAUSTED		= "ScheddSwapExhausted";
const char * const ATTR_SCHEDULER				 = "Scheduler";
const char * const ATTR_SHADOW_WAIT_FOR_DEBUG    = "ShadowWaitForDebug";
const char * const ATTR_SLOT_ID				 = "SlotID";
const char * const ATTR_SLOT_PARTITIONABLE		= "PartitionableSlot";
const char * const ATTR_SLOT_DYNAMIC          = "DynamicSlot";
const char * const ATTR_SOURCE					 = "Source";
const char * const ATTR_STAGE_IN_START           = "StageInStart";
const char * const ATTR_STAGE_IN_FINISH          = "StageInFinish";
const char * const ATTR_STAGE_OUT_START          = "StageOutStart";
const char * const ATTR_STAGE_OUT_FINISH         = "StageOutFinish";
const char * const ATTR_START                    = "Start";
const char * const ATTR_START_LOCAL_UNIVERSE	 = "StartLocalUniverse";
const char * const ATTR_START_SCHEDULER_UNIVERSE = "StartSchedulerUniverse";
const char * const ATTR_STARTD_IP_ADDR           = "StartdIpAddr";
const char * const ATTR_STARTD_PRINCIPAL         = "StartdPrincipal";
const char * const ATTR_STATE                    = "State";
const char * const ATTR_STARTER_IP_ADDR          = "StarterIpAddr";
const char * const ATTR_STARTER_ABILITY_LIST     = "StarterAbilityList";
const char * const ATTR_STARTER_IGNORED_ATTRS    = "StarterIgnoredAttributes";
const char * const ATTR_STARTER_ULOG_FILE        = "StarterUserLog";
const char * const ATTR_STARTER_ULOG_USE_XML     = "StarterUserLogUseXML";
const char * const ATTR_STARTER_WAIT_FOR_DEBUG   = "StarterWaitForDebug";
const char * const ATTR_STATUS					 = "Status";
const char * const ATTR_STREAM_INPUT			 = "StreamIn";
const char * const ATTR_STREAM_OUTPUT			 = "StreamOut";
const char * const ATTR_STREAM_ERROR			 = "StreamErr";
const char * const ATTR_SUBMITTER_GROUP_RESOURCES_IN_USE = "SubmitterGroupResourcesInUse";
const char * const ATTR_SUBMITTER_GROUP_QUOTA	 = "SubmitterGroupQuota";
const char * const ATTR_SUBMITTER_ID			 = "SubmitterId";
const char * const ATTR_SUBMITTOR_PRIO           = "SubmittorPrio";   // old-style for ATTR_SUBMITTER_USER_PRIO
const char * const ATTR_SUBMITTER_USER_PRIO	  = "SubmitterUserPrio";
const char * const ATTR_SUBMITTER_USER_RESOURCES_IN_USE = "SubmitterUserResourcesInUse";
const char * const ATTR_SUBNET                   = "Subnet";
const char * const ATTR_SUBNET_MASK              = "SubnetMask";
const char * const ATTR_SUSPEND                  = "Suspend";
const char * const ATTR_SUSPEND_JOB_AT_EXEC      = "SuspendJobAtExec";
const char * const ATTR_TARGET_TYPE				 = "TargetType";
const char * const ATTR_TIME_TO_LIVE             = "TimeToLive";
const char * const ATTR_TOTAL_CLAIM_RUN_TIME     = "TotalClaimRunTime";
const char * const ATTR_TOTAL_CLAIM_SUSPEND_TIME = "TotalClaimSuspendTime";
#define ATTR_TOTAL_CONDOR_LOAD_AVG			AttrGetName( ATTRE_TOTAL_LOAD )

const char * const ATTR_TOOL_DAEMON_ARGS1        = "ToolDaemonArgs";
const char * const ATTR_TOOL_DAEMON_ARGS2        = "ToolDaemonArguments";
const char * const ATTR_TOOL_DAEMON_CMD	         = "ToolDaemonCmd";
const char * const ATTR_TOOL_DAEMON_ERROR        = "ToolDaemonError";
const char * const ATTR_TOOL_DAEMON_INPUT        = "ToolDaemonInput";
const char * const ATTR_TOOL_DAEMON_OUTPUT       = "ToolDaemonOutput";

const char * const ATTR_TOTAL_CPUS				 = "TotalCpus";
const char * const ATTR_TOTAL_DISK				 = "TotalDisk";
const char * const ATTR_TOTAL_FLOCKED_JOBS		 = "TotalFlockedJobs";
const char * const ATTR_TOTAL_REMOVED_JOBS		 = "TotalRemovedJobs";
const char * const ATTR_TOTAL_HELD_JOBS			 = "TotalHeldJobs";
const char * const ATTR_TOTAL_IDLE_JOBS			 = "TotalIdleJobs";
const char * const ATTR_TOTAL_JOB_ADS			 = "TotalJobAds";
const char * const ATTR_TOTAL_JOB_RUN_TIME       = "TotalJobRunTime";
const char * const ATTR_TOTAL_JOB_SUSPEND_TIME   = "TotalJobSuspendTime";
const char * const ATTR_TOTAL_LOAD_AVG			 = "TotalLoadAvg";
const char * const ATTR_TOTAL_MEMORY			 = "TotalMemory";
const char * const ATTR_TOTAL_RUNNING_JOBS		 = "TotalRunningJobs";
const char * const ATTR_TOTAL_LOCAL_RUNNING_JOBS = "TotalLocalJobsRunning";
const char * const ATTR_TOTAL_LOCAL_IDLE_JOBS	 = "TotalLocalJobsIdle";
const char * const ATTR_TOTAL_SCHEDULER_RUNNING_JOBS = "TotalSchedulerJobsRunning";
const char * const ATTR_TOTAL_SCHEDULER_IDLE_JOBS	 = "TotalSchedulerJobsIdle";
const char * const ATTR_TOTAL_SLOTS			 = "TotalSlots";
const char * const ATTR_TOTAL_TIME_IN_CYCLE		   = "TotalTimeInCycle";	
const char * const ATTR_TOTAL_TIME_BACKFILL_BUSY      = "TotalTimeBackfillBusy";
const char * const ATTR_TOTAL_TIME_BACKFILL_IDLE      = "TotalTimeBackfillIdle";
const char * const ATTR_TOTAL_TIME_BACKFILL_KILLING   = "TotalTimeBackfillKilling";
const char * const ATTR_TOTAL_TIME_CLAIMED_BUSY       = "TotalTimeClaimedBusy";
const char * const ATTR_TOTAL_TIME_CLAIMED_IDLE       = "TotalTimeClaimedIdle";
const char * const ATTR_TOTAL_TIME_CLAIMED_RETIRING   = "TotalTimeClaimedRetiring";
const char * const ATTR_TOTAL_TIME_CLAIMED_SUSPENDED  = "TotalTimeClaimedSuspended";
const char * const ATTR_TOTAL_TIME_MATCHED_IDLE       = "TotalTimeMatchedIdle";
const char * const ATTR_TOTAL_TIME_OWNER_IDLE         = "TotalTimeOwnerIdle";
const char * const ATTR_TOTAL_TIME_PREEMPTING_KILLING = "TotalTimePreemptingKilling";
const char * const ATTR_TOTAL_TIME_PREEMPTING_VACATING = "TotalTimePreemptingVacating";
const char * const ATTR_TOTAL_TIME_UNCLAIMED_BENCHMARKING = "TotalTimeUnclaimedBenchmarking";
const char * const ATTR_TOTAL_TIME_UNCLAIMED_IDLE     = "TotalTimeUnclaimedIdle";

// Deprecated (cruft) -- use: ATTR_TOTAL_SLOTS;
const char * const ATTR_TOTAL_VIRTUAL_MACHINES	 = "TotalVirtualMachines";
const char * const ATTR_TOTAL_VIRTUAL_MEMORY	 = "TotalVirtualMemory";
const char * const ATTR_UID						 = "Uid";
const char * const ATTR_UID_DOMAIN               = "UidDomain";
const char * const ATTR_ULOG_FILE				 = "UserLog";
const char * const ATTR_ULOG_USE_XML             = "UserLogUseXML";
const char * const ATTR_UPDATE_INTERVAL			 = "UpdateInterval";
const char * const ATTR_CLASSAD_LIFETIME		 = "ClassAdLifetime";
const char * const ATTR_UPDATE_PRIO              = "UpdatePrio";
const char * const ATTR_UPDATE_SEQUENCE_NUMBER   = "UpdateSequenceNumber";
const char * const ATTR_USE_GRID_SHELL           = "UseGridShell";
const char * const ATTR_USER					 = "User";
const char * const ATTR_VACATE                   = "Vacate";
const char * const ATTR_VACATE_TYPE              = "VacateType";
const char * const ATTR_VIRTUAL_MEMORY           = "VirtualMemory";
const char * const ATTR_VISA_TIMESTAMP           = "VisaTimestamp";
const char * const ATTR_VISA_DAEMON_TYPE         = "VisaDaemonType";
const char * const ATTR_VISA_DAEMON_PID          = "VisaDaemonPID";
const char * const ATTR_VISA_HOSTNAME            = "VisaHostname";
const char * const ATTR_VISA_IP                  = "VisaIpAddr";
const char * const ATTR_WOL_PORT                = "WakePort";
const char * const ATTR_WANT_CHECKPOINT		 	 = "WantCheckpoint";
const char * const ATTR_WANT_CLAIMING			 = "WantClaiming";
const char * const ATTR_WANT_IO_PROXY		= "WantIOProxy";
const char * const ATTR_WANT_MATCH_DIAGNOSTICS	 = "WantMatchDiagnostics";
const char * const ATTR_WANT_PARALLEL_SCHEDULING_GROUPS	 = "WantParallelSchedulingGroups";
const char * const ATTR_WANT_REMOTE_SYSCALLS 	 = "WantRemoteSyscalls";
const char * const ATTR_WANT_REMOTE_IO 			 = "WantRemoteIO";
const char * const ATTR_WANT_SCHEDD_COMPLETION_VISA = "WantCompletionVisaFromSchedD";
const char * const ATTR_WANT_STARTER_EXECUTION_VISA = "WantExecutionVisaFromStarter";
const char * const ATTR_WANT_SUBMIT_NET_STATS	 = "WantSubmitNetStats";
const char * const ATTR_WANT_LAST_CKPT_SERVER_NET_STATS= "WantLastCkptServerNetStats";
const char * const ATTR_WANT_CKPT_SERVER_NET_STATS= "WantCkptServerNetStats";
const char * const ATTR_WANT_AD_REVAULATE        = "WantAdRevaluate";
const char * const ATTR_COLLECTOR_IP_ADDR        = "CollectorIpAddr";
const char * const ATTR_NEGOTIATOR_IP_ADDR		 	= "NegotiatorIpAddr";
const char * const ATTR_CREDD_IP_ADDR            = "CredDIpAddr";
const char * const ATTR_NUM_HOSTS_TOTAL			 = "HostsTotal";
const char * const ATTR_NUM_HOSTS_CLAIMED		 = "HostsClaimed";
const char * const ATTR_NUM_HOSTS_UNCLAIMED		 = "HostsUnclaimed";
const char * const ATTR_NUM_HOSTS_OWNER			 = "HostsOwner";
const char * const ATTR_MAX_RUNNING_JOBS			 = "MaxRunningJobs";
#define ATTR_VERSION					AttrGetName( ATTRE_VERSION )
const char * const ATTR_SCHEDD_BIRTHDATE		 = "ScheddBday";
const char * const ATTR_SHADOW_VERSION			 = "ShadowVersion";
// Deprecated (cruft) -- use: ATTR_SLOT_ID
const char * const ATTR_VIRTUAL_MACHINE_ID		 = "VirtualMachineID";
const char * const ATTR_SHOULD_TRANSFER_FILES    = "ShouldTransferFiles";
const char * const ATTR_WHEN_TO_TRANSFER_OUTPUT  = "WhenToTransferOutput";
const char * const ATTR_TRANSFER_TYPE			 = "TransferType";
const char * const ATTR_TRANSFER_FILES			 = "TransferFiles";
const char * const ATTR_TRANSFER_KEY			 = "TransferKey";
const char * const ATTR_TRANSFER_EXECUTABLE		 = "TransferExecutable";
const char * const ATTR_TRANSFER_INPUT			 = "TransferIn";
const char * const ATTR_TRANSFER_OUTPUT			 = "TransferOut";
const char * const ATTR_TRANSFER_ERROR			 = "TransferErr";
const char * const ATTR_TRANSFER_INPUT_FILES	 = "TransferInput";
const char * const ATTR_TRANSFER_INTERMEDIATE_FILES = "TransferIntermediate";
const char * const ATTR_TRANSFER_OUTPUT_FILES	 = "TransferOutput";
const char * const ATTR_TRANSFER_OUTPUT_REMAPS   = "TransferOutputRemaps";
const char * const ATTR_ENCRYPT_INPUT_FILES		 = "EncryptInputFiles";
const char * const ATTR_ENCRYPT_OUTPUT_FILES	 = "EncryptOutputFiles";
const char * const ATTR_DONT_ENCRYPT_INPUT_FILES = "DontEncryptInputFiles";
const char * const ATTR_DONT_ENCRYPT_OUTPUT_FILES= "DontEncryptOutputFiles";
const char * const ATTR_TRANSFER_SOCKET			 = "TransferSocket";
const char * const ATTR_SERVER_TIME				 = "ServerTime";
const char * const ATTR_SHADOW_BIRTHDATE		 = "ShadowBday";
const char * const ATTR_HOLD_REASON				 = "HoldReason";
const char * const ATTR_HOLD_REASON_CODE		 = "HoldReasonCode";
const char * const ATTR_HOLD_REASON_SUBCODE		 = "HoldReasonSubCode";
const char * const ATTR_HOLD_COPIED_FROM_TARGET_JOB = "HoldCopiedFromTargetJob";
const char * const ATTR_WANT_MATCHING			 = "WantMatching";
const char * const ATTR_WANT_RESOURCE_AD		 = "WantResAd";
const char * const ATTR_TOTAL_SUSPENSIONS        = "TotalSuspensions";
const char * const ATTR_LAST_SUSPENSION_TIME     = "LastSuspensionTime";
const char * const ATTR_CUMULATIVE_SUSPENSION_TIME= "CumulativeSuspensionTime";

const char * const ATTR_ON_EXIT_BY_SIGNAL        = "ExitBySignal";
const char * const ATTR_ON_EXIT_CODE		     = "ExitCode";
const char * const ATTR_ON_EXIT_HOLD_CHECK		 = "OnExitHold";
const char * const ATTR_ON_EXIT_REMOVE_CHECK	 = "OnExitRemove";
const char * const ATTR_ON_EXIT_SIGNAL		     = "ExitSignal";
const char * const ATTR_POST_ON_EXIT_BY_SIGNAL	 = "PostExitBySignal";
const char * const ATTR_POST_ON_EXIT_SIGNAL		 = "PostExitSignal";
const char * const ATTR_POST_ON_EXIT_CODE		 = "PostExitCode";
const char * const ATTR_POST_EXIT_REASON		 = "PostExitReason";
const char * const ATTR_PERIODIC_HOLD_CHECK		 = "PeriodicHold";
const char * const ATTR_PERIODIC_RELEASE_CHECK	 = "PeriodicRelease";
const char * const ATTR_PERIODIC_REMOVE_CHECK	 = "PeriodicRemove";
const char * const ATTR_TIMER_REMOVE_CHECK		 = "TimerRemove";
const char * const ATTR_TIMER_REMOVE_CHECK_SENT	 = "TimerRemoveSent";
const char * const ATTR_GLOBUS_RESUBMIT_CHECK	 = "GlobusResubmit";
const char * const ATTR_REMATCH_CHECK			 = "Rematch";

const char * const ATTR_SEC_AUTHENTICATION_METHODS_LIST= "AuthMethodsList";
const char * const ATTR_SEC_AUTHENTICATION_METHODS= "AuthMethods";
const char * const ATTR_SEC_CRYPTO_METHODS       = "CryptoMethods";
const char * const ATTR_SEC_AUTHENTICATION       = "Authentication";
const char * const ATTR_SEC_AUTH_REQUIRED        = "AuthRequired";
const char * const ATTR_SEC_ENCRYPTION           = "Encryption";
const char * const ATTR_SEC_INTEGRITY            = "Integrity";
const char * const ATTR_SEC_ENACT                = "Enact";
const char * const ATTR_SEC_RESPOND              = "Respond";
const char * const ATTR_SEC_COMMAND              = "Command";
const char * const ATTR_SEC_AUTH_COMMAND         = "AuthCommand";
const char * const ATTR_SEC_SID                  = "Sid";
const char * const ATTR_SEC_SUBSYSTEM            = "Subsystem";
const char * const ATTR_SEC_REMOTE_VERSION       = "RemoteVersion";
const char * const ATTR_SEC_SERVER_ENDPOINT      = "ServerEndpoint";
const char * const ATTR_SEC_SERVER_COMMAND_SOCK  = "ServerCommandSock";
const char * const ATTR_SEC_SERVER_PID           = "ServerPid";
const char * const ATTR_SEC_PARENT_UNIQUE_ID     = "ParentUniqueID";
const char * const ATTR_SEC_PACKET_COUNT         = "PacketCount";
const char * const ATTR_SEC_NEGOTIATION          = "OutgoingNegotiation";
const char * const ATTR_SEC_VALID_COMMANDS       = "ValidCommands";
const char * const ATTR_SEC_SESSION_DURATION     = "SessionDuration";
const char * const ATTR_SEC_SESSION_EXPIRES      = "SessionExpires";
const char * const ATTR_SEC_SESSION_LEASE        = "SessionLease";
const char * const ATTR_SEC_USER                 = "User";
const char * const ATTR_SEC_MY_REMOTE_USER_NAME  = "MyRemoteUserName";
const char * const ATTR_SEC_NEW_SESSION          = "NewSession";
const char * const ATTR_SEC_USE_SESSION          = "UseSession";
const char * const ATTR_SEC_COOKIE               = "Cookie";
const char * const  ATTR_SEC_TRIED_AUTHENTICATION = "TriedAuthentication";

const char * const ATTR_MULTIPLE_TASKS_PER_PVMD  = "MultipleTasksPerPvmd";

const char * const ATTR_UPDATESTATS_TOTAL		 = "UpdatesTotal";
const char * const ATTR_UPDATESTATS_SEQUENCED	 = "UpdatesSequenced";
const char * const ATTR_UPDATESTATS_LOST			 = "UpdatesLost";
const char * const ATTR_UPDATESTATS_HISTORY		 = "UpdatesHistory";

const char * const ATTR_QUILL_ENABLED		= "QuillEnabled";
const char * const ATTR_QUILL_NAME		= "QuillName";
const char * const ATTR_QUILL_IS_REMOTELY_QUERYABLE	= "QuillIsRemotelyQueryable";
const char * const ATTR_QUILL_DB_IP_ADDR	= "QuillDatabaseIpAddr";
const char * const ATTR_QUILL_DB_NAME		= "QuillDatabaseName";
const char * const ATTR_QUILL_DB_QUERY_PASSWORD	= "QuillDatabaseQueryPassword";

const char * const ATTR_QUILL_SQL_TOTAL		= "NumSqlTotal";
const char * const ATTR_QUILL_SQL_LAST_BATCH	= "NumSqlLastBatch";

const char * const ATTR_CHECKPOINT_PLATFORM			= "CheckpointPlatform";
const char * const ATTR_LAST_CHECKPOINT_PLATFORM	= "LastCheckpointPlatform";
const char * const ATTR_IS_VALID_CHECKPOINT_PLATFORM  = "IsValidCheckpointPlatform";

const char * const ATTR_WITHIN_RESOURCE_LIMITS  = "WithinResourceLimits";

const char * const ATTR_HAD_IS_ACTIVE			= "HadIsActive";
const char * const ATTR_HAD_LIST   			= "HadList";
const char * const ATTR_HAD_INDEX   			= "HadIndex";
const char * const ATTR_HAD_SELF_ID   			= "HadSelfID";
const char * const ATTR_HAD_CONTROLLEE_NAME    = "HadControlleeName";
const char * const ATTR_TERMINATION_PENDING	= "TerminationPending";
const char * const ATTR_TERMINATION_EXITREASON	= "TerminationExitReason";

const char * const ATTR_REPLICATION_LIST      = "ReplicationList";

// Attributes used in clasads that go back and forth between a submitting
// client, the schedd, and the transferd while negotiating for a place to
// put/get a sandbox of files and then for when actually putting/getting the
// files themselves..
const char * const ATTR_TREQ_DIRECTION = "TransferDirection";
const char * const ATTR_TREQ_INVALID_REQUEST = "InvalidRequest";
const char * const ATTR_TREQ_INVALID_REASON = "InvalidReason";
const char * const ATTR_TREQ_HAS_CONSTRAINT = "HasConstraint";
const char * const ATTR_TREQ_JOBID_LIST = "JobIDList";
const char * const ATTR_TREQ_PEER_VERSION = "PeerVersion";
const char * const ATTR_TREQ_FTP = "FileTransferProtocol"; 
const char * const ATTR_TREQ_TD_SINFUL = "TDSinful"; 
const char * const ATTR_TREQ_TD_ID = "TDID"; 
const char * const ATTR_TREQ_CONSTRAINT = "Constraint";
const char * const ATTR_TREQ_JOBID_ALLOW_LIST = "JobIDAllowList";
const char * const ATTR_TREQ_JOBID_DENY_LIST = "JobIDDenyList";
const char * const ATTR_TREQ_WILL_BLOCK = "WillBlock";
const char * const ATTR_TREQ_CAPABILITY = "Capability";
const char * const ATTR_TREQ_NUM_TRANSFERS = "NumberOfTransfers";
const char * const ATTR_TREQ_UPDATE_STATUS = "UpdateStatus";
const char * const ATTR_TREQ_UPDATE_REASON = "UpdateReason";
const char * const ATTR_TREQ_SIGNALED = "Signaled";
const char * const ATTR_TREQ_SIGNAL = "Signal";
const char * const ATTR_TREQ_EXIT_CODE = "ExitCode";
const char * const ATTR_NEGOTIATOR_MATCH_EXPR = "NegotiatorMatchExpr";

// This is a record of the job exit status from a standard universe job exit
// via waitpid. It is in the job ad to implement the terminate_pending
// feature. It has to be here because of rampant global variable usage in the
// standard universe shadow. It saved a tremendous amount of code to just
// put this value in the job ad.
const char * const ATTR_WAITPID_STATUS	= "WaitpidStatus";
const char * const ATTR_TERMINATION_REASON	= "TerminationReason";

// VM universe
const char * const ATTR_VM_TYPE    = "VM_Type";
const char * const ATTR_VM_MEMORY = "VM_Memory";
const char * const ATTR_VM_NETWORKING = "VM_Networking";
const char * const ATTR_VM_NETWORKING_TYPES = "VM_Networking_Types";
const char * const ATTR_VM_HARDWARE_VT = "VM_HardwareVT";
const char * const ATTR_VM_AVAIL_NUM = "VM_AvailNum";
const char * const ATTR_VM_ALL_GUEST_MACS = "VM_All_Guest_Macs";
const char * const ATTR_VM_ALL_GUEST_IPS = "VM_All_Guest_IPs";
const char * const ATTR_VM_GUEST_MAC = "VM_Guest_Mac";
const char * const ATTR_VM_GUEST_IP = "VM_Guest_IP";
const char * const ATTR_VM_GUEST_MEM = "VM_Guest_Mem";
const char * const ATTR_VM_CKPT_MAC = "VM_CkptMac";
const char * const ATTR_VM_CKPT_IP = "VM_CkptIP";

// End VM universe

const char * const ATTR_REQUEST_CPUS = "RequestCpus";
const char * const ATTR_REQUEST_MEMORY = "RequestMemory";
const char * const ATTR_REQUEST_DISK = "RequestDisk";

// Valid settings for ATTR_JOB_MANAGED.
	// Managed by an external process (gridmanager)
const char * const MANAGED_EXTERNAL				 = "External";
	// Schedd should manage as normal
const char * const MANAGED_SCHEDD				 = "Schedd";
	// Schedd should manage as normal.  External process doesn't want back.
const char * const MANAGED_DONE				 = "ScheddDone";
const char * const  COLLECTOR_REQUIREMENTS = "COLLECTOR_REQUIREMENTS";
const char * const ATTR_PREV_LAST_HEARD_FROM	= "PrevLastHeardFrom";

const char * const ATTR_TRY_AGAIN = "TryAgain";
const char * const ATTR_DOWNLOADING = "Downloading";
const char * const ATTR_TIMEOUT = "Timeout";
const char * const ATTR_CCBID = "CCBID";
const char * const ATTR_REQUEST_ID = "RequestID";
const char * const ATTR_SESSION_INFO = "SessionInfo";
const char * const ATTR_SSH_PUBLIC_SERVER_KEY = "SSHPublicServerKey";
const char * const ATTR_SSH_PRIVATE_CLIENT_KEY = "SSHPrivateClientKey";
const char * const ATTR_SHELL = "Shell";
const char * const ATTR_RETRY = "Retry";
const char * const ATTR_SSH_KEYGEN_ARGS = "SSHKeyGenArgs";
const char * const ATTR_SOCK = "sock";


//************* Added for Amazon Jobs ***************************//
const char * const ATTR_AMAZON_PUBLIC_KEY = "AmazonPublicKey";
const char * const ATTR_AMAZON_PRIVATE_KEY = "AmazonPrivateKey";
const char * const ATTR_AMAZON_AMI_ID = "AmazonAmiID";
const char * const ATTR_AMAZON_SECURITY_GROUPS = "AmazonSecurityGroups";
const char * const ATTR_AMAZON_KEY_PAIR_FILE = "AmazonKeyPairFile";
const char * const ATTR_AMAZON_USER_DATA = "AmazonUserData";
const char * const ATTR_AMAZON_USER_DATA_FILE = "AmazonUserDataFile";
const char * const ATTR_AMAZON_REMOTE_VM_NAME = "AmazonRemoteVirtualMachineName";
const char * const ATTR_AMAZON_INSTANCE_TYPE = "AmazonInstanceType";
//************* End of changes for Amamzon Jobs *****************//


//************* Added for Lease Manager *******************//
const char * const ATTR_LEASE_MANAGER_IP_ADDR = "LeaseManagerIpAddr";
//************* End of Lease Manager    *******************//
