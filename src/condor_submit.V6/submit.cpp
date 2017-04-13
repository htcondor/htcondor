/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_string.h"
#include "spooled_job_files.h"
#include "subsystem_info.h"
#include "env.h"
#include "basename.h"
#include "condor_getcwd.h"
#include <time.h>
#include "write_user_log.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_distribution.h"
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif
#include "store_cred.h"
#include "internet.h"
#include "my_hostname.h"
#include "domain_tools.h"
#include "get_daemon_name.h"
//#include "condor_qmgr.h"  // only submit_protocol.cpp is allowed to access the schedd's Qmgt functions
#include "sig_install.h"
#include "access.h"
#include "daemon.h"
#include "match_prefix.h"

#include "extArray.h"
#include "HashTable.h"
#include "MyString.h"
#include "string_list.h"
#include "which.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "my_username.h"
#include "globus_utils.h"
#include "enum_utils.h"
#include "setenv.h"
#include "classad_hashtable.h"
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "dc_transferd.h"
#include "condor_ftp.h"
#include "condor_crontab.h"
#include <scheduler.h>
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"
#include "NegotiationUtils.h"
#include <submit_utils.h>
//uncomment this to have condor_submit use the new for 8.5 submit_utils classes
#define USE_SUBMIT_UTILS 1
#include "submit_internal.h"

#include "list.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "condor_md.h"
#include "my_popen.h"
#include "zkm_base64.h"

#include <algorithm>
#include <string>
#include <set>

#ifdef USE_SUBMIT_UTILS
// the halting parser doesn't work when Queue statements are in an submit file include : statement
//#define USE_HALTING_PARSER 1
#else
#endif


// TODO: hashFunction() is case-insenstive, but when a MyString is the
//   hash key, the comparison in HashTable is case-sensitive. Therefore,
//   the case-insensitivity of hashFunction() doesn't complish anything.
//   CheckFilesRead and CheckFilesWrite should be
//   either completely case-sensitive (and use MyStringHash()) or
//   completely case-insensitive (and use AttrKey and AttrKeyHashFunction).
static unsigned int hashFunction( const MyString& );
#include "file_sql.h"

HashTable<MyString,int> CheckFilesRead( 577, hashFunction ); 
HashTable<MyString,int> CheckFilesWrite( 577, hashFunction ); 

#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
#else
StringList NoClusterCheckAttrs;
#endif
#ifdef USE_SUBMIT_UTILS
ClassAd *gClusterAd = NULL;
#else
ClassAd *ClusterAd = NULL;
#endif

time_t submit_time = 0;

// Explicit template instantiation


#ifdef USE_SUBMIT_UTILS
#else
ClassAd *job = NULL;

//char	*OperatingSystem;
//char	*Architecture;
//char	*Spool;
#endif
char	*ScheddName = NULL;
std::string ScheddAddr;
AbstractScheddQ * MyQ = NULL;
char	*PoolName = NULL;
DCSchedd* MySchedd = NULL;

char	*My_fs_domain;
#ifdef USE_SUBMIT_UTILS
#else
MyString	JobRequirements;
#if !defined(WIN32)
MyString    JobRootdir;
#endif
MyString	JobIwd;
#endif

int		ExtraLineNo;
int		GotQueueCommand = 0;
int		GotNonEmptyQueueCommand = 0;
SandboxTransferMethod	STMethod = STM_USE_SCHEDD_ONLY;

#ifdef USE_SUBMIT_UTILS
#else
char *	IckptName;	/* Pathname of spooled initial ckpt file */
int64_t TransferInputSizeKb;	/* total size of files transfered to exec machine */
#endif

const char	*MyName;
#ifdef USE_SUBMIT_UTILS
int 	dash_interactive = 0; /* true if job submitted with -interactive flag */
#else
int 	InteractiveJob = 0; /* true if job submitted with -interactive flag */
#define dash_interactive InteractiveJob
#endif
int 	InteractiveSubmitFile = 0; /* true if using INTERACTIVE_SUBMIT_FILE */
bool	verbose = false; // formerly: int Quiet = 1;
bool	terse = false; // generate parsable output
SetAttributeFlags_t setattrflags = 0; // flags to SetAttribute()
bool	SubmitFromStdin = false;
int		WarnOnUnusedMacros = 1;
int		DisableFileChecks = 0;
int		DashDryRun = 0;
int		DashMaxJobs = 0;	 // maximum number of jobs to create before generating an error
int		DashMaxClusters = 0; // maximum number of clusters to create before generating an error.
const char * DashDryRunOutName = NULL;
int		DumpSubmitHash = 0;
#ifdef USE_SUBMIT_UTILS
#else
int		JobDisableFileChecks = 0;
bool	RequestMemoryIsZero = false;
bool	RequestDiskIsZero = false;
bool	RequestCpusIsZeroOrOne = false;
bool	already_warned_requirements_mem = false;
bool	already_warned_requirements_disk = false;
#endif
int		MaxProcsPerCluster;
#ifdef USE_SUBMIT_UTILS
int	  ClusterId = -1;
int	  ProcId = -1;
#else
int	  ClusterId = -1;
int	  ProcId = -1;
int	  JobUniverse = CONDOR_UNIVERSE_MIN;
char *JobGridType = NULL;
int		Remote = 0;
#define dash_remote Remote
#endif
int		ClustersCreated = 0;
int		JobsCreated = 0;
int		ActiveQueueConnection = FALSE;
bool    nice_user_setting = false;
bool	NewExecutable = false;
#ifdef USE_SUBMIT_UTILS
int		dash_remote=0;
int		dash_factory=0;
int		default_to_factory=0;
unsigned int submit_unique_id=1; // hack to make default_to_factory work in test suite for milestone 1 (8.7.1)
#else
bool	IsFirstExecutable;
bool	UserLogSpecified = false;
bool	UseXMLInLog = false;
ShouldTransferFiles_t should_transfer;
bool stream_stdout_toggle = false;
bool stream_stderr_toggle = false;
bool    NeedsPerFileEncryption = false;
bool	NeedsJobDeferral = false;
bool job_ad_saved = false;	// should we deallocate the job ad after storing it?
bool HasEncryptExecuteDir = false;
bool HasTDP = false;
char* tdp_cmd = NULL;
char* tdp_input = NULL;
#endif
#if defined(WIN32)
char* RunAsOwnerCredD = NULL;
#endif
char * batch_name_line = NULL;
bool sent_credential_to_credd = false;

// For mpi universe testing
bool use_condor_mpi_universe = false;

#ifdef USE_SUBMIT_UTILS
MyString LastExecutable; // used to pass executable between the check_file callback and SendExecutableb
bool     SpoolLastExecutable;
#else
// For docker "universe"
bool IsDockerJob = false;

// For vm universe
MyString VMType;
int VMMemoryMb = 0;
int VMVCPUS = 0;
MyString VMMACAddr;
bool VMCheckpoint = false;
bool VMNetworking = false;
bool VMVNC=false;
MyString VMNetworkType;
bool VMHardwareVT = false;
bool vm_need_fsdomain = false;
#endif

time_t get_submit_time()
{
	if ( ! submit_time) {
		submit_time = time(0);
	}
	return submit_time;
}

#define time(a) poison_time(a)

//
// The default polling interval for the schedd
//
extern const int SCHEDD_INTERVAL_DEFAULT;
//
// The default job deferral prep time
//
extern const int JOB_DEFERRAL_PREP_DEFAULT;

char* LogNotesVal = NULL;
char* UserNotesVal = NULL;
char* StackSizeVal = NULL;
List<const char> extraLines;  // lines passed in via -a argument
std::string queueCommandLine; // queue statement passed in via -q argument

#ifdef USE_SUBMIT_UTILS
SubmitHash submit_hash;
#else

#if !defined(_XFORM_UTILS_H)
// declare enough of the condor_params structure definitions so that we can define submit hashtable defaults
namespace condor_params {
	typedef struct string_value { char * psz; int flags; } string_value;
	struct key_value_pair { const char * key; const string_value * def; };
}
#endif

// buffers used while processing the queue statement to inject $(Cluster), $(Process) and $(Step)
// into the submit hash table.  Note that these buffers are also used BEFORE the queue
// statement to set defaults for $(Cluster), $(Process) and $(Step).
static char ClusterString[20]="1", ProcessString[20]="0", StepString[20]="0", RowString[20]="0", EmptyItemString[] = "";
// values for submit hashtable defaults, these are declared as char rather than as const char to make g++ on fedora shut up.
static char OneString[] = "1", ZeroString[] = "0";
static char ParallelNodeString[] = "#pArAlLeLnOdE#";
static char UnsetString[] = "";


static condor_params::string_value ArchMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysVerMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysMajorVerMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysAndVerMacroDef = { UnsetString, 0 };
static condor_params::string_value SpoolMacroDef = { UnsetString, 0 };
static condor_params::string_value ClusterMacroDef = { ClusterString, 0 };
static condor_params::string_value ProcessMacroDef = { ProcessString, 0 };
static condor_params::string_value NodeMacroDef = { UnsetString, 0 };
static condor_params::string_value StepMacroDef = { StepString, 0 };
static condor_params::string_value RowMacroDef = { RowString, 0 };
#ifdef WIN32
static condor_params::string_value IsLinuxMacroDef = { ZeroString, 0 };
static condor_params::string_value IsWinMacroDef = { OneString, 0 };
#elif defined LINUX
static condor_params::string_value IsLinuxMacroDef = { OneString, 0 };
static condor_params::string_value IsWinMacroDef = { ZeroString, 0 };
#else
static condor_params::string_value IsLinuxMacroDef = { ZeroString, 0 };
static condor_params::string_value IsWinMacroDef = { ZeroString, 0 };
#endif
static condor_params::string_value SubmitFileMacroDef = { EmptyItemString, 0 };

static char rc[] = "$(Request_CPUs)";
static condor_params::string_value VMVCPUSMacroDef = { rc, 0 };
static char rm[] = "$(Request_Memory)";
static condor_params::string_value VMMemoryMacroDef = { rm, 0 };

// Necessary so that the user who sets request_memory instead of
// RequestMemory doesn't miss out on the default value for VM_MEMORY.
static char rem[] = "$(RequestMemory)";
static condor_params::string_value RequestMemoryMacroDef = { rem, 0 };
// The same for CPUs.
static char rec[] = "$(RequestCPUs)";
static condor_params::string_value RequestCPUsMacroDef = { rec, 0 };

static char StrictFalseMetaKnob[] = 
	"SubmitWarnEmptyMatches=false\n"
	"SubmitFailEmptyMatches=false\n"
	"SubmitWarnDuplicateMatches=false\n"
	"SubmitFailEmptyFields=false\n"
	"SubmitWarnEmptyFields=false\n";
static char StrictTrueMetaKnob[] = 
	"SubmitWarnEmptyMatches=true\n"
	"SubmitFailEmptyMatches=true\n"
	"SubmitWarnDuplicateMatches=true\n"
	"SubmitFailEmptyFields=false\n"
	"SubmitWarnEmptyFields=true\n";
static char StrictHarshMetaKnob[] = 
	"SubmitWarnEmptyMatches=true\n"
	"SubmitFailEmptyMatches=true\n"
	"SubmitWarnDuplicateMatches=true\n"
	"SubmitFailEmptyFields=true\n"
	"SubmitWarnEmptyFields=true\n";
static condor_params::string_value StrictFalseMetaDef = { StrictFalseMetaKnob, 0 };
static condor_params::string_value StrictTrueMetaDef = { StrictTrueMetaKnob, 0 };
static condor_params::string_value StrictHarshMetaDef = { StrictHarshMetaKnob, 0 };

// Table of submit macro 'defaults'. This provides a convenient way to inject things into the submit
// hashtable without actually running a bunch of code to stuff the table.
// Because they are defaults they will be ignored when scanning for unreferenced macros.
// NOTE: TABLE MUST BE SORTED BY THE FIRST FIELD!!
static MACRO_DEF_ITEM SubmitMacroDefaults[] = {
	{ "$STRICT.FALSE", &StrictFalseMetaDef },
	{ "$STRICT.HARSH", &StrictHarshMetaDef },
	{ "$STRICT.TRUE", &StrictTrueMetaDef },
	{ "ARCH",      &ArchMacroDef },
	{ "Cluster",   &ClusterMacroDef },
	{ "ClusterId", &ClusterMacroDef },
	{ "IsLinux",   &IsLinuxMacroDef },
	{ "IsWindows", &IsWinMacroDef },
	{ "ItemIndex", &RowMacroDef },
	{ "Node",      &NodeMacroDef },
	{ "OPSYS",           &OpsysMacroDef },
	{ "OPSYSANDVER",     &OpsysAndVerMacroDef },
	{ "OPSYSMAJORVER",   &OpsysMajorVerMacroDef },
	{ "OPSYSVER",        &OpsysVerMacroDef },
	{ "Process",   &ProcessMacroDef },
	{ "ProcId",    &ProcessMacroDef },
	{ "Request_CPUs", &RequestCPUsMacroDef },
	{ "Request_Memory", &RequestMemoryMacroDef },
	{ "Row",       &RowMacroDef },
	{ "SPOOL",     &SpoolMacroDef },
	{ "Step",      &StepMacroDef },
	{ "SUBMIT_FILE", &SubmitFileMacroDef },
	{ "VM_MEMORY", &VMMemoryMacroDef },
	{ "VM_VCPUS",  &VMVCPUSMacroDef },
};

static MACRO_DEFAULTS SubmitMacroDefaultSet = {
	COUNTOF(SubmitMacroDefaults), SubmitMacroDefaults, NULL
};

// the submit file is read into this macro table
//
static MACRO_SET SubmitMacroSet = {
	0, 0,
	CONFIG_OPT_WANT_META | CONFIG_OPT_KEEP_DEFAULTS | CONFIG_OPT_SUBMIT_SYNTAX,
	0, NULL, NULL, ALLOCATION_POOL(), std::vector<const char*>(),
	&SubmitMacroDefaultSet, NULL };

#endif

// these are used to keep track of the source of various macros in the table.
const MACRO_SOURCE DefaultMacro = { true, false, 1, -2, -1, -2 }; // for macros set by default
const MACRO_SOURCE ArgumentMacro = { true, false, 2, -2, -1, -2 }; // for macros set by command line
const MACRO_SOURCE LiveMacro = { true, false, 3, -2, -1, -2 };    // for macros use as queue loop variables
MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };

#define MEG	(1<<20)


//
// Dump ClassAd to file junk
//
const char * DumpFileName = NULL;
bool		DumpClassAdToFile = false;
FILE		*DumpFile = NULL;
bool		DumpFileIsStdout = 0;

#ifdef USE_SUBMIT_UTILS
#else
/*
**	Names of possible CONDOR variables.
*/
const char	*Cluster 		= "Cluster";
const char	*Process			= "Process";
const char	*BatchName		= "batch_name";
const char	*Hold			= "hold";
const char	*Priority		= "priority";
const char	*Notification	= "notification";
const char	*WantRemoteIO	= "want_remote_io";
const char	*Executable		= "executable";
const char *Description = "description";
const char	*Arguments1		= "arguments";
const char	*Arguments2		= "arguments2";
const char    *AllowArgumentsV1 = "allow_arguments_v1";
const char	*GetEnvironment	= "getenv";
const char	*AllowStartupScript = "allow_startup_script";
const char	*AllowEnvironmentV1 = "allow_environment_v1";
const char	*Environment1	= "environment";
const char	*Environment2	= "environment2";
const char	*Input			= "input";
const char	*Output			= "output";
const char	*Error			= "error";
#if !defined(WIN32)
const char	*RootDir		= "rootdir";
#endif
const char	*InitialDir		= "initialdir";
const char	*RemoteInitialDir		= "remote_initialdir";
const char	*Requirements	= "requirements";
const char	*Preferences	= "preferences";
const char	*Rank				= "rank";
const char	*ImageSize		= "image_size";
const char	*DiskUsage		= "disk_usage";
const char	*MemoryUsage	= "memory_usage";

const char	*RequestCpus	= "request_cpus";
const char	*RequestMemory	= "request_memory";
const char	*RequestDisk	= "request_disk";
const char	*RequestPrefix  = "request_";
typedef std::set<std::string,  classad::CaseIgnLTStr> ResSet;
ResSet fixedReqRes;	 // a case-insenstive set
ResSet stringReqRes;

const char	*Universe		= "universe";
const char	*MachineCount	= "machine_count";
const char	*NotifyUser		= "notify_user";
const char	*EmailAttributes = "email_attributes";
const char	*ExitRequirements = "exit_requirements";
const char	*UserLogFile	= "log";
const char	*UseLogUseXML	= "log_xml";
const char	*DagmanLogFile	= "dagman_log";
const char	*CoreSize		= "coresize";
const char	*NiceUser		= "nice_user";

const char	*GridResource	= "grid_resource";
const char	*X509UserProxy	= "x509userproxy";
const char	*UseX509UserProxy = "use_x509userproxy";
const char  *DelegateJobGSICredentialsLifetime = "delegate_job_gsi_credentials_lifetime";
const char    *GridShell = "gridshell";
const char	*GlobusRSL = "globus_rsl";
const char	*NordugridRSL = "nordugrid_rsl";
const char	*RendezvousDir	= "rendezvousdir";
const char	*KeystoreFile = "keystore_file";
const char	*KeystoreAlias = "keystore_alias";
const char	*KeystorePassphraseFile = "keystore_passphrase_file";
const char  *CreamAttributes = "cream_attributes";
const char  *BatchQueue = "batch_queue";

const char	*SendCredential	= "send_credential";

const char	*FileRemaps = "file_remaps";
const char	*BufferFiles = "buffer_files";
const char	*BufferSize = "buffer_size";
const char	*BufferBlockSize = "buffer_block_size";

const char	*StackSize = "stack_size";
const char	*FetchFiles = "fetch_files";
const char	*CompressFiles = "compress_files";
const char	*AppendFiles = "append_files";
const char	*LocalFiles = "local_files";

const char 	*EncryptExecuteDir = "encrypt_execute_directory";

const char	*ToolDaemonCmd = "tool_daemon_cmd";
const char	*ToolDaemonArgs = "tool_daemon_args"; // for backward compatibility
const char	*ToolDaemonArguments1 = "tool_daemon_arguments";
const char	*ToolDaemonArguments2 = "tool_daemon_arguments2";
const char	*ToolDaemonInput = "tool_daemon_input";
const char	*ToolDaemonError = "tool_daemon_error";
const char	*ToolDaemonOutput = "tool_daemon_output";
const char	*SuspendJobAtExec = "suspend_job_at_exec";

const char	*TransferInputFiles = "transfer_input_files";
const char	*TransferOutputFiles = "transfer_output_files";
const char    *TransferOutputRemaps = "transfer_output_remaps";
const char	*TransferExecutable = "transfer_executable";
const char	*TransferInput = "transfer_input";
const char	*TransferOutput = "transfer_output";
const char	*TransferError = "transfer_error";
const char  *MaxTransferInputMB = "max_transfer_input_mb";
const char  *MaxTransferOutputMB = "max_transfer_output_mb";

const char	*EncryptInputFiles = "encrypt_input_files";
const char	*EncryptOutputFiles = "encrypt_output_files";
const char	*DontEncryptInputFiles = "dont_encrypt_input_files";
const char	*DontEncryptOutputFiles = "dont_encrypt_output_files";

const char	*OutputDestination = "output_destination";

const char	*StreamInput = "stream_input";
const char	*StreamOutput = "stream_output";
const char	*StreamError = "stream_error";

const char	*CopyToSpool = "copy_to_spool";
const char	*LeaveInQueue = "leave_in_queue";

const char	*PeriodicHoldCheck = "periodic_hold";
const char	*PeriodicHoldReason = "periodic_hold_reason";
const char	*PeriodicHoldSubCode = "periodic_hold_subcode";
const char	*PeriodicReleaseCheck = "periodic_release";
const char	*PeriodicRemoveCheck = "periodic_remove";
const char	*OnExitHoldCheck = "on_exit_hold";
const char	*OnExitHoldReason = "on_exit_hold_reason";
const char	*OnExitHoldSubCode = "on_exit_hold_subcode";
const char	*OnExitRemoveCheck = "on_exit_remove";
const char	*Noop = "noop_job";
const char	*NoopExitSignal = "noop_job_exit_signal";
const char	*NoopExitCode = "noop_job_exit_code";

const char	*GlobusResubmit = "globus_resubmit";
const char	*GlobusRematch = "globus_rematch";

const char	*LastMatchListLength = "match_list_length";

const char	*DAGManJobId = "dagman_job_id";
const char	*LogNotesCommand = "submit_event_notes";
const char	*UserNotesCommand = "submit_event_user_notes";
const char	*JarFiles = "jar_files";
const char	*JavaVMArgs = "java_vm_args";
const char	*JavaVMArguments1 = "java_vm_arguments";
const char	*JavaVMArguments2 = "java_vm_arguments2";

const char    *ParallelScriptShadow  = "parallel_script_shadow";  
const char    *ParallelScriptStarter = "parallel_script_starter"; 

const char    *MaxJobRetirementTime = "max_job_retirement_time";

const char    *JobWantsAds = "want_ads";

//
// Job Deferral Parameters
//
const char	*DeferralTime	= "deferral_time";
const char	*DeferralWindow 	= "deferral_window";
const char	*DeferralPrepTime	= "deferral_prep_time";

// Job Retry Parameters
const char *MaxRetries = "max_retries";
const char *RetryUntil = "retry_until";
const char *SuccessExitCode = "success_exit_code";

//
// CronTab Parameters
// The index value below should be the # of parameters
//
const char	*CronMinute		= "cron_minute";
const char	*CronHour		= "cron_hour";
const char	*CronDayOfMonth	= "cron_day_of_month";
const char	*CronMonth		= "cron_month";
const char	*CronDayOfWeek	= "cron_day_of_week";
const char	*CronWindow		= "cron_window";
const char	*CronPrepTime	= "cron_prep_time";

const char	*RunAsOwner = "run_as_owner";
const char	*LoadProfile = "load_profile";

// Concurrency Limit parameters
const char    *ConcurrencyLimits = "concurrency_limits";
const char    *ConcurrencyLimitsExpr = "concurrency_limits_expr";

// Accounting Group parameters
const char* AcctGroup = "accounting_group";
const char* AcctGroupUser = "accounting_group_user";

//
// docker "universe" Parameters
//
const char    *DockerImage="docker_image";

//
// VM universe Parameters
//
const char    *VM_VNC = "vm_vnc";
const char    *VM_Type = "vm_type";
const char    *VM_Memory = "vm_memory";
const char    *VM_VCPUS = "vm_vcpus";
const char    *VM_MACAddr = "vm_macaddr";
const char    *VM_Checkpoint = "vm_checkpoint";
const char    *VM_Networking = "vm_networking";
const char    *VM_Networking_Type = "vm_networking_type";

//
// EC2 Query Parameters
//
const char* EC2AccessKeyId = "ec2_access_key_id";
const char* EC2SecretAccessKey = "ec2_secret_access_key";
const char* EC2AmiID = "ec2_ami_id";
const char* EC2UserData = "ec2_user_data";
const char* EC2UserDataFile = "ec2_user_data_file";
const char* EC2SecurityGroups = "ec2_security_groups";
const char* EC2SecurityIDs = "ec2_security_ids";
const char* EC2KeyPair = "ec2_keypair";
const char* EC2KeyPairFile = "ec2_keypair_file";
const char* EC2KeyPairAlt = "ec2_key_pair";
const char* EC2KeyPairFileAlt = "ec2_key_pair_file";
const char* EC2InstanceType = "ec2_instance_type";
const char* EC2ElasticIP = "ec2_elastic_ip";
const char* EC2EBSVolumes = "ec2_ebs_volumes";
const char* EC2AvailabilityZone= "ec2_availability_zone";
const char* EC2VpcSubnet = "ec2_vpc_subnet";
const char* EC2VpcIP = "ec2_vpc_ip";
const char* EC2TagNames = "ec2_tag_names";
const char* EC2SpotPrice = "ec2_spot_price";
const char* EC2BlockDeviceMapping = "ec2_block_device_mapping";
const char* EC2ParamNames = "ec2_parameter_names";
const char* EC2ParamPrefix = "ec2_parameter_";
const char* EC2IamProfileArn = "ec2_iam_profile_arn";
const char* EC2IamProfileName = "ec2_iam_profile_name";

const char* BoincAuthenticatorFile = "boinc_authenticator_file";

//
// GCE Parameters
//
const char* GceImage = "gce_image";
const char* GceAuthFile = "gce_auth_file";
const char* GceMachineType = "gce_machine_type";
const char* GceMetadata = "gce_metadata";
const char* GceMetadataFile = "gce_metadata_file";
const char* GcePreemptible = "gce_preemptible";
const char* GceJsonFile = "gce_json_file";

char const *next_job_start_delay = "next_job_start_delay";
char const *next_job_start_delay2 = "NextJobStartDelay";
char const *want_graceful_removal = "want_graceful_removal";
char const *job_max_vacate_time = "job_max_vacate_time";

const char * REMOTE_PREFIX="Remote_";

#if !defined(WIN32)
const char	*KillSig			= "kill_sig";
const char	*RmKillSig			= "remove_kill_sig";
const char	*HoldKillSig		= "hold_kill_sig";
const char	*KillSigTimeout		= "kill_sig_timeout";
#endif

void    SetJobDisableFileChecks();
void    SetSimpleJobExprs();
void	SetRemoteAttrs();
void 	reschedule();
void 	SetExecutable();
void 	SetUniverse();
void	SetMachineCount();
void 	SetImageSize();
void    SetRequestResources();
int64_t	calc_image_size_kb( const char *name);
int 	find_cmd( char *name );
char *	get_tok();
void 	SetStdFile( int which_file );
void 	SetPriority();
void 	SetNotification();
void	SetWantRemoteIO(void);
void 	SetNotifyUser ();
void	SetEmailAttributes();
void 	SetCronTab();
void	SetRemoteInitialDir();
void	SetExitRequirements();
void	SetOutputDestination();
void 	SetArguments();
void 	SetJobDeferral();
void	SetJobRetries();
void 	SetEnvironment();
#if !defined(WIN32)
void 	ComputeRootDir();
void 	SetRootDir();
#endif
void 	SetRequirements();
void 	SetTransferFiles();
void    process_input_file_list(StringList * input_list, char *input_files, bool * files_specified);
void 	SetPerFileEncryption();
void	InsertFileTransAttrs( FileTransferOutput_t when_output );
void 	SetEncryptExecuteDir();
void 	SetTDP();
void	SetRunAsOwner();
void    SetLoadProfile();
void	SetRank();
void 	SetIWD();
void 	ComputeIWD();
void	SetUserLog();
void    SetUserLogXML();
void	SetCoreSize();
void	SetFileOptions();
#if !defined(WIN32)
void	SetKillSig();
#endif
void	SetForcedAttributes();
int		DoUnitTests(int options);
void 	check_iwd( char const *iwd );
int 	read_submit_file( FILE *fp );
const char * is_queue_statement(const char * line); // return ptr to queue args of this is a queue statement
char * 	condor_param( const char* name, const char* alt_name );
char * 	condor_param( const char* name ); // call param with NULL as the alt
void 	set_condor_param( const char* name, const char* value);
void 	set_condor_param_used( const char* name);
// stuff value into the submit's hashtable and mark 'name' as a used param
// this function is intended for use during queue iteration to stuff changing values like $(Cluster) and $(Process)
// Because of this the function does NOT make a copy of value, it's up to the caller to
// make sure that value is not changed for the lifetime of possible macro substitution.
void 	set_live_submit_variable(const char* name, const char* live_value, bool force_used=true);
bool 	check_requirements( char const *orig, MyString &answer );
void 	check_open( const char *name, int flags );
void 	usage();
void 	init_params();
int 	whitespace( const char *str);
void 	compress( MyString &path );
char const*full_path(const char *name, bool use_iwd=true);
void 	get_time_conv( int &hours, int &minutes );
int	  SaveClassAd ();
void InsertJobExpr (const char *expr, bool from_config_file=false);
void InsertJobExpr (const MyString &expr);
void InsertJobExprInt(const char * name, int val);
void InsertJobExprString(const char * name, const char * val);
#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
bool IsNoClusterAttr(const char * name);
#else
void SetNoClusterAttr(const char * name);
#endif
void	check_umask();
void setupAuthentication();
void	SetPeriodicHoldCheck(void);
void	SetPeriodicRemoveCheck(void);
void	SetNoopJob(void);
void	SetNoopJobExitSignal(void);
void	SetNoopJobExitCode(void);
void SetDAGNodeName();
void SetMatchListLen();
void SetDAGManJobId();
void SetLogNotes();
void SetUserNotes();
void SetJarFiles();
void SetJavaVMArgs();
void SetParallelStartupScripts(); //JDB
void SetMaxJobRetirementTime();
bool mightTransfer( int universe );
bool isTrue( const char* attr );
void SetConcurrencyLimits();
void SetAccountingGroup();
void SetVMParams();
void SetVMRequirements();
bool parse_vm_option(char *value, bool& onoff);
void transfer_vm_file(const char *filename);
bool validate_disk_param(const char *disk, int min_params=3, int max_params=4);

char *owner = NULL;
char *ntdomain = NULL;
char *myproxy_password = NULL;
bool stream_std_file = false;
classad::References forced_submit_attrs;
#endif

#ifdef USE_SUBMIT_UTILS
void usage();
void init_params();
void reschedule();
int  read_submit_file( FILE *fp );
void check_umask();
void setupAuthentication();
const char * is_queue_statement(const char * line); // return ptr to queue args of this is a queue statement
bool IsNoClusterAttr(const char * name);
int  check_sub_file(void*pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags);
int  SendLastExecutable();
int  SendJobAd (ClassAd * job, ClassAd * ClusterAd);
int  DoUnitTests(int options);

char *owner = NULL;
char *myproxy_password = NULL;
#endif

int  SendJobCredential();
void SetSendCredentialInAd( ClassAd *job_ad );

extern DLL_IMPORT_MAGIC char **environ;

extern "C" {
int SetSyscalls( int foo );
int DoCleanup(int,int,const char*);
}

struct SubmitErrContext ErrContext = { PHASE_INIT, -1, -1, NULL, NULL };

// this will get called when the condor EXCEPT() macro is invoked.
// for the most part, we want to report the message, but to use submit file and
// line numbers rather and the source file and line numbers that are passed in.
void ReportSubmitException(const char * msg, int /*src_line*/, const char * /*src_file*/)
{
	char loc[100];
	switch (ErrContext.phase) {
	case PHASE_READ_SUBMIT: sprintf(loc, " on Line %d of submit file", FileMacroSource.line); break;
	case PHASE_DASH_APPEND: sprintf(loc, " with -a argument #%d", ExtraLineNo); break;
	case PHASE_QUEUE:       sprintf(loc, " at Queue statement on Line %d", FileMacroSource.line); break;
	case PHASE_QUEUE_ARG:   sprintf(loc, " with -queue argument"); break;
	default: loc[0] = 0; break;
	}

	fprintf( stderr, "ERROR%s: %s\n", loc, msg );
	if ((ErrContext.phase == PHASE_QUEUE || ErrContext.phase == PHASE_QUEUE_ARG) && ErrContext.step >= 0) {
		if (ErrContext.raw_macro_val && ErrContext.raw_macro_val[0]) {
			fprintf(stderr, "\t at Step %d, ItemIndex %d while expanding %s=%s\n",
				ErrContext.step, ErrContext.item_index, ErrContext.macro_name, ErrContext.raw_macro_val);
		} else {
			fprintf(stderr, "\t at Step %d, ItemIndex %d\n", ErrContext.step, ErrContext.item_index);
		}
	}
}


struct SubmitRec {
	int cluster;
	int firstjob;
	int lastjob;
};

ExtArray <SubmitRec> SubmitInfo(10);
int CurrentSubmitInfo = -1;

ExtArray <ClassAd*> JobAdsArray(100);
int JobAdsArrayLen = 0;
#ifdef USE_SUBMIT_UTILS
int JobAdsArrayLastClusterIndex = 0;

// called by the factory submit to fill out the data structures that
// we use to print out the standard messages on complection.
void set_factory_submit_info(int cluster, int num_procs)
{
	CurrentSubmitInfo++;
	SubmitInfo[CurrentSubmitInfo].cluster = cluster;
	SubmitInfo[CurrentSubmitInfo].firstjob = 0;
	SubmitInfo[CurrentSubmitInfo].lastjob = num_procs-1;
}

#else

// explicit template instantiations
bool condor_param_exists(const char* name, const char * alt_name, std::string & value)
{
	auto_free_ptr result(condor_param(name, alt_name));
	if ( ! result)
		return false;

	value = result.ptr();
	return true;
}

MyString 
condor_param_mystring( const char * name, const char * alt_name )
{
	char * result = condor_param(name, alt_name);
	MyString ret = result;
	free(result);
	return ret;
}

bool condor_param_long_exists(const char* name, const char * alt_name, long long & value, bool int_range=false)
{
	auto_free_ptr result(condor_param(name, alt_name));
	if ( ! result)
		return false;

	if ( ! string_is_long_param(result.ptr(), value) ||
		(int_range && (value < INT_MIN || value >= INT_MAX)) ) {
		fprintf(stderr, "\nERROR: %s=%s is invalid, must eval to an integer.\n", name, result.ptr());
		DoCleanup(0,0,NULL);
		exit(1);
	}

	return true;
}

int condor_param_int(const char* name, const char * alt_name, int def_value)
{
	long long value = def_value;
	if ( ! condor_param_long_exists(name, alt_name, value, true)) {
		value = def_value;
	}
	return (int)value;
}

void non_negative_int_fail(const char * Name, char * Value)
{

	int iTemp=0;
	if (strstr(Value,".") || 
		(sscanf(Value, "%d", &iTemp) > 0 && iTemp < 0))
	{
		fprintf( stderr, "\nERROR: '%s'='%s' is invalid, must eval to a non-negative integer.\n", Name, Value );
		DoCleanup(0,0,NULL);
		exit(1);
	}
	
	// sigh lexical_cast<>
}

int condor_param_bool(const char* name, const char * alt_name, bool def_value)
{
	char * result = condor_param(name, alt_name);
	if ( ! result)
		return def_value;

	bool value = def_value;
	if (*result) {
		if ( ! string_is_boolean_param(result, value)) {
			fprintf(stderr, "\nERROR: %s=%s is invalid, must eval to a boolean.\n", name, result);
			DoCleanup(0,0,NULL);
			exit(1);
		}
	}
	free(result);
	return value;
}

// parse a input string for an int64 value optionally followed by K,M,G,or T
// as a scaling factor, then divide by a base scaling factor and return the
// result by ref. base is expected to be a multiple of 2 usually  1, 1024 or 1024*1024.
// result is truncated to the next largest value by base.
//
// Return value is true if the input string contains only a valid int, false if
// there are any unexpected characters other than whitespace.  value is
// unmodified when false is returned.
//
// this function exists to regularize the former ad-hoc parsing of integers in the
// submit file, it expands parsing to handle 64 bit ints and multiplier suffixes.
// Note that new classads will interpret the multiplier suffixes without
// regard for the fact that the defined units of many job ad attributes are
// in Kbytes or Mbytes. We need to parse them in submit rather than
// passing the expression on to the classad code to be parsed to preserve the
// assumption that the base units of the output is not bytes.
//
static bool parse_int64_bytes(const char * input, int64_t & value, int base)
{
	const char * tmp = input;
	while (isspace(*tmp)) ++tmp;

	char * p;
#ifdef WIN32
	int64_t val = _strtoi64(tmp, &p, 10);
#else
	int64_t val = strtol(tmp, &p, 10);
#endif

	// allow input to have a fractional part, so "2.2M" would be valid input.
	// this doesn't have to be very accurate, since we round up to base anyway.
	double fract = 0;
	if (*p == '.') {
		++p;
		if (isdigit(*p)) { fract += (*p - '0') / 10.0; ++p; }
		if (isdigit(*p)) { fract += (*p - '0') / 100.0; ++p; }
		if (isdigit(*p)) { fract += (*p - '0') / 1000.0; ++p; }
		while (isdigit(*p)) ++p;
	}

	// if the first non-space character wasn't a number
	// then this isn't a simple integer, return false.
	if (p == tmp)
		return false;

	while (isspace(*p)) ++p;

	// parse the multiplier postfix
	int64_t mult = 1;
	if (!*p) mult = base;
	else if (*p == 'k' || *p == 'K') mult = 1024;
	else if (*p == 'm' || *p == 'M') mult = 1024*1024;
	else if (*p == 'g' || *p == 'G') mult = (int64_t)1024*1024*1024;
	else if (*p == 't' || *p == 'T') mult = (int64_t)1024*1024*1024*1024;
	else return false;

	val = (int64_t)((val + fract) * mult + base-1) / base;

	// if we to to here and we are at the end of the string
	// then the input is valid, return true;
	if (!*p || !p[1]) { 
		value = val;
		return true; 
	}

	// Tolerate a b (as in Kb) and whitespace at the end, anything else and return false)
	if (p[1] == 'b' || p[1] == 'B') p += 2;
	while (isspace(*p)) ++p;
	if (!*p) {
		value = val;
		return true;
	}

	return false;
}

/** Given a universe in string form, return the number

Passing a universe in as a null terminated string in univ.  This can be
a case-insensitive word ("standard", "java"), or the associated number (1, 7).
Returns the integer of the universe.  In the event a given universe is not
understood, returns 0.

(The "Ex"tra functionality over CondorUniverseNumber is that it will
handle a string of "1".  This is primarily included for backward compatility
with the old icky way of specifying a Remote_Universe.
*/
static int CondorUniverseNumberEx(const char * univ)
{
	if( univ == 0 ) {
		return 0;
	}

	if( atoi(univ) != 0) {
		return atoi(univ);
	}

	return CondorUniverseNumber(univ);
}
#endif

void TestFilePermissions( char *scheddAddr = NULL )
{
#ifdef WIN32
	// this isn't going to happen on Windows since:
	// 1. this uid/gid stuff isn't portable to windows
	// 2. The shadow runs as the user now anyways, so there's no
	//    need for condor to waste time finding out if SYSTEM can
	//    write to some path. The one exception is if the user
	//    submits from AFS, but we're documenting that as unsupported.
	
#else
	gid_t gid = getgid();
	uid_t uid = getuid();

	int result, junk;
	MyString name;

	CheckFilesRead.startIterations();
	while( ( CheckFilesRead.iterate( name, junk ) ) )
	{
		result = attempt_access(name.Value(), ACCESS_READ, uid, gid, scheddAddr);
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not readable by condor.\n", 
					name.Value());
		}
	}

	CheckFilesWrite.startIterations();
	while( ( CheckFilesWrite.iterate( name, junk ) ) )
	{
		result = attempt_access(name.Value(), ACCESS_WRITE, uid, gid, scheddAddr );
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not writable by condor.\n",
					name.Value());
		}
	}
#endif
}

#ifdef USE_SUBMIT_UTILS
void print_errstack(FILE* out, CondorError *errstack)
{
	if ( ! errstack)
		return;

	for (/*nothing*/ ; ! errstack->empty(); errstack->pop()) {
		int code = errstack->code();
		std::string msg(errstack->message());
		if (msg.size() && msg[msg.size()-1] != '\n') { msg += '\n'; }
		if (code) {
			fprintf(out, "ERROR: %s", msg.c_str());
		} else {
			fprintf(out, "WARNING: %s", msg.c_str());
		}
	}
}
#else
void
init_job_ad()
{
	MyString buffer;
	ASSERT(owner);

	if (job) {
		delete job;
		job = NULL;
	}

	// set up types of the ad
	if ( !job ) {
		job = new ClassAd();
		SetMyTypeName (*job, JOB_ADTYPE);
		SetTargetTypeName (*job, STARTD_ADTYPE);
	}

	// all jobs should end up with the same qdate, so we only query time once.
	buffer.formatstr( "%s = %d", ATTR_Q_DATE, (int)get_submit_time());
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_COMPLETION_DATE);
	InsertJobExpr (buffer);

	if ( Remote ) {
		buffer.formatstr( "%s = Undefined", ATTR_OWNER );
	} else {
		buffer.formatstr( "%s = \"%s\"", ATTR_OWNER, owner);
	}
	InsertJobExpr (buffer);

#ifdef WIN32
	// put the NT domain into the ad as well
	ntdomain = my_domainname();
	buffer.formatstr( "%s = \"%s\"", ATTR_NT_DOMAIN, ntdomain);
	InsertJobExpr (buffer);

	// Publish the version of Windows we are running
	if (param_boolean("SUBMIT_PUBLISH_WINDOWS_OSVERSIONINFO", false)) {
		publishWindowsOSVersionInfo(*job);
	}
#endif

	buffer.formatstr( "%s = 0.0", ATTR_JOB_REMOTE_WALL_CLOCK);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0.0", ATTR_JOB_LOCAL_USER_CPU);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0.0", ATTR_JOB_LOCAL_SYS_CPU);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0.0", ATTR_JOB_REMOTE_USER_CPU);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0.0", ATTR_JOB_REMOTE_SYS_CPU);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0.0", ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0.0", ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_JOB_EXIT_STATUS);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_NUM_CKPTS);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_NUM_JOB_STARTS);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_NUM_JOB_COMPLETIONS);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_NUM_RESTARTS);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_NUM_SYSTEM_HOLDS);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_JOB_COMMITTED_TIME);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_COMMITTED_SLOT_TIME);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_CUMULATIVE_SLOT_TIME);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_TOTAL_SUSPENSIONS);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_LAST_SUSPENSION_TIME);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_CUMULATIVE_SUSPENSION_TIME);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = 0", ATTR_COMMITTED_SUSPENSION_TIME);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = FALSE", ATTR_ON_EXIT_BY_SIGNAL);
	InsertJobExpr (buffer);

#if 0 
	// can't use this because it doesn't handle of MY. or +attrs correctly
	config_fill_ad( job );
#else
	void param_and_insert_unique_items(const char * param_name, classad::References & attrs);

	classad::References submit_attrs;
	param_and_insert_unique_items("SUBMIT_ATTRS", submit_attrs);
	param_and_insert_unique_items("SUBMIT_EXPRS", submit_attrs);
	param_and_insert_unique_items("SYSTEM_SUBMIT_ATTRS", submit_attrs);

	if ( ! submit_attrs.empty()) {
		MyString buffer;

		for (classad::References::const_iterator it = submit_attrs.begin(); it != submit_attrs.end(); ++it) {
			if (starts_with(*it,"+")) {
				forced_submit_attrs.insert(it->substr(1));
				continue;
			} else if (starts_with_ignore_case(*it, "MY.")) {
				forced_submit_attrs.insert(it->substr(3));
				continue;
			}

			auto_free_ptr expr(param(it->c_str()));
			if ( ! expr) continue;
			buffer.formatstr("%s = %s", it->c_str(), expr.ptr());
			InsertJobExpr(buffer.c_str(), true);
		}
	}
	
	/* Insert the version into the ClassAd */
	job->Assign( ATTR_VERSION, CondorVersion() );
	job->Assign( ATTR_PLATFORM, CondorPlatform() );
#endif
}
#endif


int
main( int argc, const char *argv[] )
{
	FILE	*fp;
	const char **ptr;
	const char *pcolon = NULL;
	const char *cmd_file = NULL;
	int i;
	MyString method;

	setbuf( stdout, NULL );

	set_mySubSystem( "SUBMIT", SUBSYSTEM_TYPE_SUBMIT );

#if !defined(WIN32)
		// Make sure root isn't trying to submit.
	if( getuid() == 0 || getgid() == 0 ) {
		fprintf( stderr, "\nERROR: Submitting jobs as user/group 0 (root) is not "
				 "allowed for security reasons.\n" );
		exit( 1 );
	}
#endif /* not WIN32 */

	MyName = condor_basename(argv[0]);
	myDistro->Init( argc, argv );
	config();

	//TODO:this should go away, and the owner name be placed in ad by schedd!
	owner = my_username();
	if( !owner ) {
		owner = strdup("unknown");
	}

	init_params();
#ifdef USE_SUBMIT_UTILS
	submit_hash.init();

	default_to_factory = param_boolean("SUBMIT_FACTORY_JOBS_BY_DEFAULT", default_to_factory);
#endif

		// If our effective and real gids are different (b/c the
		// submit binary is setgid) set umask(002) so that stdout,
		// stderr and the user log files are created writable by group
		// condor.
	check_umask();

	set_debug_flags(NULL, D_EXPR);

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, (SIG_HANDLER)SIG_IGN );
#endif

	bool query_credential = true;

	for( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if (MATCH == strcmp(ptr[0], "-")) { // treat a bare - as a submit filename, it means read from stdin.
				cmd_file = ptr[0];
			} else if (is_dash_arg_prefix(ptr[0], "verbose", 1)) {
				verbose = true; terse = false;
			} else if (is_dash_arg_prefix(ptr[0], "terse", 3)) {
				terse = true; verbose = false;
			} else if (is_dash_arg_prefix(ptr[0], "disable", 1)) {
				DisableFileChecks = 1;
			} else if (is_dash_arg_prefix(ptr[0], "debug", 2)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", 0);
			} else if (is_dash_arg_colon_prefix(ptr[0], "dry-run", &pcolon, 3)) {
				DashDryRun = 1;
				bool needs_file_arg = true;
				if (pcolon) { 
					needs_file_arg = false;
					int opt = atoi(pcolon+1);
					if (opt > 1) {  DashDryRun =  opt; needs_file_arg = opt < 0x10; }
					else if (MATCH == strcmp(pcolon+1, "hash")) {
						DumpSubmitHash = 1;
						needs_file_arg = true;
					}
				}
				if (needs_file_arg) {
					if (DumpClassAdToFile) {
						fprintf(stderr, "%s: -dry-run <file> and -dump <file> cannot be used together\n", MyName);
						exit(1);
					}
					const char * pfilearg = ptr[1];
					if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
						fprintf( stderr, "%s: -dry-run requires another argument\n", MyName );
						exit(1);
					}
					DashDryRunOutName = pfilearg;
					--argc; ++ptr;
				}

			} else if (is_dash_arg_prefix(ptr[0], "spool", 1)) {
				dash_remote++;
				DisableFileChecks = 1;
			} else if (is_dash_arg_prefix(ptr[0], "factory", 1)) {
				dash_factory++;
				DisableFileChecks = 1;
			} else if (is_dash_arg_prefix(ptr[0], "address", 2)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf(stderr, "%s: -address requires another argument\n", MyName);
					exit(1);
				}
                if (!is_valid_sinful(*ptr)) {
                    fprintf(stderr, "%s: \"%s\" is not a valid address\n", MyName, *ptr);
                    fprintf(stderr, "Should be of the form <ip:port>\n");
                    fprintf(stderr, "For example: <123.456.789.123:6789>\n");
                    exit(1);
                }
                ScheddAddr = *ptr;
			} else if (is_dash_arg_prefix(ptr[0], "remote", 1)) {
				dash_remote++;
				DisableFileChecks = 1;
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -remote requires another argument\n",
							 MyName );
					exit(1);
				}
				if( ScheddName ) {
					delete [] ScheddName;
				}
				if( !(ScheddName = get_daemon_name(*ptr)) ) {
					fprintf( stderr, "%s: unknown host %s\n",
							 MyName, get_host_part(*ptr) );
					exit(1);
				}
				query_credential = false;
			} else if (is_dash_arg_prefix(ptr[0], "name", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -name requires another argument\n",
							 MyName );
					exit(1);
				}
				if( ScheddName ) {
					delete [] ScheddName;
				}
				if( !(ScheddName = get_daemon_name(*ptr)) ) {
					fprintf( stderr, "%s: unknown host %s\n",
							 MyName, get_host_part(*ptr) );
					exit(1);
				}
				query_credential = false;
			} else if (is_dash_arg_prefix(ptr[0], "append", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -append requires another argument\n",
							 MyName );
					exit( 1 );
				}
				// if appended submit line looks like a queue statement, save it off for queue processing.
				if (is_queue_statement(ptr[0])) {
					if ( ! queueCommandLine.empty()) {
						fprintf(stderr, "%s: only one -queue command allowed\n", MyName);
						exit(1);
					}
					queueCommandLine = ptr[0];
				} else {
					extraLines.Append( *ptr );
				}
			} else if (is_dash_arg_prefix(ptr[0], "batch-name", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -batch-name requires another argument\n",
							 MyName );
					exit( 1 );
				}
				const char * bname = *ptr;
				MyString tmp; // if -batch-name was specified, this holds the string 'MY.JobBatchName = "name"'
				if (*bname == '"') {
					tmp.formatstr("MY.%s = %s", ATTR_JOB_BATCH_NAME, bname);
				} else {
					tmp.formatstr("MY.%s = \"%s\"", ATTR_JOB_BATCH_NAME, bname);
				}
				// if batch_name_line is not NULL,  we will leak a bit here, but that's better than
				// freeing something behind the back of the extraLines
				batch_name_line = strdup(tmp.c_str());
				extraLines.Append(const_cast<const char*>(batch_name_line));
			} else if (is_dash_arg_prefix(ptr[0], "queue", 1)) {
				if( !(--argc) || (!(*ptr[1]) || *ptr[1] == '-')) {
					fprintf( stderr, "%s: -queue requires at least one argument\n",
							 MyName );
					exit( 1 );
				}
				if ( ! queueCommandLine.empty()) {
					fprintf(stderr, "%s: only one -queue command allowed\n", MyName);
					exit(1);
				}

				// The queue command uses arguments until it sees one starting with -
				// we do this so that shell glob expansion behaves like you would expect.
				++ptr;
				queueCommandLine = "queue "; queueCommandLine += ptr[0];
				while (argc > 1) {
					bool is_dash = (MATCH == strcmp(ptr[1], "-"));
					if (is_dash || ptr[1][0] != '-') {
						queueCommandLine += " ";
						queueCommandLine += ptr[1];
						++ptr;
						--argc;
						if ( ! is_dash) continue;
					}
					break;
				}
			} else if (is_dash_arg_prefix (ptr[0], "maxjobs", 3)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -maxjobs requires another argument\n",
							 MyName );
					exit(1);
				}
				// read the next argument to set DashMaxJobs as an integer and
				// set it to the job limit.
				char *endptr;
				DashMaxJobs = strtol(*ptr, &endptr, 10);
				if (*endptr != '\0') {
					fprintf(stderr, "Error: Unable to convert argument (%s) to a number for -maxjobs.\n", *ptr);
					exit(1);
				}
				if (DashMaxJobs < -1) {
					fprintf(stderr, "Error: %d is not a valid for -maxjobs.\n", DashMaxJobs);
					exit(1);
				}
			} else if (is_dash_arg_prefix (ptr[0], "single-cluster", 3)) {
				DashMaxClusters = 1;
			} else if (is_dash_arg_prefix( ptr[0], "password", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -password requires another argument\n",
							 MyName );
				}
				myproxy_password = strdup (*ptr);
			} else if (is_dash_arg_prefix(ptr[0], "pool", 2)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -pool requires another argument\n",
							 MyName );
					exit(1);
				}
				if( PoolName ) {
					delete [] PoolName;
				}
					// TODO We should try to resolve the name to a full
					//   hostname, but get_full_hostname() doesn't like
					//   seeing ":<port>" at the end, which is valid for a
					//   collector name.
				PoolName = strnewp( *ptr );
			} else if (is_dash_arg_prefix(ptr[0], "stm", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -stm requires another argument\n",
							 MyName );
					exit(1);
				}
				method = *ptr;
				string_to_stm(method, STMethod);
			} else if (is_dash_arg_prefix(ptr[0], "unused", 1)) {
				WarnOnUnusedMacros = WarnOnUnusedMacros == 1 ? 0 : 1;
				// TOGGLE? 
				// -- why not? if you set it to warn on unused macros in the 
				// config file, there should be a way to turn it off
			} else if (is_dash_arg_prefix(ptr[0], "dump", 2)) {
				if (DashDryRunOutName) {
					fprintf(stderr, "%s: -dump <file> and -dry-run <file> cannot be used together\n", MyName);
					exit(1);
				}
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -dump requires another argument\n",
						MyName );
					exit(1);
				}
				DumpFileName = *ptr;
				DumpClassAdToFile = true;
				DumpFileIsStdout = (MATCH == strcmp(DumpFileName,"-"));
				// if we are dumping to a file, we never want to check file permissions
				// as this would initiate some schedd communication
				DisableFileChecks = 1;
				// we don't really want to do this because there is no real 
				// schedd to query the credentials from...
				query_credential = false;
			} else if (is_dash_arg_prefix(ptr[0], "force-mpi-universe", 7)) {
				use_condor_mpi_universe = true;
			} else if (is_dash_arg_prefix(ptr[0], "help")) {
				usage();
				exit( 0 );
			} else if (is_dash_arg_prefix(ptr[0], "interactive", 1)) {
				// we don't currently support -interactive on Windows, but we parse for it anyway.
				dash_interactive = 1;
				extraLines.Append( "+InteractiveJob=True" );
			} else {
				usage();
				exit( 1 );
			}
		} else if (strchr(ptr[0],'=')) {
			// loose arguments that contain '=' are attrib=value pairs.
			// so we split on the = into name and value and insert into the submit hashtable
			// we do this BEFORE parsing the submit file so that they can be referred to by submit.
			std::string name(ptr[0]);
			size_t ix = name.find('=');
			std::string value(name.substr(ix+1));
			name = name.substr(0,ix);
			trim(name); trim(value);
			if ( ! name.empty() && name[1] == '+') {
				name = "MY." + name.substr(1);
			}
			if (name.empty() || ! is_valid_param_name(name.c_str())) {
				fprintf( stderr, "%s: invalid attribute name '%s' for attrib=value assigment\n", MyName, name.c_str() );
				exit(1);
			}
#ifdef USE_SUBMIT_UTILS
			submit_hash.set_arg_variable(name.c_str(), value.c_str());
#else
			MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT",0);
			insert_macro(name.c_str(), value.c_str(), SubmitMacroSet, ArgumentMacro, ctx);
#endif
		} else {
			cmd_file = *ptr;
		}
	}

	// Have reading the submit file from stdin imply -verbose. This is
	// for backward compatibility with HTCondor version 8.1.1 and earlier
	// -terse can be used to override the backward compatable behavior.
	SubmitFromStdin = cmd_file && ! strcmp(cmd_file, "-");
	if (SubmitFromStdin && ! terse) {
		verbose = true;
	}

	// ensure I have a known transfer method
	if (STMethod == STM_UNKNOWN) {
		fprintf( stderr, 
			"%s: Unknown sandbox transfer method: %s\n", MyName, method.Value());
		usage();
		exit(1);
	}

	// we only want communication with the schedd to take place... because
	// that's the only type of communication we are interested in
	if ( DumpClassAdToFile && STMethod != STM_USE_SCHEDD_ONLY ) {
		fprintf( stderr, 
			"%s: Dumping ClassAds to a file is not compatible with sandbox "
			"transfer method: %s\n", MyName, method.Value());
		usage();
		exit(1);
	}

	if (!DisableFileChecks) {
		DisableFileChecks = param_boolean_crufty("SUBMIT_SKIP_FILECHECKS", true) ? 1 : 0;
	}

	MaxProcsPerCluster = param_integer("SUBMIT_MAX_PROCS_IN_CLUSTER", 0, 0);

	// the -dry argument takes a qualifier that I'm hijacking to do queue parsing unit tests for now the 8.3 series.
	if (DashDryRun > 0x10) { exit(DoUnitTests(DashDryRun)); }

	// we don't want a schedd instance if we are dumping to a file
	if ( !DumpClassAdToFile ) {
		// Instantiate our DCSchedd so we can locate and communicate
		// with our schedd.  
        if (!ScheddAddr.empty()) {
            MySchedd = new DCSchedd(ScheddAddr.c_str());
        } else {
            MySchedd = new DCSchedd(ScheddName, PoolName);
        }
		if( ! MySchedd->locate() ) {
			if( ScheddName ) {
				fprintf( stderr, "\nERROR: Can't find address of schedd %s\n",
						 ScheddName );
			} else {
				fprintf( stderr, "\nERROR: Can't find address of local schedd\n" );
			}
			exit(1);
		}
	}


	// make sure our shadow will have access to our credential
	// (check is disabled for "-n" and "-r" submits)
	if (query_credential) {

#ifdef WIN32
		// setup the username to query
		char userdom[256];
		char* the_username = my_username();
		char* the_domainname = my_domainname();
		sprintf(userdom, "%s@%s", the_username, the_domainname);
		free(the_username);
		free(the_domainname);

		// if we have a credd, query it first
		bool cred_is_stored = false;
		int store_cred_result;
		Daemon my_credd(DT_CREDD);
		if (my_credd.locate()) {
			store_cred_result = store_cred(userdom, NULL, QUERY_MODE, &my_credd);
			if ( store_cred_result == SUCCESS ||
							store_cred_result == FAILURE_NOT_SUPPORTED) {
				cred_is_stored = true;
			}
		}

		if (!cred_is_stored) {
			// query the schedd
			store_cred_result = store_cred(userdom, NULL, QUERY_MODE, MySchedd);
			if ( store_cred_result == SUCCESS ||
							store_cred_result == FAILURE_NOT_SUPPORTED) {
				cred_is_stored = true;
			}
		}
		if (!cred_is_stored) {
			fprintf( stderr, "\nERROR: No credential stored for %s\n"
					"\n\tCorrect this by running:\n"
					"\tcondor_store_cred add\n", userdom );
			exit(1);
		}
#else
		SendJobCredential();
#endif
	}

	// if this is an interactive job, and no cmd_file was specified on the
	// command line, use a default cmd file as specified in the config table.
	if ( dash_interactive ) {
		if ( !cmd_file ) {
			cmd_file = param("INTERACTIVE_SUBMIT_FILE");
			if (cmd_file) {
				InteractiveSubmitFile = 1;
			} 
		}
		// If the user specified their own submit file for an interactive
		// submit, "rewrite" the job to run /bin/sleep.
		// This effectively means executable, transfer_executable,
		// arguments, universe, and queue X are ignored from the submit
		// file and instead we rewrite to the values below.
		if ( !InteractiveSubmitFile ) {
			extraLines.Append( "executable=/bin/sleep" );
			extraLines.Append( "transfer_executable=false" );
			extraLines.Append( "arguments=180" );
		}
	}

	if ( ! cmd_file && (DashDryRunOutName || DumpFileName)) {
		const char * dump_name = DumpFileName ? DumpFileName : DashDryRunOutName;
		fprintf( stderr,
			"\nERROR: No submit filename was provided, and '%s'\n"
			"  was given as the output filename for -dump or -dry-run. Was this intended?\n"
			"  To to avoid confusing the output file with the submit file when using these\n"
			"  commands, an explicit submit filename argument must be given. You can use -\n"
			"  as the submit filename argument to read from stdin.\n",
			dump_name);
		exit(1);
	}

	bool has_late_materialize = false;
	CondorVersionInfo cvi(MySchedd ? MySchedd->version() : CondorVersion());
	if (cvi.built_since_version(8,7,1)) {
		has_late_materialize = true;
		if ( ! DumpClassAdToFile && ! DashDryRun && ! dash_interactive && default_to_factory) { dash_factory = true; }
	}
	if (dash_factory && ! has_late_materialize) {
		fprintf(stderr, "\nERROR: Schedd does not support late materialization\n");
		exit(1);
	} else if (dash_factory && dash_interactive) {
		fprintf(stderr, "\nERROR: Interactive jobs cannot be materialized late\n");
		exit(1);
	}

	// open submit file
	if ( ! cmd_file || SubmitFromStdin) {
		// no file specified, read from stdin
		fp = stdin;
#ifdef USE_SUBMIT_UTILS
		submit_hash.insert_source("<stdin>", FileMacroSource);
#else
		insert_source("<stdin>", SubmitMacroSet, FileMacroSource);
#endif
	} else {
		if( (fp=safe_fopen_wrapper_follow(cmd_file,"r")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open command file (%s)\n",
						strerror(errno));
			exit(1);
		}
#ifdef USE_SUBMIT_UTILS
		// this does both insert_source, and also gives a values to the default $(SUBMIT_FILE) expansion
		submit_hash.insert_submit_filename(cmd_file, FileMacroSource);
		submit_unique_id = hashFuncChars(cmd_file);
#else
		insert_source(cmd_file, SubmitMacroSet, FileMacroSource);
		SubmitFileMacroDef.psz = const_cast<char*>(SubmitMacroSet.apool.insert(full_path(cmd_file, false)));
#endif
	}

	// in case things go awry ...
	_EXCEPT_Reporter = ReportSubmitException;
	_EXCEPT_Cleanup = DoCleanup;

#ifdef USE_SUBMIT_UTILS
	submit_hash.setDisableFileChecks(DisableFileChecks);
	submit_hash.setFakeFileCreationChecks(DashDryRun);
	submit_hash.setScheddVersion(MySchedd ? MySchedd->version() : CondorVersion());
	if (myproxy_password) submit_hash.setMyProxyPassword(myproxy_password);
	submit_hash.init_cluster_ad(get_submit_time(), owner);
#else
	IsFirstExecutable = true;
	init_job_ad();
#endif

	if ( !DumpClassAdToFile ) {
		if ( ! SubmitFromStdin && ! terse) {
			fprintf(stdout, DashDryRun ? "Dry-Run job(s)" : "Submitting job(s)");
		}
	} else if ( ! DumpFileIsStdout ) {
		// get the file we are to dump the ClassAds to...
		if ( ! terse) { fprintf(stdout, "Storing job ClassAd(s)"); }
		if( (DumpFile = safe_fopen_wrapper_follow(DumpFileName,"w")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open file to dump ClassAds into (%s)\n",
				strerror(errno));
			exit(1);
		}
	}

	int rval = 0;
	//  Parse the file and queue the jobs
	if (dash_factory && has_late_materialize) {
		rval = submit_factory_job(fp, FileMacroSource, extraLines, queueCommandLine);
	} else {
		rval = read_submit_file(fp);
	}
	if( rval < 0 ) {
		if( ExtraLineNo == 0 ) {
			fprintf( stderr,
					 "\nERROR: Failed to parse command file (line %d).\n",
					 FileMacroSource.line);
		}
		else {
			fprintf( stderr,
					 "\nERROR: Failed to parse -a argument line (#%d).\n",
					 ExtraLineNo );
		}
		exit(1);
	}

	if( !GotQueueCommand ) {
		fprintf(stderr, "\nERROR: \"%s\" doesn't contain any \"queue\" commands -- no jobs queued\n",
				SubmitFromStdin ? "(stdin)" : cmd_file);
		exit( 1 );
	} else if ( ! GotNonEmptyQueueCommand && ! terse) {
		fprintf(stderr, "\nWARNING: \"%s\" has only empty \"queue\" commands -- no jobs queued\n",
				SubmitFromStdin ? "(stdin)" : cmd_file);
	}

	// we can't disconnect from something if we haven't connected to it: since
	// we are dumping to a file, we don't actually open a connection to the schedd
	if (MyQ) {
		CondorError errstack;
		if ( !MyQ->disconnect(true, errstack) ) {
			fprintf(stderr, "\nERROR: Failed to commit job submission into the queue.\n");
			if (errstack.subsys())
			{
				fprintf(stderr, "ERROR: %s\n", errstack.message());
			}
			exit(1);
		}
	}

	if ( ! SubmitFromStdin && ! terse) {
		fprintf(stdout, "\n");
	}

	ErrContext.phase = PHASE_COMMIT;

	if ( ! verbose && ! DumpFileIsStdout) {
		if (terse) {
			int ixFirst = 0;
			for (int ix = 0; ix <= CurrentSubmitInfo; ++ix) {
				// fprintf(stderr, "\t%d.%d - %d\n", SubmitInfo[ix].cluster, SubmitInfo[ix].firstjob, SubmitInfo[ix].lastjob);
				if ((ix == CurrentSubmitInfo) || SubmitInfo[ix].cluster != SubmitInfo[ix+1].cluster) {
					if (SubmitInfo[ixFirst].cluster >= 0) {
						fprintf(stdout, "%d.%d - %d.%d\n", 
							SubmitInfo[ixFirst].cluster, SubmitInfo[ixFirst].firstjob,
							SubmitInfo[ix].cluster, SubmitInfo[ix].lastjob);
					}
					ixFirst = ix+1;
				}
			}
		} else {
			int this_cluster = -1, job_count=0;
			for (i=0; i <= CurrentSubmitInfo; i++) {
				if (SubmitInfo[i].cluster != this_cluster) {
					if (this_cluster != -1) {
						fprintf(stdout, "%d job(s) %s to cluster %d.\n", job_count, DashDryRun ? "dry-run" : "submitted", this_cluster);
						job_count = 0;
					}
					this_cluster = SubmitInfo[i].cluster;
				}
				job_count += SubmitInfo[i].lastjob - SubmitInfo[i].firstjob + 1;
			}
			if (this_cluster != -1) {
				fprintf(stdout, "%d job(s) %s to cluster %d.\n", job_count, DashDryRun ? "dry-run" : "submitted", this_cluster);
			}
		}
	}

	ActiveQueueConnection = FALSE; 

    bool isStandardUni = false;
#ifdef USE_SUBMIT_UTILS
	isStandardUni = submit_hash.getUniverse() == CONDOR_UNIVERSE_STANDARD;
#else
	isStandardUni = JobUniverse == CONDOR_UNIVERSE_STANDARD;
#endif

	if ( !DisableFileChecks || isStandardUni) {
		TestFilePermissions( MySchedd->addr() );
	}

	// we don't want to spool jobs if we are simply writing the ClassAds to 
	// a file, so we just skip this block entirely if we are doing this...
	if ( !DumpClassAdToFile ) {
		if ( dash_remote && JobAdsArrayLen > 0 ) {
			bool result;
			CondorError errstack;

			switch(STMethod) {
				case STM_USE_SCHEDD_ONLY:
					// perhaps check for proper schedd version here?
					result = MySchedd->spoolJobFiles( JobAdsArrayLen,
											  JobAdsArray.getarray(),
											  &errstack );
					if ( !result ) {
						fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
						fprintf( stderr, "ERROR: Failed to spool job files.\n" );
						exit(1);
					}
					break;

				case STM_USE_TRANSFERD:
					{ // start block

					fprintf(stdout,
						"Locating a Sandbox for %d jobs.\n",JobAdsArrayLen);
					MyString td_sinful;
					MyString td_capability;
					ClassAd respad;
					int invalid;
					MyString reason;

					result = MySchedd->requestSandboxLocation( FTPD_UPLOAD, 
												JobAdsArrayLen,
												JobAdsArray.getarray(), FTP_CFTP,
												&respad, &errstack );
					if ( !result ) {
						fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
						fprintf( stderr, 
							"ERROR: Failed to get a sandbox location.\n" );
						exit(1);
					}

					respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
					if (invalid == TRUE) {
						fprintf( stderr, 
							"Schedd rejected sand box location request:\n");
						respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
						fprintf( stderr, "\t%s\n", reason.Value());
						return false;
					}

					respad.LookupString(ATTR_TREQ_TD_SINFUL, td_sinful);
					respad.LookupString(ATTR_TREQ_CAPABILITY, td_capability);

					dprintf(D_ALWAYS, "Got td: %s, cap: %s\n", td_sinful.Value(),
						td_capability.Value());

					fprintf(stdout,"Spooling data files for %d jobs.\n",
						JobAdsArrayLen);

					DCTransferD dctd(td_sinful.Value());

					result = dctd.upload_job_files( JobAdsArrayLen,
											  JobAdsArray.getarray(),
											  &respad, &errstack );
					if ( !result ) {
						fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
						fprintf( stderr, "ERROR: Failed to spool job files.\n" );
						exit(1);
					}

					} // end block

					break;

				default:
					EXCEPT("PROGRAMMER ERROR: Unknown sandbox transfer method");
					break;
			}
		}
	}

	// don't try to reschedule jobs if we are dumping to a file, because, again
	// there will be not schedd present to reschedule thru.
	if ( !DumpClassAdToFile ) {
		if( ProcId != -1 ) {
			reschedule();
		}
	}

	// Deallocate some memory just to keep Purify happy
	for (i=0;i<JobAdsArrayLen;i++) {
		delete JobAdsArray[i];
	}
#ifdef USE_SUBMIT_UTILS
	submit_hash.delete_job_ad();
#else
	delete job;
	delete ClusterAd;
#endif
	delete MySchedd;

	/*	print all of the parameters that were not actually expanded/used 
		in the submit file. but not if we never queued any jobs. since
		there would be a ton of false positives in that case.
	*/
	if (WarnOnUnusedMacros && GotNonEmptyQueueCommand) {
		if (verbose) { fprintf(stdout, "\n"); }
#ifdef USE_SUBMIT_UTILS
		submit_hash.warn_unused(stderr);
		// if there was an errorstack, then the above populates the errorstack rather than printing to stdout
		// so we now flush the errstack to stdout.
		print_errstack(stderr, submit_hash.error_stack());
#else

		// Force non-zero ref count for DAG_STATUS and FAILED_COUNT
		// these are specified for all DAG node jobs (see dagman_submit.cpp).
		// wenger 2012-03-26 (refactored by TJ 2015-March)
		increment_macro_use_count("DAG_STATUS", SubmitMacroSet);
		increment_macro_use_count("FAILED_COUNT", SubmitMacroSet);

		HASHITER it = hash_iter_begin(SubmitMacroSet);
		for ( ; !hash_iter_done(it); hash_iter_next(it) ) {
			MACRO_META * pmeta = hash_iter_meta(it);
			if (pmeta && !pmeta->use_count && !pmeta->ref_count) {
				const char *key = hash_iter_key(it);
				if (pmeta->source_id == LiveMacro.id) {
					fprintf(stderr, "WARNING: the Queue variable '%s' was unused by condor_submit. Is it a typo?\n", key);
				} else {
					const char *val = hash_iter_value(it);
					fprintf(stderr, "WARNING: the line '%s = %s' was unused by condor_submit. Is it a typo?\n", key, val);
				}
			}
		}
		hash_iter_delete(&it);
		
#endif
	}

	// If this is an interactive job, spawn ssh_to_job -auto-retry -X, and also
	// with pool and schedd names if they were passed into condor_submit
#ifdef WIN32
	if (dash_interactive &&  ClusterId != -1) {
		fprintf( stderr, "ERROR: -interactive not supported on this platform.\n" );
		exit(1);
	}
#else
	if (dash_interactive &&  ClusterId != -1) {
		char jobid[40];
		int i,j,rval;
		const char* sshargs[15];

		for (i=0;i<15;i++) sshargs[i]=NULL; // init all points to NULL
		i=0;
		sshargs[i++] = "condor_ssh_to_job"; // note: this must be in the PATH
		sshargs[i++] = "-auto-retry";
		sshargs[i++] = "-remove-on-interrupt";
		sshargs[i++] = "-X";
		if (PoolName) {
			sshargs[i++] = "-pool";
			sshargs[i++] = PoolName;
		}
		if (ScheddName) {
			sshargs[i++] = "-name";
			sshargs[i++] = ScheddName;
		}
		sprintf(jobid,"%d.0",ClusterId);
		sshargs[i++] = jobid;
		sleep(3);	// with luck, schedd will start the job while we sleep
		// We cannot fork before calling ssh_to_job, since ssh will want
		// direct access to the pseudo terminal. Since util functions like
		// my_spawn, my_popen, my_system and friends all fork, we cannot use
		// them. Since this is all specific to Unix anyhow, call exec directly.
		rval = execvp("condor_ssh_to_job", const_cast<char *const*>(sshargs));
		if (rval == -1 ) {
			int savederr = errno;
			fprintf( stderr, "ERROR: Failed to spawn condor_ssh_to_job" );
			// Display all args to ssh_to_job to stderr
			for (j=0;j<i;j++) {
				fprintf( stderr, " %s",sshargs[j] );
			}
			fprintf( stderr, " : %s\n",strerror(savederr));
			exit(1);
		}
	}
#endif

	return 0;
}

#ifdef USE_SUBMIT_UTILS
// callback passed to make_job_ad on the submit_hash that gets passed each input or output file
// so we can choose to do file checks. 
int check_sub_file(void* /*pv*/, SubmitHash * sub, _submit_file_role role, const char * pathname, int flags)
{
	if (role == SFR_LOG) {
		if ( !DumpClassAdToFile && !DashDryRun ) {
			// check that the log is a valid path
			if ( !DisableFileChecks ) {
				FILE* test = safe_fopen_wrapper_follow(pathname, "a+", 0664);
				if (!test) {
					fprintf(stderr,
							"\nERROR: Invalid log file: \"%s\" (%s)\n", pathname,
							strerror(errno));
					return 1;
				} else {
					fclose(test);
				}
			}

			// Check that the log file isn't on NFS
			bool nfs_is_error = param_boolean("LOG_ON_NFS_IS_ERROR", false);
			bool nfs = false;

			if ( nfs_is_error ) {
				if ( fs_detect_nfs( pathname, &nfs ) != 0 ) {
					fprintf(stderr,
							"\nWARNING: Can't determine whether log file %s is on NFS\n",
							pathname );
				} else if ( nfs ) {

					fprintf(stderr,
							"\nERROR: Log file %s is on NFS.\nThis could cause"
							" log file corruption. Condor has been configured to"
							" prohibit log files on NFS.\n",
							pathname );

					return 1;

				}
			}
		}
		return 0;

	} else if (role == SFR_EXECUTABLE || role == SFR_PSEUDO_EXECUTABLE) {

		const char * ename = pathname;
		bool transfer_it = (flags & 1) != 0;

		LastExecutable = ename;
		SpoolLastExecutable = false;

		// ensure the executables exist and spool them only if no
		// $$(arch).$$(opsys) are specified  (note that if we are simply
		// dumping the class-ad to a file, we won't actually transfer
		// or do anything [nothing that follows will affect the ad])
		if ( transfer_it && !DumpClassAdToFile && !strstr(ename,"$$") ) {

			StatInfo si(ename);
			if ( SINoFile == si.Error () ) {
				fprintf ( stderr, "\nERROR: Executable file %s does not exist\n", ename );
				return 1; // abort
			}

			if (!si.Error() && (si.GetFileSize() == 0)) {
				fprintf( stderr, "\nERROR: Executable file %s has zero length\n", ename );
				return 1; // abort
			}

			if (role == SFR_EXECUTABLE) {
				bool param_exists;
				SpoolLastExecutable = sub->submit_param_bool( SUBMIT_KEY_CopyToSpool, "CopyToSpool", false, &param_exists );
				if ( ! param_exists) {
					if ( submit_hash.getUniverse() == CONDOR_UNIVERSE_STANDARD ) {
							// Standard universe jobs can't restore from a checkpoint
							// if the executable changes.  Therefore, it is deemed
							// too risky to have copy_to_spool=false by default
							// for standard universe.
						SpoolLastExecutable = true;
					}
					else {
							// In so many cases, copy_to_spool=true would just add
							// needless overhead.  Therefore, (as of 6.9.3), the
							// default is false.
						SpoolLastExecutable = false;
					}
				}
			}
		}
		return 0;
	}

	// Queue files for testing access if not already queued
	int junk;
	if( flags & O_WRONLY )
	{
		if ( CheckFilesWrite.lookup(pathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesWrite.insert(pathname,junk);
		}
	}
	else
	{
		if ( CheckFilesRead.lookup(pathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesRead.insert(pathname,junk);
		}
	}
	return 0; // of the check is ok,  nonzero to abort
}
#else
/*
	Walk the list of submit commands (as stored in the
	insert() table) looking for a handful of Remote_FOO
	attributes we want to special case.  Translate them (if necessary)
	and stuff them into the ClassAd.
*/
void
SetRemoteAttrs()
{
	const int REMOTE_PREFIX_LEN = strlen(REMOTE_PREFIX);

	struct ExprItem {
		const char * submit_expr;
		const char * special_expr;
		const char * job_expr;
	};

	ExprItem tostringize[] = {
		{ GlobusRSL, "globus_rsl", ATTR_GLOBUS_RSL },
		{ NordugridRSL, "nordugrid_rsl", ATTR_NORDUGRID_RSL },
		{ GridResource, 0, ATTR_GRID_RESOURCE },
	};
	const int tostringizesz = sizeof(tostringize) / sizeof(tostringize[0]);


	HASHITER it = hash_iter_begin(SubmitMacroSet);
	for( ; ! hash_iter_done(it); hash_iter_next(it)) {

		const char * key = hash_iter_key(it);
		int remote_depth = 0;
		while(strncasecmp(key, REMOTE_PREFIX, REMOTE_PREFIX_LEN) == 0) {
			remote_depth++;
			key += REMOTE_PREFIX_LEN;
		}

		if(remote_depth == 0) {
			continue;
		}

		MyString preremote = "";
		for(int i = 0; i < remote_depth; ++i) {
			preremote += REMOTE_PREFIX;
		}

		if(strcasecmp(key, Universe) == 0 || strcasecmp(key, ATTR_JOB_UNIVERSE) == 0) {
			MyString Univ1 = preremote + Universe;
			MyString Univ2 = preremote + ATTR_JOB_UNIVERSE;
			MyString val = condor_param_mystring(Univ1.Value(), Univ2.Value());
			int univ = CondorUniverseNumberEx(val.Value());
			if(univ == 0) {
				fprintf(stderr, "ERROR: Unknown universe of '%s' specified\n", val.Value());
				exit(1);
			}
			MyString attr = preremote + ATTR_JOB_UNIVERSE;
			dprintf(D_FULLDEBUG, "Adding %s = %d\n", attr.Value(), univ);
			InsertJobExprInt(attr.Value(), univ);

		} else {

			for(int i = 0; i < tostringizesz; ++i) {
				ExprItem & item = tostringize[i];

				if(	strcasecmp(key, item.submit_expr) &&
					(item.special_expr == NULL || strcasecmp(key, item.special_expr)) &&
					strcasecmp(key, item.job_expr)) {
					continue;
				}
				MyString key1 = preremote + item.submit_expr;
				MyString key2 = preremote + item.special_expr;
				MyString key3 = preremote + item.job_expr;
				const char * ckey1 = key1.Value();
				const char * ckey2 = key2.Value();
				if(item.special_expr == NULL) { ckey2 = NULL; }
				const char * ckey3 = key3.Value();
				char * val = condor_param(ckey1, ckey2);
				if( val == NULL ) {
					val = condor_param(ckey3);
				}
				ASSERT(val); // Shouldn't have gotten here if it's missing.
				dprintf(D_FULLDEBUG, "Adding %s = %s\n", ckey3, val);
				InsertJobExprString(ckey3, val);
				free(val);
				break;
			}
		}
		


	}
	hash_iter_delete(&it);
}

void
SetJobMachineAttrs()
{
	MyString job_machine_attrs = condor_param_mystring( "job_machine_attrs", ATTR_JOB_MACHINE_ATTRS );
	MyString history_len_str = condor_param_mystring( "job_machine_attrs_history_length", ATTR_JOB_MACHINE_ATTRS_HISTORY_LENGTH );
	MyString buffer;

	if( job_machine_attrs.Length() ) {
		InsertJobExprString(ATTR_JOB_MACHINE_ATTRS,job_machine_attrs.Value());
	}
	if( history_len_str.Length() ) {
		char *endptr=NULL;
		long history_len = strtol(history_len_str.Value(),&endptr,10);
		if( history_len > INT_MAX || history_len < 0 || *endptr) {
			fprintf(stderr,"\nERROR: job_machine_attrs_history_length=%s is out of bounds 0 to %d\n",history_len_str.Value(),INT_MAX);
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		InsertJobExprInt(ATTR_JOB_MACHINE_ATTRS_HISTORY_LENGTH,(int)history_len);
	}
}
#endif

/*
** Send the reschedule command to the local schedd to get the jobs running
** as soon as possible.
*/
void
reschedule()
{
	if ( param_boolean("SUBMIT_SEND_RESCHEDULE",true) ) {
		Stream::stream_type st = MySchedd->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
		if ( ! MySchedd->sendCommand(RESCHEDULE, st, 0) ) {
			fprintf( stderr,
					 "Can't send RESCHEDULE command to condor scheduler\n" );
			DoCleanup(0,0,NULL);
		}
	}
}

#ifdef USE_SUBMIT_UTILS
#else

char * trim_and_strip_quotes_in_place(char * str)
{
	char * p = str;
	while (isspace(*p)) ++p;
	char * pe = p + strlen(p);
	while (pe > p && isspace(pe[-1])) --pe;
	*pe = 0;

	if (*p == '"' && pe > p && pe[-1] == '"') {
		// if we also had both leading and trailing quotes, strip them.
		if (pe > p && pe[-1] == '"') {
			*--pe = 0;
			++p;
		}
	}
	return p;
}

char * check_docker_image(char * docker_image)
{
	// trim leading & trailing whitespace and remove surrounding "" if any.
	docker_image = trim_and_strip_quotes_in_place(docker_image);

	// TODO: add code here to validate docker image argument (if possible)
	return docker_image;
}


/* check_path_length() has been deprecated in favor of 
 * check_and_universalize_path(), which not only checks
 * the length of the path string but also, on windows, it
 * takes any network drive paths and converts them to UNC
 * paths.
 */
int
check_and_universalize_path( MyString &path )
{
	(void) path;

	int retval = 0;
#ifdef WIN32
	/*
	 * On Windows, we need to convert all drive letters (mappings) 
	 * to their UNC equivalents, and make sure these network devices
	 * are accessable by the submitting user (not mapped as someone
	 * else).
	 */

	char volume[8];
	char netuser[80];
	unsigned char name_info_buf[MAX_PATH+1];
	DWORD name_info_buf_size = MAX_PATH+1;
	DWORD netuser_size = 80;
	DWORD result;
	UNIVERSAL_NAME_INFO *uni;

	result = 0;
	volume[0] = '\0';
	if ( path[0] && (path[1]==':') ) {
		sprintf(volume,"%c:",path[0]);
	}

	if (volume[0] && (GetDriveType(volume)==DRIVE_REMOTE))
	{
		char my_name[255];
		char *tmp, *net_name;

			// check if the user is the submitting user.
		
		result = WNetGetUser(volume, netuser, &netuser_size);
		tmp = my_username();
		strncpy(my_name, tmp, sizeof(my_name));
		free(tmp);
		tmp = NULL;
		getDomainAndName(netuser, tmp, net_name);

		if ( result == NOERROR ) {
			// We compare only the name (and not the domain) since
			// it's likely that the same account across different domains
			// has the same password, and is therefore a valid path for us
			// to use. It would be nice to check that the passwords truly
			// are the same on both accounts too, but since that would 
			// basically involve creating a whole separate mapping just to
			// test it, the expense is not worth it.
			
			if (strcasecmp(my_name, net_name) != 0 ) {
				fprintf(stderr, "\nERROR: The path '%s' is associated with\n"
				"\tuser '%s', but you're '%s', so Condor can\n"
			    "\tnot access it. Currently Condor only supports network\n"
			    "\tdrives that are associated with the submitting user.\n", 
					path.Value(), net_name, my_name);
				DoCleanup(0,0,NULL);
				exit( 1 );
			}
		} else {
			fprintf(stderr, 
				"\nERROR: Unable to get name of user associated with \n"
				"the following network path (err=%d):"
				"\n\t%s\n", result, path.Value());
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		result = WNetGetUniversalName(path.Value(), UNIVERSAL_NAME_INFO_LEVEL, 
			name_info_buf, &name_info_buf_size);
		if ( result != NO_ERROR ) {
			fprintf(stderr, 
				"\nERROR: Unable to get universal name for path (err=%d):"
				"\n\t%s\n", result, path.Value());
			DoCleanup(0,0,NULL);
			exit( 1 );			
		} else {
			uni = (UNIVERSAL_NAME_INFO*)&name_info_buf;
			path = uni->lpUniversalName;
			retval = 1; // signal that we changed somthing
		}
	}
	
#endif

	return retval;
}

void
SetExecutable()
{
	bool	transfer_it = true;
	bool	ignore_it = false;
	char	*ename = NULL;
	char	*copySpool = NULL;
	char	*macro_value = NULL;
	MyString	full_ename;
	MyString buffer;

	// In vm universe and ec2/boinc grid jobs, 'Executable'
	// parameter is not a real file but just the name of job.
	if ( JobUniverse == CONDOR_UNIVERSE_VM ||
		 ( JobUniverse == CONDOR_UNIVERSE_GRID &&
		   JobGridType != NULL &&
		   ( strcasecmp( JobGridType, "ec2" ) == MATCH ||
			 strcasecmp( JobGridType, "gce" ) == MATCH ||
			 strcasecmp( JobGridType, "boinc" ) == MATCH ) ) ) {
		ignore_it = true;
	}

	if (IsDockerJob) {
		char * docker_image = condor_param(DockerImage, ATTR_DOCKER_IMAGE);
		if ( ! docker_image) {
			fprintf(stderr, "\nERROR: docker jobs require a docker_image\n");
			DoCleanup(0,0,NULL);
			exit(1);
		}
		char * image = check_docker_image(docker_image);
		if ( ! image || ! image[0]) {
			fprintf(stderr, "\nERROR: '%s' is not a valid docker_image\n", docker_image);
			DoCleanup(0,0,NULL);
			exit(1);
		}
		buffer.formatstr("%s = \"%s\"", ATTR_DOCKER_IMAGE, image);
		InsertJobExpr(buffer);
		free(docker_image);
	}

	ename = condor_param( Executable, ATTR_JOB_CMD );
	if( ename == NULL ) {
		if (IsDockerJob) {
			// docker jobs don't require an executable.
			ignore_it = true;
		} else {
			fprintf( stderr, "No '%s' parameter was provided\n", Executable);
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
	}

	macro_value = condor_param( TransferExecutable, ATTR_TRANSFER_EXECUTABLE );
	if ( macro_value ) {
		if ( macro_value[0] == 'F' || macro_value[0] == 'f' ) {
			buffer.formatstr( "%s = FALSE", ATTR_TRANSFER_EXECUTABLE );
			InsertJobExpr( buffer );
			transfer_it = false;
		}
		free( macro_value );
	}

	if ( ignore_it ) {
		if( transfer_it == true ) {
			buffer.formatstr( "%s = FALSE", ATTR_TRANSFER_EXECUTABLE );
			InsertJobExpr( buffer );
			transfer_it = false;
		}
	}

	// For Docker Universe, if xfer_exe not set at all, and we have an exe
	// heuristically set xfer_exe to false if is a absolute path
	if (IsDockerJob && (macro_value == NULL) && !ignore_it) {
		if (ename[0] == '/') {
			buffer.formatstr( "%s = FALSE", ATTR_TRANSFER_EXECUTABLE );
			InsertJobExpr( buffer );
			transfer_it = false;
		}
	}

	// If we're not transfering the executable, leave a relative pathname
	// unresolved. This is mainly important for the Globus universe.
	if ( transfer_it ) {
		full_ename = full_path( ename, false );
	} else {
		full_ename = ename;
	}
	if ( !ignore_it ) {
		check_and_universalize_path(full_ename);
	}

	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_CMD, full_ename.Value());
	InsertJobExpr (buffer);

		/* MPI REALLY doesn't like these! */
	if ( JobUniverse != CONDOR_UNIVERSE_MPI ) {
		InsertJobExpr ("MinHosts = 1");
		InsertJobExpr ("MaxHosts = 1");
	} 

	if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
		InsertJobExpr ("WantIOProxy = TRUE");
		buffer.formatstr("%s = TRUE", ATTR_JOB_REQUIRES_SANDBOX);
		InsertJobExpr (buffer);
	}

	InsertJobExpr ("CurrentHosts = 0");

	switch(JobUniverse) 
	{
	case CONDOR_UNIVERSE_STANDARD:
		buffer.formatstr( "%s = TRUE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		buffer.formatstr( "%s = TRUE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	case CONDOR_UNIVERSE_MPI:  // for now
		if(!use_condor_mpi_universe) {
			fprintf(stderr, "\nERROR: mpi universe no longer suppported. Please use parallel universe.\n"
					"You can submit mpi jobs using parallel universe. Most likely, a substitution of\n"
					"\nuniverse = parallel\n\n"
					"in place of\n"
					"\nuniverse = mpi\n\n"
					"in you submit description file will suffice.\n"
					"See the HTCondor Manual Parallel Applications section (2.9) for further details.\n");
			DoCleanup(0,0,NULL);
			exit( 1 );
		} //Purposely fall through if use_condor_mpi_universe is true
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_GRID:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_VM:
		buffer.formatstr( "%s = FALSE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		buffer.formatstr( "%s = FALSE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	default:
		fprintf(stderr, "\nERROR: Unknown universe %d (%s)\n", JobUniverse, CondorUniverseName(JobUniverse) );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	// In vm universe and ec2, there is no executable file.
	if ( ignore_it ) {
		copySpool = (char *)strdup("FALSE");
	}else {
		copySpool = condor_param( CopyToSpool, "CopyToSpool" );
		if( copySpool == NULL ) {
			if ( JobUniverse == CONDOR_UNIVERSE_STANDARD ) {
					// Standard universe jobs can't restore from a checkpoint
					// if the executable changes.  Therefore, it is deemed
					// too risky to have copy_to_spool=false by default
					// for standard universe.
				copySpool = (char *)strdup("TRUE");
			}
			else {
					// In so many cases, copy_to_spool=true would just add
					// needless overhead.  Therefore, (as of 6.9.3), the
					// default is false.
				copySpool = (char *)strdup("FALSE");
			}
		}
	}

	// generate initial checkpoint file
	// This is ignored by the schedd in 7.5.5+.  Prior to that, the
	// basename must match the name computed by the schedd.
	IckptName = gen_ckpt_name(0,ClusterId,ICKPT,0);

	// ensure the executables exist and spool them only if no 
	// $$(arch).$$(opsys) are specified  (note that if we are simply
	// dumping the class-ad to a file, we won't actually transfer
	// or do anything [nothing that follows will affect the ad])
	if ( transfer_it && !DumpClassAdToFile && !strstr(ename,"$$") ) {

		StatInfo si(ename);
		if ( SINoFile == si.Error () ) {
		  fprintf ( stderr, 
			    "\nERROR: Executable file %s does not exist\n",
			    ename );
		  DoCleanup ( 0, 0, NULL );
		  exit ( 1 );
		}
			    
		if (!si.Error() && (si.GetFileSize() == 0)) {
			fprintf( stderr,
					 "\nERROR: Executable file %s has zero length\n", 
					 ename );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		// spool executable if necessary
		if ( isTrue ( copySpool ) ) {

			bool try_ickpt_sharing = false;
			CondorVersionInfo cvi(MySchedd->version());
			if (cvi.built_since_version(7, 3, 0)) {
				try_ickpt_sharing = param_boolean("SHARE_SPOOLED_EXECUTABLES",
				                                  true);
			}

			MyString md5;
			if (try_ickpt_sharing) {
				Condor_MD_MAC cmm;
				unsigned char* md5_raw;
				if (!cmm.addMDFile(ename)) {
					dprintf(D_ALWAYS,
					        "SHARE_SPOOLED_EXECUTABLES will not be used: "
					            "MD5 of file %s failed\n",
					        ename);
				}
				else if ((md5_raw = cmm.computeMD()) == NULL) {
					dprintf(D_ALWAYS,
					        "SHARE_SPOOLED_EXECUTABLES will not be used: "
					            "no MD5 support in this Condor build\n");
				}
				else {
					for (int i = 0; i < MAC_SIZE; i++) {
						md5.formatstr_cat("%02x", static_cast<int>(md5_raw[i]));
					}
					free(md5_raw);
				}
			}
			int ret;
			if (!md5.IsEmpty()) {
				ClassAd tmp_ad;
				tmp_ad.Assign(ATTR_OWNER, owner);
				tmp_ad.Assign(ATTR_JOB_CMD_MD5, md5.Value());
				ret = MyQ->send_SpoolFileIfNeeded(tmp_ad);
			}
			else {
				ret = MyQ->send_SpoolFile(IckptName);
			}

			if (ret < 0) {
				fprintf( stderr,
				         "\nERROR: Request to transfer executable %s failed\n",
				         IckptName );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}

			free(IckptName); IckptName = NULL;

			// ret will be 0 if the SchedD gave us the go-ahead to send
			// the file. if it's not, the SchedD is using ickpt sharing
			// and already has a copy, so no need
			//
			if ((ret == 0) && MyQ->send_SpoolFileBytes(full_path(ename,false)) < 0) {
				fprintf( stderr,
						 "\nERROR: failed to transfer executable file %s\n", 
						 ename );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}
		}

	}

	if (ename) free(ename);
	free(copySpool);
	free( IckptName );
}

void
SetDescription()
{

	char* description = condor_param( Description, ATTR_JOB_DESCRIPTION );

	if ( description ){
		InsertJobExprString(ATTR_JOB_DESCRIPTION, description);
		free(description);
	}
	else if ( InteractiveJob ){
		InsertJobExprString(ATTR_JOB_DESCRIPTION, "interactive job");
	}

	MyString batch_name = condor_param_mystring(BatchName, ATTR_JOB_BATCH_NAME);
	if ( ! batch_name.empty()) {
		batch_name.trim_quotes("\"'"); // in case they supplied a quoted string, trim the quotes
		InsertJobExprString(ATTR_JOB_BATCH_NAME, batch_name.c_str());
	}
}

#ifdef WIN32
#define CLIPPED 1
#endif

void
SetUniverse()
{
	char	*univ;
	MyString buffer;

	univ = condor_param( Universe, ATTR_JOB_UNIVERSE );

	if (!univ) {
		// get a default universe from the config file
		univ = param("DEFAULT_UNIVERSE");
		if( !univ ) {
			// if nothing else, it must be a vanilla universe
			//  *changed from "standard" for 7.2.0*
			univ = strdup("vanilla");
		}
	}

	if( univ && strcasecmp(univ,"scheduler") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_SCHEDULER;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_SCHEDULER);
		InsertJobExpr (buffer);
		free(univ);
		return;
	};

	if( univ && strcasecmp(univ,"local") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_LOCAL;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE,
						CONDOR_UNIVERSE_LOCAL );
		InsertJobExpr( buffer );
		free( univ );
		return;
	};

	if( univ && 
		((strcasecmp(univ,"globus") == MATCH) || (strcasecmp(univ,"grid") == MATCH))) {
		JobUniverse = CONDOR_UNIVERSE_GRID;
		
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID);
		InsertJobExpr (buffer);
		free(univ);
		univ = 0;
	
		// Set JobGridType
		// Check grid_resource. If it starts
		// with '$$(', then we're matchmaking and don't know the grid-type.
		if ( JobGridType != NULL ) {
			free( JobGridType );
			JobGridType = NULL;
		}
		JobGridType = condor_param( GridResource, ATTR_GRID_RESOURCE );
		if ( JobGridType ) {
			if ( strncmp( JobGridType, "$$(", 3 ) == MATCH ) {
				free( JobGridType );
				JobGridType = NULL;
			} else {
				char *space = strchr( JobGridType, ' ' );
				if ( space ) {
					*space = '\0';
				}
			}
		} else {
			fprintf( stderr, "ERROR: %s attribute not defined for grid "
					 "universe job\n", GridResource );
			exit( 1 );
		}
		if ( JobGridType ) {
			// Validate
			// Valid values are (as of 7.5.1): nordugrid, globus,
			//    gt2, gt5, blah, pbs, lsf, nqs, naregi, condor,
			//    unicore, cream, ec2, sge

			// CRUFT: grid-type 'blah' is deprecated. Now, the specific batch
			//   system names should be used (pbs, lsf). Glite are the only
			//   people who care about the old value. This changed happend in
			//   Condor 6.7.12.
			if ((strcasecmp (JobGridType, "gt2") == MATCH) ||
				(strcasecmp (JobGridType, "gt5") == MATCH) ||
				(strcasecmp (JobGridType, "blah") == MATCH) ||
				(strcasecmp (JobGridType, "batch") == MATCH) ||
				(strcasecmp (JobGridType, "pbs") == MATCH) ||
				(strcasecmp (JobGridType, "sge") == MATCH) ||
				(strcasecmp (JobGridType, "lsf") == MATCH) ||
				(strcasecmp (JobGridType, "nqs") == MATCH) ||
				(strcasecmp (JobGridType, "naregi") == MATCH) ||
				(strcasecmp (JobGridType, "condor") == MATCH) ||
				(strcasecmp (JobGridType, "nordugrid") == MATCH) ||
				(strcasecmp (JobGridType, "ec2") == MATCH) ||
				(strcasecmp (JobGridType, "gce") == MATCH) ||
				(strcasecmp (JobGridType, "unicore") == MATCH) ||
				(strcasecmp (JobGridType, "boinc") == MATCH) ||
				(strcasecmp (JobGridType, "cream") == MATCH)){
				// We're ok	
				// Values are case-insensitive for gridmanager, so we don't need to change case			
			} else if ( strcasecmp( JobGridType, "globus" ) == MATCH ) {
				// Convert 'globus' to 'gt2'
				free( JobGridType );
				JobGridType = strdup( "gt2" );
			} else {

				fprintf( stderr, "\nERROR: Invalid value '%s' for grid type\n", JobGridType );
				fprintf( stderr, "Must be one of: gt2, gt5, pbs, lsf, "
						 "sge, nqs, condor, nordugrid, unicore, ec2, gce, cream, or boinc\n" );
				exit( 1 );
			}
		}			
		
		return;
	};

	if( univ && strcasecmp(univ,"parallel") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_PARALLEL;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_PARALLEL);
		InsertJobExpr (buffer);
		
		free(univ);
		return;
	}

	if( univ && (MATCH == strcasecmp(univ,"vanilla") || MATCH == strcasecmp(univ,"docker"))) {
		JobUniverse = CONDOR_UNIVERSE_VANILLA;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA);
		InsertJobExpr (buffer);
		if (MATCH == strcasecmp(univ,"docker")) {
			// TODO: remove this when the docker starter no longer requires it.
			InsertJobExpr("WantDocker=true");
			IsDockerJob = true;
			if (dash_interactive) {
				fprintf(stderr, "\nERROR: Cannot run interactive docker jobs\n");
				exit(1);
			}
		}
		free(univ);
		return;
	};

	if( univ && strcasecmp(univ,"mpi") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_MPI;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_MPI);
		InsertJobExpr (buffer);
		
		free(univ);
		return;
	}

	if( univ && strcasecmp(univ,"java") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_JAVA;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_JAVA);
		InsertJobExpr (buffer);
		free(univ);
		return;
	}

	if( univ && strcasecmp(univ,"vm") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_VM;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VM);
		InsertJobExpr (buffer);
		free(univ);

		// virtual machine type ( xen, vmware )
		char *vm_tmp = NULL;
		vm_tmp = condor_param(VM_Type, ATTR_JOB_VM_TYPE);
		if( !vm_tmp ) {
			fprintf( stderr, "\nERROR: '%s' cannot be found.\n"
					"Please specify '%s' for vm universe "
					"in your submit description file.\n", VM_Type, VM_Type);
			DoCleanup(0,0,NULL);
			exit(1);
		}
		VMType = vm_tmp;
		VMType.lower_case();
		free(vm_tmp);

		// need vm checkpoint?
		VMCheckpoint = false;
		vm_tmp = condor_param(VM_Checkpoint, ATTR_JOB_VM_CHECKPOINT);
		if( vm_tmp ) {
			parse_vm_option(vm_tmp, VMCheckpoint);
			free(vm_tmp);
		}
	
		// need vm networking?
		VMNetworking = false;
		vm_tmp = condor_param(VM_Networking, ATTR_JOB_VM_NETWORKING);
		if( vm_tmp ) {
			parse_vm_option(vm_tmp, VMNetworking);
			free(vm_tmp);
		}
		
		// vnc set for vm?
		VMVNC = false;
		vm_tmp = condor_param(VM_VNC, ATTR_JOB_VM_VNC);
		if( vm_tmp ) {
			parse_vm_option(vm_tmp, VMVNC);
			free(vm_tmp);
		}

		if( VMCheckpoint ) {
			if( VMNetworking ) {
				/*
				 * User explicitly requested vm_checkpoint = true, 
				 * but they also turned on vm_networking, 
				 * For now, vm_networking is not conflict with vm_checkpoint.
				 * If user still wants to use both vm_networking 
				 * and vm_checkpoint, they explicitly need to define 
				 * when_to_transfer_output = ON_EXIT_OR_EVICT.
				 */
				FileTransferOutput_t when_output = FTO_NONE;
				vm_tmp = condor_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, 
						"when_to_transfer_output" );
				if( vm_tmp ) {
					when_output = getFileTransferOutputNum(vm_tmp);
					free(vm_tmp);
				}
				if( when_output != FTO_ON_EXIT_OR_EVICT ) {
					MyString err_msg;
					err_msg = "\nERROR: You explicitly requested "
						"both VM checkpoint and VM networking. "
						"However, VM networking is currently conflict "
						"with VM checkpoint. If you still want to use "
						"both VM networking and VM checkpoint, "
						"you explicitly must define "
						"\"when_to_transfer_output = ON_EXIT_OR_EVICT\"\n";
					print_wrapped_text( err_msg.Value(), stderr );
					DoCleanup(0,0,NULL);
					exit( 1 );
				}
			}
			// For vm checkpoint, we turn on condor file transfer
			set_condor_param( ATTR_SHOULD_TRANSFER_FILES, "YES");
			set_condor_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, "ON_EXIT_OR_EVICT");
		}else {
			// Even if we don't use vm_checkpoint, 
			// we always turn on condor file transfer for VM universe.
			// Here there are several reasons.
			// For example, because we may use snapshot disks in vmware, 
			// those snapshot disks need to be transferred back 
			// to this submit machine.
			// For another example, a job user want to transfer vm_cdrom_files 
			// but doesn't want to transfer disk files.
			// If we need the same file system domain, 
			// we will add the requirement of file system domain later as well.
			set_condor_param( ATTR_SHOULD_TRANSFER_FILES, "YES");
			set_condor_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, "ON_EXIT");
		}

		return;
	};


	if( univ && strcasecmp(univ,"standard") == MATCH ) {
#if defined( CLIPPED )
		fprintf( stderr, 
				 "\nERROR: You are trying to submit a \"%s\" job to Condor. "
				 "However, this installation of Condor does not support the "
				 "Standard Universe.\n%s\n%s\n",
				 univ, CondorVersion(), CondorPlatform() );
		DoCleanup(0,0,NULL);
		exit( 1 );
#else
		JobUniverse = CONDOR_UNIVERSE_STANDARD;
		buffer.formatstr( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_STANDARD);
		InsertJobExpr (buffer);
		free(univ);
		return;
#endif
	};

	fprintf( stderr, "\nERROR: I don't know about the '%s' universe.\n",univ);
	DoCleanup(0,0,NULL);
	exit( 1 );

	return;
}

void
SetMachineCount()
{
	char	*mach_count;
	MyString buffer;
	int		request_cpus = 0;

	char *wantParallelString = NULL;
	bool wantParallel = false;
	wantParallelString = condor_param("WantParallelScheduling");
	if (wantParallelString && (wantParallelString[0] == 'T' || wantParallelString[0] == 't')) {
		wantParallel = true;
		buffer.formatstr(" WantParallelScheduling = true");
		InsertJobExpr (buffer);
	}
 
	if (JobUniverse == CONDOR_UNIVERSE_MPI ||
		JobUniverse == CONDOR_UNIVERSE_PARALLEL || wantParallel) {

		mach_count = condor_param( MachineCount, "MachineCount" );
		if( ! mach_count ) { 
				// try an alternate name
			mach_count = condor_param( "node_count", "NodeCount" );
		}
		int tmp;
		if ( mach_count != NULL ) {
			tmp = atoi(mach_count);
			free(mach_count);
		}
		else {
			fprintf(stderr, "\nERROR: No machine_count specified!\n" );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		buffer.formatstr( "%s = %d", ATTR_MIN_HOSTS, tmp);
		InsertJobExpr (buffer);
		buffer.formatstr( "%s = %d", ATTR_MAX_HOSTS, tmp);
		InsertJobExpr (buffer);

		request_cpus = 1;
		RequestCpusIsZeroOrOne = true;
	} else {
		mach_count = condor_param( MachineCount, "MachineCount" );
		if( mach_count ) { 
			int tmp = atoi(mach_count);
			free(mach_count);

			if( tmp < 1 ) {
				fprintf(stderr, "\nERROR: machine_count must be >= 1\n" );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}

			buffer.formatstr( "%s = %d", ATTR_MACHINE_COUNT, tmp);
			InsertJobExpr (buffer);

			request_cpus = tmp;
			RequestCpusIsZeroOrOne = (request_cpus == 0 || request_cpus == 1);
		}
	}

	if ((mach_count = condor_param(RequestCpus, ATTR_REQUEST_CPUS))) {
		if (MATCH == strcasecmp(mach_count, "undefined")) {
			RequestCpusIsZeroOrOne = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_CPUS, mach_count);
			InsertJobExpr(buffer);
			RequestCpusIsZeroOrOne = ((MATCH == strcmp(mach_count, "0")) || (MATCH == strcmp(mach_count, "1")));
		}
		free(mach_count);
	} else 
	if (request_cpus > 0) {
		buffer.formatstr("%s = %d", ATTR_REQUEST_CPUS, request_cpus);
		InsertJobExpr(buffer);
	} else 
	if ((mach_count = param("JOB_DEFAULT_REQUESTCPUS"))) {
		if (MATCH == strcasecmp(mach_count, "undefined")) {
			RequestCpusIsZeroOrOne = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_CPUS, mach_count);
			InsertJobExpr(buffer);
			RequestCpusIsZeroOrOne = ((MATCH == strcmp(mach_count, "0")) || (MATCH == strcmp(mach_count, "1")));
		}
		free(mach_count);
	}

}

struct SimpleExprInfo {
	char const *ad_attr_name;
	char const *name1;
	char const *name2;
	char const *default_value;
	bool quote_it;
};

void
SetJobDisableFileChecks()
{
	JobDisableFileChecks = 0;
	char *dis_check = condor_param("skip_filechecks");
	if ( dis_check ) {
		if (dis_check[0]=='T' || dis_check[0]=='t') {
			JobDisableFileChecks = 1;
		}
		free(dis_check);
	}
}

/* This function is used to handle submit file commands that are inserted
 * into the job ClassAd verbatim, with no special treatment.
 */
void
SetSimpleJobExprs()
{
	SimpleExprInfo simple_exprs[] = {
		{ATTR_NEXT_JOB_START_DELAY, next_job_start_delay, next_job_start_delay2, NULL, false},
		{ATTR_JOB_KEEP_CLAIM_IDLE, "KeepClaimIdle", "keep_claim_idle", NULL, false},
		{ATTR_JOB_AD_INFORMATION_ATTRS, "JobAdInformationAttrs", "job_ad_information_attrs", NULL, true},
		{NULL,NULL,NULL,NULL,false}
	};

	SimpleExprInfo *i = simple_exprs;
	for( i=simple_exprs; i->name1; i++) {
		char *expr;
		expr = condor_param( i->name1, i->name2 );
		if( !expr ) {
			if( !i->default_value ) {
				continue;
			}
			expr = strdup( i->default_value );
			ASSERT( expr );
		}

		MyString buffer;
		if( i->quote_it ) {
			std::string expr_buf;
			QuoteAdStringValue( expr, expr_buf );
			buffer.formatstr( "%s = %s", i->ad_attr_name, expr_buf.c_str());
		}
		else {
			buffer.formatstr( "%s = %s", i->ad_attr_name, expr);
		}

		InsertJobExpr (buffer);

		free( expr );
	}
}

// Note: you must call SetTransferFiles() *before* calling SetImageSize().
void
SetImageSize()
{
	static bool    got_exe_size = false;
	static int64_t executable_size_kb = 0;
	char	*tmp;
	char    buff[2048];
	MyString buffer;

	// we should only call calc_image_size_kb on the first
	// proc in the cluster, since the executable cannot change.
	if ( ProcId < 1 || ! got_exe_size ) {
		ASSERT (job->LookupString (ATTR_JOB_CMD, buff, sizeof(buff)));
		if( JobUniverse == CONDOR_UNIVERSE_VM ) { 
			executable_size_kb = 0;
		}else {
			executable_size_kb = calc_image_size_kb( buff );
		}
		got_exe_size = true;
	}

	int64_t image_size_kb = executable_size_kb;

	// if the user specifies an initial image size, use that instead 
	// of the calculated 
	tmp = condor_param( ImageSize, ATTR_IMAGE_SIZE );
	if( tmp ) {
#if 1
		if ( ! parse_int64_bytes(tmp, image_size_kb, 1024)) {
			fprintf(stderr, "\nERROR: '%s' is not valid for Image Size\n", tmp);
			image_size_kb = 0;
		}
#else
		char	*p;
		image_size_kb = strtol(tmp, &p, 10);
		//for( p=tmp; *p && isdigit(*p); p++ )
		//	;
		while( isspace(*p) ) p++;
		if( *p == 'm' || *p == 'M' ) {
			image_size_kb *=  1024;
		}
#endif
		free( tmp );
		if( image_size_kb < 1 ) {
			fprintf(stderr, "\nERROR: Image Size must be positive\n" );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
	}

	/* It's reasonable to expect the image size will be at least the
		physical memory requirement, so make sure it is. */
	  
	// At one time there was some stuff to attempt to gleen this from
	// the requirements line, but that caused many problems.
	// Jeff Ballard 11/4/98

	buffer.formatstr( "%s = %" PRId64, ATTR_IMAGE_SIZE, image_size_kb);
	InsertJobExpr (buffer);

	buffer.formatstr( "%s = %" PRId64, ATTR_EXECUTABLE_SIZE, executable_size_kb);
	InsertJobExpr (buffer);

	// set an initial value for memory usage
	//
	tmp = condor_param( MemoryUsage, ATTR_MEMORY_USAGE );
	if (tmp) {
		int64_t memory_usage_mb = 0;
		if ( ! parse_int64_bytes(tmp, memory_usage_mb, 1024*1024) ||
			memory_usage_mb < 0) {
			fprintf(stderr, "\nERROR: '%s' is not valid for Memory Usage\n", tmp);
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		free(tmp);
		buffer.formatstr( "%s = %" PRId64, ATTR_MEMORY_USAGE, memory_usage_mb);
		InsertJobExpr (buffer);
	}

	// set an initial value for disk usage based on the size of the input sandbox.
	//
	int64_t disk_usage_kb = 0;
	tmp = condor_param( DiskUsage, ATTR_DISK_USAGE );
	if( tmp ) {
		if ( ! parse_int64_bytes(tmp, disk_usage_kb, 1024) || disk_usage_kb < 1) {
			fprintf( stderr, "\nERROR: '%s' is not valid for disk_usage. It must be >= 1\n", tmp);
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		free( tmp );
	} else {
		// In vm universe, when a VM is suspended, 
		// memory being used by the VM will be saved into a file. 
		// So we need as much disk space as the memory.
		// For non-vm jobs, VMMemoryMb is 0.
		disk_usage_kb = executable_size_kb + TransferInputSizeKb + (int64_t)VMMemoryMb*1024;
	}
	buffer.formatstr( "%s = %" PRId64, ATTR_DISK_USAGE, disk_usage_kb );
	InsertJobExpr (buffer);

	InsertJobExprInt(ATTR_TRANSFER_INPUT_SIZE_MB, (executable_size_kb + TransferInputSizeKb)/1024);

	// set an intial value for RequestMemory
	tmp = condor_param(RequestMemory, ATTR_REQUEST_MEMORY);
	if (tmp) {
		// if input is an integer followed by K,M,G or T, scale it MB and 
		// insert it into the jobAd, otherwise assume it is an expression
		// and insert it as text into the jobAd.
		int64_t req_memory_mb = 0;
		if (parse_int64_bytes(tmp, req_memory_mb, 1024*1024)) {
			buffer.formatstr("%s = %" PRId64, ATTR_REQUEST_MEMORY, req_memory_mb);
			RequestMemoryIsZero = (req_memory_mb == 0);
		} else if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestMemoryIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_MEMORY, tmp);
		}
		free(tmp);
		InsertJobExpr(buffer);
	} else if ( (tmp = condor_param(VM_Memory)) || (tmp = condor_param( ATTR_JOB_VM_MEMORY )) ) {
		fprintf(stderr, "\nNOTE: '%s' was NOT specified.  Using %s = %s. \n", ATTR_REQUEST_MEMORY,ATTR_JOB_VM_MEMORY, tmp );
		buffer.formatstr("%s = MY.%s", ATTR_REQUEST_MEMORY, ATTR_JOB_VM_MEMORY);
		free(tmp);
		InsertJobExpr(buffer);
	} else if ( (tmp = param("JOB_DEFAULT_REQUESTMEMORY")) ) {
		if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestMemoryIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_MEMORY, tmp);
			RequestMemoryIsZero = (MATCH == strcmp(tmp, "0"));
			InsertJobExpr(buffer);
		}
		free(tmp);
	}

	// set an initial value for RequestDisk
	if ((tmp = condor_param(RequestDisk, ATTR_REQUEST_DISK))) {
		// if input is an integer followed by K,M,G or T, scale it MB and 
		// insert it into the jobAd, otherwise assume it is an expression
		// and insert it as text into the jobAd.
		int64_t req_disk_kb = 0;
		if (parse_int64_bytes(tmp, req_disk_kb, 1024)) {
			buffer.formatstr("%s = %" PRId64, ATTR_REQUEST_DISK, req_disk_kb);
			RequestDiskIsZero = (req_disk_kb == 0);
		} else if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestDiskIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_DISK, tmp);
		}
		free(tmp);
		InsertJobExpr(buffer);
	} else if ( (tmp = param("JOB_DEFAULT_REQUESTDISK")) ) {
		if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestDiskIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_DISK, tmp);
			RequestDiskIsZero = (MATCH == strcmp(tmp, "0"));
			InsertJobExpr(buffer);
		}
		free(tmp);
	}
	
}

void SetFileOptions()
{
	char *tmp;
	MyString strbuffer;

	tmp = condor_param( FileRemaps, ATTR_FILE_REMAPS );
	if(tmp) {
		strbuffer.formatstr("%s = %s",ATTR_FILE_REMAPS,tmp);
		InsertJobExpr(strbuffer);
		free(tmp);
	}

	tmp = condor_param( BufferFiles, ATTR_BUFFER_FILES );
	if(tmp) {
		strbuffer.formatstr("%s = %s",ATTR_BUFFER_FILES,tmp);
		InsertJobExpr(strbuffer);
		free(tmp);
	}

	/* If no buffer size is given, use 512 KB */

	tmp = condor_param( BufferSize, ATTR_BUFFER_SIZE );
	if(!tmp) {
		tmp = param("DEFAULT_IO_BUFFER_SIZE");
		if (!tmp) {
			tmp = strdup("524288");
		}
	}
	strbuffer.formatstr("%s = %s",ATTR_BUFFER_SIZE,tmp);
	InsertJobExpr(strbuffer);
	free(tmp);

	/* If not buffer block size is given, use 32 KB */

	tmp = condor_param( BufferBlockSize, ATTR_BUFFER_BLOCK_SIZE );
	if(!tmp) {
		tmp = param("DEFAULT_IO_BUFFER_BLOCK_SIZE");
		if (!tmp) {
			tmp = strdup("32768");
		}
	}
	strbuffer.formatstr("%s = %s",ATTR_BUFFER_BLOCK_SIZE,tmp);
	InsertJobExpr(strbuffer.Value());
	free(tmp);
}


void SetRequestResources() {
    HASHITER it = hash_iter_begin(SubmitMacroSet);
    for (;  !hash_iter_done(it);  hash_iter_next(it)) {
        const char * key = hash_iter_key(it);
        // if key is not of form "request_xxx", ignore it:
        if ( ! starts_with_ignore_case(key, RequestPrefix)) continue;
        // if key is one of the predefined request_cpus, request_memory, etc, also ignore it,
        // those have their own special handling:
        if (fixedReqRes.count(key) > 0) continue;
        const char * rname = key + strlen(RequestPrefix);
        // resource name should be nonempty
        if ( ! *rname) continue;
        // could get this from 'it', but this prevents unused-line warnings:
        char * val = condor_param(key);
        std::string assign;
        formatstr(assign, "%s%s = %s", ATTR_REQUEST_PREFIX, rname, val);
        
        if (val[0]=='\"')
        {
            stringReqRes.insert(rname);
        }
        
        InsertJobExpr(assign.c_str()); 
    }
    hash_iter_delete(&it);
}



/*
** Make a wild guess at the size of the image represented by this a.out.
** Should add up the text, data, and bss sizes, then allow something for
** the stack.  But how we gonna do that if the executable is for some
** other architecture??  Our answer is in kilobytes.
*/
int64_t
calc_image_size_kb( const char *name)
{
	struct stat	buf;

	if ( IsUrl( name ) ) {
		return 0;
	}

	if( stat(full_path(name),&buf) < 0 ) {
		// EXCEPT( "Cannot stat \"%s\"", name );
		return 0;
	}
	if( buf.st_mode & S_IFDIR ) {
		Directory dir(full_path(name));
		return ((int64_t)dir.GetDirectorySize() + 1023) / 1024;
	}
	return ((int64_t)buf.st_size + 1023) / 1024;
}

void
process_input_file_list(StringList * input_list, MyString *input_files, bool * files_specified)
{
	int count;
	MyString tmp;
	char* tmp_ptr;

	if( ! input_list->isEmpty() ) {
		input_list->rewind();
		count = 0;
		while ( (tmp_ptr=input_list->next()) ) {
			count++;
			tmp = tmp_ptr;
			if ( check_and_universalize_path(tmp) != 0) {
				// path was universalized, so update the string list
				input_list->deleteCurrent();
				input_list->insert(tmp.Value());
			}
			check_open(tmp.Value(), O_RDONLY);
			TransferInputSizeKb += calc_image_size_kb(tmp.Value());
		}
		if ( count ) {
			tmp_ptr = input_list->print_to_string();
			input_files->formatstr( "%s = \"%s\"",
				ATTR_TRANSFER_INPUT_FILES, tmp_ptr );
			free( tmp_ptr );
			*files_specified = true;
		}
	}
}

// Note: SetTransferFiles() sets a global variable TransferInputSizeKb which
// is the size of all the transferred input files.  This variable is used
// by SetImageSize().  So, SetTransferFiles() must be called _before_ calling
// SetImageSize().
// SetTransferFiles also sets a global "should_transfer", which is 
// used by SetRequirements().  So, SetTransferFiles must be called _before_
// calling SetRequirements() as well.
// If we are transfering files, and stdout or stderr contains
// path information, SetTransferFiles renames the output file to a plain
// file (and stores the original) in the ClassAd, so SetStdFile() should
// be called before getting here too.
void
SetTransferFiles()
{
	char *macro_value;
	int count;
	MyString tmp;
	char* tmp_ptr;
	bool in_files_specified = false;
	bool out_files_specified = false;
	MyString	 input_files;
	MyString	 output_files;
	StringList input_file_list(NULL, ",");
	StringList output_file_list(NULL,",");
	MyString output_remaps;

	macro_value = condor_param( TransferInputFiles, "TransferInputFiles" ) ;
	TransferInputSizeKb = 0;
	if( macro_value ) {
		// as a special case transferinputfiles="" will produce an empty list of input files, not a syntax error
		// PRAGMA_REMIND("replace this special case with code that correctly parses any input wrapped in double quotes")
		if (macro_value[0] == '"' && macro_value[1] == '"' && macro_value[2] == 0) {
			input_file_list.clearAll();
		} else {
			input_file_list.initializeFromString( macro_value );
		}
	}


#if defined( WIN32 )
	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
			// On NT, if we're an MPI job, we need to find the
			// mpich.dll file and automatically include that in the
			// transfer input files
		MyString dll_name( "mpich.dll" );

			// first, check to make sure the user didn't already
			// specify mpich.dll in transfer_input_files
		if( ! input_file_list.contains(dll_name.Value()) ) {
				// nothing there yet, try to find it ourselves
			MyString dll_path = which( dll_name );
			if( dll_path.Length() == 0 ) {
					// File not found, fatal error.
				fprintf( stderr, "\nERROR: Condor cannot find the "
						 "\"mpich.dll\" file it needs to run your MPI job.\n"
						 "Please specify the full path to this file in the "
						 "\"transfer_input_files\"\n"
						 "setting in your submit description file.\n" );
				exit( 1 );
			}
				// If we made it here, which() gave us a real path.
				// so, now we just have to append that to our list of
				// files. 
			input_file_list.append( dll_path.Value() );
		}
	}
#endif /* WIN32 */

	if( ! input_file_list.isEmpty() ) {
		process_input_file_list(&input_file_list, &input_files, &in_files_specified);
	}

		// also account for the size of the stdin file, if any
	bool transfer_stdin = true;
	job->LookupBool(ATTR_TRANSFER_INPUT,transfer_stdin);
	if( transfer_stdin ) {
		std::string stdin_fname;
		job->LookupString(ATTR_JOB_INPUT,stdin_fname);
		if( !stdin_fname.empty() ) {
			TransferInputSizeKb += calc_image_size_kb(stdin_fname.c_str());
		}
	}

	macro_value = condor_param( TransferOutputFiles,
								"TransferOutputFiles" ); 
	if( macro_value ) 
	{
		// as a special case transferoutputfiles="" will produce an empty list of output files, not a syntax error
		// PRAGMA_REMIND("replace this special case with code that correctly parses any input wrapped in double quotes")
		if (macro_value[0] == '"' && macro_value[1] == '"' && macro_value[2] == 0) {
			output_file_list.clearAll();
			output_files = ATTR_TRANSFER_OUTPUT_FILES " = \"\"";
		} else {
			output_file_list.initializeFromString(macro_value);
		}
		output_file_list.rewind();
		count = 0;
		while ( (tmp_ptr=output_file_list.next()) ) {
			count++;
			tmp = tmp_ptr;
			if ( check_and_universalize_path(tmp) != 0) 
			{
				// we universalized the path, so update the string list
				output_file_list.deleteCurrent();
				output_file_list.insert(tmp.Value());
			}
		}
		tmp_ptr = output_file_list.print_to_string();
		if ( count ) {
			(void) output_files.formatstr ("%s = \"%s\"", 
				ATTR_TRANSFER_OUTPUT_FILES, tmp_ptr);
			free(tmp_ptr);
			tmp_ptr = NULL;
			out_files_specified = true;
		}
		free(macro_value);
	}

	//
	// START FILE TRANSFER VALIDATION
	//
		// now that we've gathered up all the possible info on files
		// the user explicitly wants to transfer, see if they set the
		// right attributes controlling if and when to do the
		// transfers.  if they didn't tell us what to do, in some
		// cases, we can give reasonable defaults, but in others, it's
		// a fatal error.
		//
		// SHOULD_TRANSFER_FILES (STF) defaults to IF_NEEDED (STF_IF_NEEDED)
		// WHEN_TO_TRANSFER_OUTPUT (WTTO) defaults to ON_EXIT (FTO_ON_EXIT)
		// 
		// Error if:
		//  (A) bad user input - getShouldTransferFilesNum fails
		//  (B) bas user input - getFileTransferOutputNum fails
		//  (C) STF is STF_NO and WTTO is not FTO_NONE
		//  (D) STF is not STF_NO and WTTO is FTO_NONE
		//  (E) STF is STF_IF_NEEDED and WTTO is FTO_ON_EXIT_OR_EVICT
		//  (F) STF is STF_NO and transfer_input_files or transfer_output_files specified
	const char *should = "INTERNAL ERROR";
	const char *when = "INTERNAL ERROR";
	bool default_should;
	bool default_when;
	FileTransferOutput_t when_output;
	MyString err_msg;
	
	should = condor_param(ATTR_SHOULD_TRANSFER_FILES, 
						  "should_transfer_files");
	if (!should) {
		should = "IF_NEEDED";
		should_transfer = STF_IF_NEEDED;
		default_should = true;
	} else {
		should_transfer = getShouldTransferFilesNum(should);
		if (should_transfer < 0) { // (A)
			err_msg = "\nERROR: invalid value (\"";
			err_msg += should;
			err_msg += "\") for ";
			err_msg += ATTR_SHOULD_TRANSFER_FILES;
			err_msg += ".  Please either specify \"YES\", \"NO\", or ";
			err_msg += "\"IF_NEEDED\" and try again.";
			print_wrapped_text(err_msg.Value(), stderr);
			DoCleanup(0, 0, NULL);
			exit(1);
		}
		default_should = false;
	}

	if (should_transfer == STF_NO &&
		(in_files_specified || out_files_specified)) { // (F)
		err_msg = "\nERROR: you specified files you want Condor to "
			"transfer via \"";
		if( in_files_specified ) {
			err_msg += "transfer_input_files";
			if( out_files_specified ) {
				err_msg += "\" and \"transfer_output_files\",";
			} else {
				err_msg += "\",";
			}
		} else {
			ASSERT( out_files_specified );
			err_msg += "transfer_output_files\",";
		}
		err_msg += " but you disabled should_transfer_files.";
		print_wrapped_text(err_msg.Value(), stderr);
		DoCleanup(0, 0, NULL);
		exit(1);
	}

	when = condor_param(ATTR_WHEN_TO_TRANSFER_OUTPUT, 
						"when_to_transfer_output");
	if (!when) {
		when = "ON_EXIT";
		when_output = FTO_ON_EXIT;
		default_when = true;
	} else {
		when_output = getFileTransferOutputNum(when);
		if (when_output < 0) { // (B)
			err_msg = "\nERROR: invalid value (\"";
			err_msg += when;
			err_msg += "\") for ";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += ".  Please either specify \"ON_EXIT\", or ";
			err_msg += "\"ON_EXIT_OR_EVICT\" and try again.";
			print_wrapped_text(err_msg.Value(), stderr);
			DoCleanup(0, 0, NULL);
			exit(1);
		}
		default_when = false;
	}

		// for backward compatibility and user convenience -
		// if the user specifies should_transfer_files = NO and has
		// not specified when_to_transfer_output, we'll change
		// when_to_transfer_output to NEVER and avoid an unhelpful
		// error message later.
	if (!default_should && default_when &&
		should_transfer == STF_NO) {
		when = "NEVER";
		when_output = FTO_NONE;
	}

	if ((should_transfer == STF_NO && when_output != FTO_NONE) || // (C)
		(should_transfer != STF_NO && when_output == FTO_NONE)) { // (D)
		err_msg = "\nERROR: ";
		err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
		err_msg += " specified as \"";
		err_msg += when;
		err_msg += "\"";
		err_msg += " yet ";
		err_msg += ATTR_SHOULD_TRANSFER_FILES;
		err_msg += " defined as \"";
		err_msg += should;
		err_msg += "\".  Please remove this contradiction from ";
		err_msg += "your submit file and try again.";
		print_wrapped_text(err_msg.Value(), stderr);
		DoCleanup(0, 0, NULL);
		exit(1);
	}

		// for backward compatibility and user convenience -
		// if the user specifies only when_to_transfer_output =
		// ON_EXIT_OR_EVICT, which is incompatible with the default
		// should_transfer_files of IF_NEEDED, we'll change
		// should_transfer_files to YES for them.
	if (default_should &&
		when_output == FTO_ON_EXIT_OR_EVICT &&
		should_transfer == STF_IF_NEEDED) {
		should = "YES";
		should_transfer = STF_YES;
	}

	if (when_output == FTO_ON_EXIT_OR_EVICT && 
		should_transfer == STF_IF_NEEDED) { // (E)
			// error, these are incompatible!
		err_msg = "\nERROR: \"when_to_transfer_output = ON_EXIT_OR_EVICT\" "
			"and \"should_transfer_files = IF_NEEDED\" are incompatible.  "
			"The behavior of these two settings together would produce "
			"incorrect file access in some cases.  Please decide which "
			"one of those two settings you're more interested in. "
			"If you really want \"IF_NEEDED\", set "
			"\"when_to_transfer_output = ON_EXIT\".  If you really want "
			"\"ON_EXIT_OR_EVICT\", please set \"should_transfer_files = "
			"YES\".  After you have corrected this incompatibility, "
			"please try running condor_submit again.\n";
		print_wrapped_text(err_msg.Value(), stderr);
		DoCleanup(0, 0, NULL);
		exit(1);
	}

	InsertFileTransAttrs(when_output);
	//
	// END FILE TRANSFER VALIDATION
	//

		/*
		  If we're dealing w/ TDP and we might be transfering files,
		  we want to make sure the tool binary and input file (if
		  any) are included in the transfer_input_files list.
		*/
	if( should_transfer!=STF_NO && HasTDP ) {
		char *file_list = NULL;
		bool changed_it = false;
		if(job->LookupString(ATTR_TRANSFER_INPUT_FILES,&file_list)!=1) {
			file_list = (char *)malloc(1);
			file_list[0] = '\0';
		}
		MyString file_list_tdp;
		file_list_tdp += file_list;
		if( tdp_cmd && (!strstr(file_list, tdp_cmd)) ) {
			TransferInputSizeKb += calc_image_size_kb(tdp_cmd);
			if(file_list[0]) {
				file_list_tdp += ",";
			}
			file_list_tdp += tdp_cmd;
			changed_it = true;
		}
		if( tdp_input && (!strstr(file_list, tdp_input)) ) {
			TransferInputSizeKb += calc_image_size_kb(tdp_input);
			if(file_list[0]) {
				file_list_tdp += ",";
				file_list_tdp += tdp_input;
			} else {
				file_list_tdp += tdp_input;
			}
			changed_it = true;
		}
		if( changed_it ) {
			InsertJobExprString(ATTR_TRANSFER_INPUT_FILES, file_list_tdp.Value() );
		}
		free(file_list);
	}


	/*
	  In the Java universe, if we might be transfering files, we want
	  to automatically transfer the "executable" (i.e. the entry
	  class) and any requested .jar files.  However, the executable is
	  put directly into TransferInputFiles with TransferExecutable set
	  to false, because the FileTransfer object happily renames
	  executables left and right, which we don't want.
	*/

	/* 
	  We need to have our best guess of file transfer needs processed before
	  we change things to deal with the jar files and class file which
	  is the executable and should not be renamed. But we can't just
	  change and set ATTR_TRANSFER_INPUT_FILES as it is done later as the saved
	  settings in input_files and output_files is dumped out undoing our
	  careful efforts. So we will append to the input_file_list and regenerate
	  input_files. bt
	*/

	if( should_transfer!=STF_NO && JobUniverse==CONDOR_UNIVERSE_JAVA ) {
		macro_value = condor_param( Executable, ATTR_JOB_CMD );

		if(macro_value) {
			MyString executable_str = macro_value;
			input_file_list.append(executable_str.Value());
			free(macro_value);
		}

		macro_value = condor_param( JarFiles, ATTR_JAR_FILES );
		if(macro_value) {
			StringList files(macro_value, ",");
			files.rewind();
			while ( (tmp_ptr=files.next()) ) {
				tmp = tmp_ptr;
				input_file_list.append(tmp.Value());
			}
			free(macro_value);
		}

		if( ! input_file_list.isEmpty() ) {
			process_input_file_list(&input_file_list, &input_files, &in_files_specified);
		}

		InsertJobExprString( ATTR_JOB_CMD, "java");

		MyString b;
		b.formatstr("%s = FALSE", ATTR_TRANSFER_EXECUTABLE);
		InsertJobExpr( b.Value() );
	}

	// If either stdout or stderr contains path information, and the
	// file is being transfered back via the FileTransfer object,
	// substitute a safe name to use in the sandbox and stash the
	// original name in the output file "remaps".  The FileTransfer
	// object will take care of transferring the data back to the
	// correct path.

	// Starting with Condor 7.7.2, we only do this remapping if we're
	// spooling files to the schedd. The shadow/starter will do any
	// required renaming in the non-spooling case.
	CondorVersionInfo cvi((MySchedd) ? MySchedd->version() : NULL);
	if ( (!cvi.built_since_version(7, 7, 2) && should_transfer != STF_NO &&
		  JobUniverse != CONDOR_UNIVERSE_GRID &&
		  JobUniverse != CONDOR_UNIVERSE_STANDARD) ||
		 Remote ) {

		MyString output;
		MyString error;

		job->LookupString(ATTR_JOB_OUTPUT,output);
		job->LookupString(ATTR_JOB_ERROR,error);

		if(output.Length() && output != condor_basename(output.Value()) && 
		   strcmp(output.Value(),"/dev/null") != 0 && !stream_stdout_toggle)
		{
			char const *working_name = StdoutRemapName;
				//Force setting value, even if we have already set it
				//in the cluster ad, because whatever was in the
				//cluster ad may have been overwritten (e.g. by a
				//filename containing $(Process)).  At this time, the
				//check in InsertJobExpr() is not smart enough to
				//notice that.
			InsertJobExprString(ATTR_JOB_OUTPUT, working_name);

			if(!output_remaps.IsEmpty()) output_remaps += ";";
			output_remaps.formatstr_cat("%s=%s",working_name,output.EscapeChars(";=\\",'\\').Value());
		}

		if(error.Length() && error != condor_basename(error.Value()) && 
		   strcmp(error.Value(),"/dev/null") != 0 && !stream_stderr_toggle)
		{
			char const *working_name = StderrRemapName;

			if(error == output) {
				//stderr will use same file as stdout
				working_name = StdoutRemapName;
			}
				//Force setting value, even if we have already set it
				//in the cluster ad, because whatever was in the
				//cluster ad may have been overwritten (e.g. by a
				//filename containing $(Process)).  At this time, the
				//check in InsertJobExpr() is not smart enough to
				//notice that.
			InsertJobExprString(ATTR_JOB_ERROR, working_name);

			if(!output_remaps.IsEmpty()) output_remaps += ";";
			output_remaps.formatstr_cat("%s=%s",working_name,error.EscapeChars(";=\\",'\\').Value());
		}
	}

	// if we might be using file transfer, insert in input/output
	// exprs  
	if( should_transfer != STF_NO ) {
		if ( input_files.Length() > 0 ) {
			InsertJobExpr (input_files);
		}
		if ( output_files.Length() > 0 ) {
			InsertJobExpr (output_files);
		}
	}

	if( should_transfer == STF_NO && 
		JobUniverse != CONDOR_UNIVERSE_GRID &&
		JobUniverse != CONDOR_UNIVERSE_JAVA &&
		JobUniverse != CONDOR_UNIVERSE_VM )
	{
		char *transfer_exe;
		transfer_exe = condor_param( TransferExecutable,
									 ATTR_TRANSFER_EXECUTABLE );
		if(transfer_exe && *transfer_exe != 'F' && *transfer_exe != 'f') {
			//User explicitly requested transfer_executable = true,
			//but they did not turn on transfer_files, so they are
			//going to be confused when the executable is _not_
			//transfered!  Better bail out.
			err_msg = "\nERROR: You explicitly requested that the "
				"executable be transfered, but for this to work, you must "
				"enable Condor's file transfer functionality.  You need "
				"to define either: \"when_to_transfer_output = ON_EXIT\" "
				"or \"when_to_transfer_output = ON_EXIT_OR_EVICT\".  "
				"Optionally, you can define \"should_transfer_files = "
				"IF_NEEDED\" if you do not want any files to be transfered "
				"if the job executes on a machine in the same "
				"FileSystemDomain.  See the Condor manual for more "
				"details.";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		free(transfer_exe);
	}

	macro_value = condor_param( TransferOutputRemaps,ATTR_TRANSFER_OUTPUT_REMAPS);
	if(macro_value) {
		if(*macro_value != '"' || macro_value[1] == '\0' || macro_value[strlen(macro_value)-1] != '"') {
			fprintf(stderr,"\nERROR: transfer_output_remaps must be a quoted string, not: %s\n",macro_value);
			exit( 1 );
		}

		macro_value[strlen(macro_value)-1] = '\0';  //get rid of terminal quote

		if(!output_remaps.IsEmpty()) output_remaps += ";";
		output_remaps += macro_value+1; // add user remaps to auto-generated ones

		free(macro_value);
	}

	if(!output_remaps.IsEmpty()) {
		MyString expr;
		expr.formatstr("%s = \"%s\"",ATTR_TRANSFER_OUTPUT_REMAPS,output_remaps.Value());
		InsertJobExpr(expr);
	}

		// Check accessibility of output files.
	output_file_list.rewind();
	char const *output_file;
	while ( (output_file=output_file_list.next()) ) {
		output_file = condor_basename(output_file);
		if( !output_file || !output_file[0] ) {
				// output_file may be empty if the entry in the list is
				// a path ending with a slash.  Since a path ending in a
				// slash means to get the contents of a directory, and we
				// don't know in advance what names will exist in the
				// directory, we can't do any check now.
			continue;
		}
		// Apply filename remaps if there are any.
		MyString remap_fname;
		if(filename_remap_find(output_remaps.Value(),output_file,remap_fname)) {
			output_file = remap_fname.Value();
		}

		check_open(output_file, O_WRONLY|O_CREAT|O_TRUNC );
	}

	char *MaxTransferInputExpr = condor_param(MaxTransferInputMB,ATTR_MAX_TRANSFER_INPUT_MB);
	char *MaxTransferOutputExpr = condor_param(MaxTransferOutputMB,ATTR_MAX_TRANSFER_OUTPUT_MB);
	if( MaxTransferInputExpr ) {
		std::string max_expr;
		formatstr(max_expr,"%s = %s",ATTR_MAX_TRANSFER_INPUT_MB,MaxTransferInputExpr);
		InsertJobExpr(max_expr.c_str());
		free( MaxTransferInputExpr );
	}
	if( MaxTransferOutputExpr ) {
		std::string max_expr;
		formatstr(max_expr,"%s = %s",ATTR_MAX_TRANSFER_OUTPUT_MB,MaxTransferOutputExpr);
		InsertJobExpr(max_expr.c_str());
		free( MaxTransferOutputExpr );
	}
}

void FixupTransferInputFiles( void )
{
		// See the comment in the function body of ExpandInputFileList
		// for an explanation of what is going on here.

	MyString error_msg;
	if( Remote && !FileTransfer::ExpandInputFileList( job, error_msg ) )
	{
		MyString err_msg;
		err_msg.formatstr( "\n%s\n",error_msg.Value());
		print_wrapped_text( err_msg.Value(), stderr );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}

void SetPerFileEncryption( void )
{
	char* encrypt_input_files;
	char* encrypt_output_files;
	char* dont_encrypt_input_files;
	char* dont_encrypt_output_files;

	encrypt_input_files = condor_param( EncryptInputFiles, "EncryptInputFiles");
	if (encrypt_input_files) {
		InsertJobExprString(ATTR_ENCRYPT_INPUT_FILES,encrypt_input_files);
		NeedsPerFileEncryption = true;
	}

	encrypt_output_files = condor_param( EncryptOutputFiles, "EncryptOutputFiles");
	if (encrypt_output_files) {
		InsertJobExprString( ATTR_ENCRYPT_OUTPUT_FILES,encrypt_output_files);
		NeedsPerFileEncryption = true;
	}

	dont_encrypt_input_files = condor_param( DontEncryptInputFiles, "DontEncryptInputFiles");
	if (dont_encrypt_input_files) {
		InsertJobExprString( ATTR_DONT_ENCRYPT_INPUT_FILES,dont_encrypt_input_files);
		NeedsPerFileEncryption = true;
	}

	dont_encrypt_output_files = condor_param( DontEncryptOutputFiles, "DontEncryptOutputFiles");
	if (dont_encrypt_output_files) {
		InsertJobExprString( ATTR_DONT_ENCRYPT_OUTPUT_FILES,dont_encrypt_output_files);
		NeedsPerFileEncryption = true;
	}
}


void
InsertFileTransAttrs( FileTransferOutput_t when_output )
{
	MyString should = ATTR_SHOULD_TRANSFER_FILES;
	should += " = \"";
	MyString when = ATTR_WHEN_TO_TRANSFER_OUTPUT;
	when += " = \"";
	
	should += getShouldTransferFilesString( should_transfer );
	should += '"';
	if( should_transfer != STF_NO ) {
		if( ! when_output ) {
			EXCEPT( "InsertFileTransAttrs() called we might transfer "
					"files but when_output hasn't been set" );
		}
		when += getFileTransferOutputString( when_output );
		when += '"';
	}
	InsertJobExpr( should.Value() );
	if( should_transfer != STF_NO ) {
		InsertJobExpr( when.Value() );
	}
}


void
SetFetchFiles()
{
	char *value;

	value = condor_param( FetchFiles, ATTR_FETCH_FILES );
	if(value) {
		InsertJobExprString(ATTR_FETCH_FILES,value);
	}
}

void
SetCompressFiles()
{
	char *value;

	value = condor_param( CompressFiles, ATTR_COMPRESS_FILES );
	if(value) {
		InsertJobExprString(ATTR_COMPRESS_FILES,value);
	}
}

void
SetAppendFiles()
{
	char *value;

	value = condor_param( AppendFiles, ATTR_APPEND_FILES );
	if(value) {
		InsertJobExprString(ATTR_APPEND_FILES,value);
	}
}

void
SetLocalFiles()
{
	char *value;

	value = condor_param( LocalFiles, ATTR_LOCAL_FILES );
	if(value) {
		InsertJobExprString(ATTR_LOCAL_FILES,value);
	}
}

void
SetJarFiles()
{
	char *value;

	value = condor_param( JarFiles, ATTR_JAR_FILES );
	if(value) {
		InsertJobExprString(ATTR_JAR_FILES,value);
	}
}

void
SetJavaVMArgs()
{
	ArgList args;
	MyString error_msg;
	MyString strbuffer;
	MyString value;
	char *args1 = condor_param(JavaVMArgs); // for backward compatibility
	char *args1_ext=condor_param(JavaVMArguments1,ATTR_JOB_JAVA_VM_ARGS1);
		// NOTE: no ATTR_JOB_JAVA_VM_ARGS2 in the following,
		// because that is the same as JavaVMArguments1.
	char *args2 = condor_param( JavaVMArguments2 );
	char *allow_arguments_v1 = condor_param(AllowArgumentsV1);

	if(args1_ext && args1) {
		fprintf(stderr,"\nERROR: you specified a value for both %s and %s.\n",
				JavaVMArgs,JavaVMArguments1);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if(args1_ext) {
		free(args1);
		args1 = args1_ext;
		args1_ext = NULL;
	}

	if(args2 && args1 && !isTrue(allow_arguments_v1)) {
		fprintf(stderr,
		 "\nERROR: If you wish to specify both 'java_vm_arguments' and\n"
		 "'java_vm_arguments2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_arguments_v1=true.\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}

	bool args_success = true;

	if(args2) {
		args_success = args.AppendArgsV2Quoted(args2,&error_msg);
	}
	else if(args1) {
		args_success = args.AppendArgsV1WackedOrV2Quoted(args1,&error_msg);
	}

	if(!args_success) {
		fprintf(stderr,"ERROR: failed to parse java VM arguments: %s\n"
				"The full arguments you specified were %s\n",
				error_msg.Value(),args2 ? args2 : args1);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	// since we don't care about what version the schedd needs if it
	// is not present, we just default to no.... this only happens
	// in the case when we are dumping to a file.
	bool MyCondorVersionRequiresV1 = false;
	if ( !DumpClassAdToFile ) {
		MyCondorVersionRequiresV1 = args.InputWasV1() || args.CondorVersionRequiresV1(MySchedd->version());
	}
	if( MyCondorVersionRequiresV1 ) {
		args_success = args.GetArgsStringV1Raw(&value,&error_msg);
		if(!value.IsEmpty()) {
			strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_JAVA_VM_ARGS1,
						   value.EscapeChars("\"",'\\').Value());
			InsertJobExpr (strbuffer);
		}
	}
	else {
		args_success = args.GetArgsStringV2Raw(&value,&error_msg);
		if(!value.IsEmpty()) {
			strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_JAVA_VM_ARGS2,
						   value.EscapeChars("\"",'\\').Value());
			InsertJobExpr (strbuffer);
		}
	}

	if(!args_success) {
		fprintf(stderr,"\nERROR: failed to insert java vm arguments into "
				"ClassAd: %s\n",error_msg.Value());
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	free(args1);
	free(args2);
	free(args1_ext);
	free(allow_arguments_v1);
}

void
SetParallelStartupScripts() //JDB
{
	char *value;

	value = condor_param( ParallelScriptShadow, ATTR_PARALLEL_SCRIPT_SHADOW );
	if(value) {
		InsertJobExprString(ATTR_PARALLEL_SCRIPT_SHADOW,value);
	}
	value = condor_param( ParallelScriptStarter,
						  ATTR_PARALLEL_SCRIPT_STARTER );
	if(value) {
		InsertJobExprString(ATTR_PARALLEL_SCRIPT_STARTER,value);
	}
}

void
SetStdFile( int which_file )
{
	bool	transfer_it = true;
	bool	stream_it = false;
	char	*macro_value = NULL;
	char	*macro_value2 = NULL;
	const char	*generic_name;
	MyString	 strbuffer;

	switch( which_file ) 
	{
	case 0:
		generic_name = Input;
		macro_value = condor_param( TransferInput, ATTR_TRANSFER_INPUT );
		macro_value2 = condor_param( StreamInput, ATTR_STREAM_INPUT );
		break;
	case 1:
		generic_name = Output;
		macro_value = condor_param( TransferOutput, ATTR_TRANSFER_OUTPUT );
		macro_value2 = condor_param( StreamOutput, ATTR_STREAM_OUTPUT );
		break;
	case 2:
		generic_name = Error;
		macro_value = condor_param( TransferError, ATTR_TRANSFER_ERROR );
		macro_value2 = condor_param( StreamError, ATTR_STREAM_ERROR );
		break;
	default:
		fprintf( stderr, "\nERROR: Unknown standard file descriptor (%d)\n",
				 which_file ); 
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if ( macro_value ) {
		if ( macro_value[0] == 'F' || macro_value[0] == 'f' ) {
			transfer_it = false;
		}
		free( macro_value );
	}

	if ( macro_value2 ) {
		if ( macro_value2[0] == 'T' || macro_value2[0] == 't' ) {
			stream_it = true;
			stream_std_file = true;
		} else if( macro_value2[0] == 'F' || macro_value2[0] == 'f' ) {
			stream_it = false;
		}
		free( macro_value2 );
	}

	macro_value = condor_param( generic_name, NULL );

	/* Globus jobs are allowed to specify urls */
	if(JobUniverse == CONDOR_UNIVERSE_GRID && is_globus_friendly_url(macro_value)) {
		transfer_it = false;
		stream_it = false;
	}
	
	if( !macro_value || *macro_value == '\0') 
	{
		transfer_it = false;
		stream_it = false;
		// always canonicalize to the UNIX null file (i.e. /dev/null)
		macro_value = strdup(UNIX_NULL_FILE);
	}else if( strcmp(macro_value,UNIX_NULL_FILE)==0 ) {
		transfer_it = false;
		stream_it = false;
	}else {
		if( JobUniverse == CONDOR_UNIVERSE_VM ) {
			fprintf( stderr,"\nERROR: You cannot use input, ouput, "
					"and error parameters in the submit description "
					"file for vm universe\n");
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
	}
	
	if( whitespace(macro_value) ) 
	{
		fprintf( stderr,"\nERROR: The '%s' takes exactly one argument (%s)\n", 
				 generic_name, macro_value );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}	

	MyString tmp = macro_value;
	if ( check_and_universalize_path(tmp) != 0 ) {
		// we changed the value, so we have to update macro_value
		free(macro_value);
		macro_value = strdup(tmp.Value());
	}
	
	switch( which_file ) 
	{
	case 0:
		strbuffer.formatstr( "%s = \"%s\"", ATTR_JOB_INPUT, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( macro_value, O_RDONLY );
			strbuffer.formatstr( "%s = %s", ATTR_STREAM_INPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
		} else {
			strbuffer.formatstr( "%s = FALSE", ATTR_TRANSFER_INPUT );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	case 1:
		strbuffer.formatstr( "%s = \"%s\"", ATTR_JOB_OUTPUT, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			strbuffer.formatstr( "%s = %s", ATTR_STREAM_OUTPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
			stream_stdout_toggle = stream_it;
		} else {
			strbuffer.formatstr( "%s = FALSE", ATTR_TRANSFER_OUTPUT );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	case 2:
		strbuffer.formatstr( "%s = \"%s\"", ATTR_JOB_ERROR, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			strbuffer.formatstr( "%s = %s", ATTR_STREAM_ERROR, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
			stream_stderr_toggle = stream_it;
		} else {
			strbuffer.formatstr( "%s = FALSE", ATTR_TRANSFER_ERROR );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	}
		
	if( macro_value ) {
		free(macro_value);
	}
}

void
SetJobStatus()
{
	char *hold = condor_param( Hold, NULL );
	MyString buffer;

#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
#else
	// we will be inserting "JobStatus = <something>" into the jobad, but we DON'T
	// want that to be written into the cluster ad, it must always go into the proc ad.
	SetNoClusterAttr(ATTR_JOB_STATUS);
#endif

	if( hold && (hold[0] == 'T' || hold[0] == 't') ) {
		if ( Remote ) {
			fprintf( stderr,"\nERROR: Cannot set '%s' to 'true' when using -remote or -spool\n", 
					 Hold );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		buffer.formatstr( "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer);

		buffer.formatstr( "%s=\"submitted on hold at user's request\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );

		buffer.formatstr( "%s = %d", ATTR_HOLD_REASON_CODE,
				 CONDOR_HOLD_CODE_SubmittedOnHold );
		InsertJobExpr( buffer );
	} else 
	if ( Remote ) {
		buffer.formatstr( "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer);

		buffer.formatstr( "%s=\"Spooling input data files\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );

		buffer.formatstr( "%s = %d", ATTR_HOLD_REASON_CODE,
				 CONDOR_HOLD_CODE_SpoolingInput );
		InsertJobExpr( buffer );
	} else {
		buffer.formatstr( "%s = %d", ATTR_JOB_STATUS, IDLE);
		InsertJobExpr (buffer);
	}

	buffer.formatstr( "%s = %d", ATTR_ENTERED_CURRENT_STATUS,
					(int)get_submit_time() );
	InsertJobExpr( buffer );

	if( hold ) {
		free(hold);
	}
}

void
SetPriority()
{
	char *prio = condor_param( Priority, ATTR_PRIO );
	int  prioval = 0;
	MyString buffer;

	if( prio != NULL ) 
	{
		prioval = atoi (prio);
		free(prio);
	}
	buffer.formatstr( "%s = %d", ATTR_JOB_PRIO, prioval);
	InsertJobExpr (buffer);

	// also check if the job is "dirt user" priority (i.e., nice_user==True)
	char *nice_user = condor_param( NiceUser, ATTR_NICE_USER );
	if( nice_user && (*nice_user == 'T' || *nice_user == 't') )
	{
		buffer.formatstr( "%s = TRUE", ATTR_NICE_USER );
		InsertJobExpr( buffer );
		free(nice_user);
		nice_user_setting = true;
	}
	else
	{
		buffer.formatstr( "%s = FALSE", ATTR_NICE_USER );
		InsertJobExpr( buffer );
		nice_user_setting = false;
	}
}

void
SetMaxJobRetirementTime()
{
	// Assume that SetPriority() has been called before getting here.
	const char *value = NULL;

	value = condor_param( MaxJobRetirementTime, ATTR_MAX_JOB_RETIREMENT_TIME );
	if(!value && (nice_user_setting || JobUniverse == 1)) {
		// Regardless of the startd graceful retirement policy,
		// nice_user and standard universe jobs that do not specify
		// otherwise will self-limit their retirement time to 0.  So
		// the user plays nice by default, but they have the option to
		// override this (assuming, of course, that the startd policy
		// actually gives them any retirement time to play with).

		value = "0";
	}
	if(value) {
		MyString expr;
		expr.formatstr("%s = %s",ATTR_MAX_JOB_RETIREMENT_TIME, value);
		InsertJobExpr(expr);
	}
}

void
SetPeriodicHoldCheck(void)
{
	char *phc = condor_param(PeriodicHoldCheck, ATTR_PERIODIC_HOLD_CHECK);
	MyString buffer;

	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_PERIODIC_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_HOLD_CHECK, phc );
		free(phc);
	}

	InsertJobExpr( buffer );

	phc = condor_param(PeriodicHoldReason, ATTR_PERIODIC_HOLD_REASON);
	if( phc ) {
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_HOLD_REASON, phc );
		InsertJobExpr( buffer );
		free(phc);
	}

	phc = condor_param(PeriodicHoldSubCode, ATTR_PERIODIC_HOLD_SUBCODE);
	if( phc ) {
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_HOLD_SUBCODE, phc );
		InsertJobExpr( buffer );
		free(phc);
	}

	phc = condor_param(PeriodicReleaseCheck, ATTR_PERIODIC_RELEASE_CHECK);

	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_PERIODIC_RELEASE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_RELEASE_CHECK, phc );
		free(phc);
	}

	InsertJobExpr( buffer );
}

void
SetPeriodicRemoveCheck(void)
{
	char *prc = condor_param(PeriodicRemoveCheck, ATTR_PERIODIC_REMOVE_CHECK);
	MyString buffer;

	if (prc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_PERIODIC_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_REMOVE_CHECK, prc );
		free(prc);
	}

	prc = condor_param(OnExitHoldReason, ATTR_ON_EXIT_HOLD_REASON);
	if( prc ) {
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_HOLD_REASON, prc );
		InsertJobExpr( buffer );
		free(prc);
	}

	prc = condor_param(OnExitHoldSubCode, ATTR_ON_EXIT_HOLD_SUBCODE);
	if( prc ) {
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_HOLD_SUBCODE, prc );
		InsertJobExpr( buffer );
		free(prc);
	}

	InsertJobExpr( buffer );
}

#if 0 
void
SetExitHoldCheck(void)
{
	char *ehc = condor_param(OnExitHoldCheck, ATTR_ON_EXIT_HOLD_CHECK);
	MyString buffer;

	if (ehc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_HOLD_CHECK, ehc );
		free(ehc);
	}

	InsertJobExpr( buffer );
}
#endif

void
SetLeaveInQueue()
{
	char *erc = condor_param(LeaveInQueue, ATTR_JOB_LEAVE_IN_QUEUE);
	MyString buffer;

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		if ( !Remote ) {
			buffer.formatstr( "%s = FALSE", ATTR_JOB_LEAVE_IN_QUEUE );
		} else {
				/* if remote spooling, leave in the queue after the job completes
				   for up to 10 days, so user can grab the output.
				 */
			buffer.formatstr( 
				"%s = %s == %d && (%s =?= UNDEFINED || %s == 0 || ((time() - %s) < %d))",
				ATTR_JOB_LEAVE_IN_QUEUE,
				ATTR_JOB_STATUS,
				COMPLETED,
				ATTR_COMPLETION_DATE,
				ATTR_COMPLETION_DATE,
				ATTR_COMPLETION_DATE,
				60 * 60 * 24 * 10 
				);
		}
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_JOB_LEAVE_IN_QUEUE, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
}

#if 0
void
SetExitRemoveCheck(void)
{
	char *erc = condor_param(OnExitRemoveCheck, ATTR_ON_EXIT_REMOVE_CHECK);
	MyString buffer;

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_REMOVE_CHECK, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
}
#endif

void
SetNoopJob(void)
{
	char *noop = condor_param(Noop, ATTR_JOB_NOOP);
	MyString buffer;

	if (noop == NULL)
	{	
		/* user didn't want one, so just return*/
		return;
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_JOB_NOOP, noop );
		free(noop);
	}

	InsertJobExpr( buffer );
}

void
SetNoopJobExitSignal(void)
{
	char *noop = condor_param(NoopExitSignal, ATTR_JOB_NOOP_EXIT_SIGNAL);
	MyString buffer;

	if (noop == NULL)
	{	
		/* user didn't want one, so just return*/
		return;
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_JOB_NOOP_EXIT_SIGNAL, noop );
		free(noop);
	}

	InsertJobExpr( buffer );
}

void
SetNoopJobExitCode(void)
{
	char *noop = condor_param(NoopExitCode, ATTR_JOB_NOOP_EXIT_CODE);
	MyString buffer;

	if (noop == NULL)
	{	
		/* user didn't want one, so just return*/
		return;
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_JOB_NOOP_EXIT_CODE, noop );
		free(noop);
	}

	InsertJobExpr( buffer );
}

void
SetWantRemoteIO(void)
{
	char *how = condor_param( WantRemoteIO, ATTR_WANT_REMOTE_IO );
	MyString buffer;

	if (how == NULL) {
		buffer.formatstr( "%s = TRUE", 
						ATTR_WANT_REMOTE_IO);
	} else {
		buffer.formatstr( "%s = %s", ATTR_WANT_REMOTE_IO, 
						isTrue(how)?"TRUE":"FALSE" );
	}

	InsertJobExpr (buffer);

	if (how != NULL) {
		free(how);
	}
}

void
SetNotification()
{
	char *how = condor_param( Notification, ATTR_JOB_NOTIFICATION );
	int notification;
	MyString buffer;
	
	if( how == NULL ) {
		how = param ( "JOB_DEFAULT_NOTIFICATION" );		
	}
	if( (how == NULL) || (strcasecmp(how, "NEVER") == 0) ) {
		notification = NOTIFY_NEVER;
	} 
	else if( strcasecmp(how, "COMPLETE") == 0 ) {
		notification = NOTIFY_COMPLETE;
	} 
	else if( strcasecmp(how, "ALWAYS") == 0 ) {
		notification = NOTIFY_ALWAYS;
	} 
	else if( strcasecmp(how, "ERROR") == 0 ) {
		notification = NOTIFY_ERROR;
	} 
	else {
		fprintf( stderr, "\nERROR: Notification must be 'Never', "
				 "'Always', 'Complete', or 'Error'\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	buffer.formatstr( "%s = %d", ATTR_JOB_NOTIFICATION, notification);
	InsertJobExpr (buffer);

	if ( how ) {
		free(how);
	}
}

void
SetNotifyUser()
{
	bool needs_warning = false;
	static bool did_warning = false;
	MyString buffer;

	char *who = condor_param( NotifyUser, ATTR_NOTIFY_USER );

	if (who) {
		if( ! did_warning ) {
			if( !strcasecmp(who, "false") ) {
				needs_warning = true;
			}
			if( !strcasecmp(who, "never") ) {
				needs_warning = true;
			}
		}
		if( needs_warning && ! did_warning ) {
            char* tmp = param( "UID_DOMAIN" );

			fprintf( stderr, "\nWARNING: You used \"%s = %s\" in your "
					 "submit file.\n", NotifyUser, who );
			fprintf( stderr, "This means notification email will go to "
					 "user \"%s@%s\".\n", who, tmp );
			free( tmp );
			fprintf( stderr, "This is probably not what you expect!\n"
					 "If you do not want notification email, put "
					 "\"notification = never\"\n"
					 "into your submit file, instead.\n" );

			did_warning = true;
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_NOTIFY_USER, who);
		InsertJobExpr (buffer);
		free(who);
	}
}

void
SetWantGracefulRemoval()
{
	char *how = condor_param( want_graceful_removal, ATTR_WANT_GRACEFUL_REMOVAL );
	MyString buffer;

	if( how ) {
		buffer.formatstr( "%s = %s", ATTR_WANT_GRACEFUL_REMOVAL, how );
		InsertJobExpr (buffer);
		free( how );
	}
}

void
SetJobMaxVacateTime()
{
	char *expr = condor_param( job_max_vacate_time, ATTR_JOB_MAX_VACATE_TIME );
	MyString buffer;

	if( expr ) {
		buffer.formatstr( "%s = %s", ATTR_JOB_MAX_VACATE_TIME, expr );
		InsertJobExpr (buffer);
		free( expr );
	}
}

void
SetEmailAttributes()
{
	char *attrs = condor_param( EmailAttributes, ATTR_EMAIL_ATTRIBUTES );

	if ( attrs ) {
		StringList attr_list( attrs );

		if ( !attr_list.isEmpty() ) {
			char *tmp;
			MyString buffer;

			tmp = attr_list.print_to_string();
			buffer.formatstr( "%s = \"%s\"", ATTR_EMAIL_ATTRIBUTES, tmp );
			InsertJobExpr( buffer );
			free( tmp );
		}

		free( attrs );
	}
}

/**
 * We search to see if the ad has any CronTab attributes defined
 * If so, then we will check to make sure they are using the 
 * proper syntax and then stuff them in the ad
 **/
void
SetCronTab()
{
	MyString buffer;
		//
		// For convienence I put all the attributes in array
		// and just run through the ad looking for them
		//
	const char* attributes[] = { CronMinute,
								 CronHour,
								 CronDayOfMonth,
								 CronMonth,
								 CronDayOfWeek,
								};
	const int CronFields = COUNTOF(attributes);

	int ctr;
	char *param = NULL;
	CronTab::initRegexObject();
	for ( ctr = 0; ctr < CronFields; ctr++ ) {
		param = condor_param( attributes[ctr], CronTab::attributes[ctr] );
		if ( param != NULL ) {
				//
				// We'll try to be nice and validate it first
				//
			MyString error;
			if ( ! CronTab::validateParameter( ctr, param, error ) ) {
				fprintf( stderr, "ERROR: %s\n", error.Value() );
				DoCleanup( 0, 0, NULL );
				exit( 1 );
			}
				//
				// Go ahead and stuff it in the job ad now
				// The parameters must be wrapped in quotation marks
				//				
			buffer.formatstr( "%s = \"%s\"", CronTab::attributes[ctr], param );
			InsertJobExpr (buffer);
			free( param );
			NeedsJobDeferral = true;
		}		
	} // FOR
		//
		// Validation
		// Because the scheduler universe doesn't use a Starter,
		// we can't let them use the CronTab scheduling which needs 
		// to be able to use the job deferral feature
		//
	if ( NeedsJobDeferral && JobUniverse == CONDOR_UNIVERSE_SCHEDULER ) {
		fprintf( stderr, "\nCronTab scheduling does not work for scheduler "
						 "universe jobs.\n"
						 "Consider submitting this job using the local "
						 "universe, instead\n" );
		DoCleanup( 0, 0, NULL );
		fprintf( stderr, "Error in submit file\n" );
		exit(1);
	} // validation	
}

void
SetMatchListLen()
{
	MyString buffer;
	int len = 0;
	char* tmp = condor_param( LastMatchListLength,
							  ATTR_LAST_MATCH_LIST_LENGTH );
	if( tmp ) {
		len = atoi(tmp);
		buffer.formatstr( "%s = %d", ATTR_LAST_MATCH_LIST_LENGTH, len );
		InsertJobExpr( buffer );
		free( tmp );
	}
}

void
SetDAGNodeName()
{
	char* name = condor_param( ATTR_DAG_NODE_NAME_ALT, ATTR_DAG_NODE_NAME );
	MyString buffer;
	if( name ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_DAG_NODE_NAME, name );
		InsertJobExpr( buffer );
		free( name );
	}
}

void
SetDAGManJobId()
{
	char* id = condor_param( DAGManJobId, ATTR_DAGMAN_JOB_ID );
	MyString buffer;
	if( id ) {
		(void) buffer.formatstr( "%s = \"%s\"", ATTR_DAGMAN_JOB_ID, id );
		InsertJobExpr( buffer );
		free( id );
	}
}

void
SetLogNotes()
{
	LogNotesVal = condor_param( LogNotesCommand, ATTR_SUBMIT_EVENT_NOTES );
	if ( LogNotesVal ) {
		InsertJobExprString( ATTR_SUBMIT_EVENT_NOTES, LogNotesVal );
		free( LogNotesVal );
	}
}

void
SetUserNotes()
{
	UserNotesVal = condor_param( UserNotesCommand, ATTR_SUBMIT_EVENT_USER_NOTES );
	if ( UserNotesVal ) {
		InsertJobExprString( ATTR_SUBMIT_EVENT_USER_NOTES, UserNotesVal );
		free( UserNotesVal );
	}
}

void
SetStackSize()
{
	StackSizeVal = condor_param(StackSize,ATTR_STACK_SIZE);
	MyString buffer;
	if( StackSizeVal ) {
		(void) buffer.formatstr( "%s = %s", ATTR_STACK_SIZE, StackSizeVal);
		InsertJobExpr(buffer);
		free( StackSizeVal );
	}
}

void
SetRemoteInitialDir()
{
	char *who = condor_param( RemoteInitialDir, ATTR_JOB_REMOTE_IWD );
	MyString buffer;
	if (who) {
		buffer.formatstr( "%s = \"%s\"", ATTR_JOB_REMOTE_IWD, who);
		InsertJobExpr (buffer);
		free(who);
	}
}

void
SetExitRequirements()
{
	char *who = condor_param( ExitRequirements,
							  ATTR_JOB_EXIT_REQUIREMENTS );

	if (who) {
		fprintf(stderr, 
			"\nERROR: %s is deprecated.\n"
			"Please use on_exit_remove or on_exit_hold.\n", 
			ExitRequirements );
		free(who);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}
	
void
SetOutputDestination()
{
	char *od = condor_param( OutputDestination, ATTR_OUTPUT_DESTINATION );
	MyString buffer;
	if (od) {
		buffer.formatstr( "%s = \"%s\"", ATTR_OUTPUT_DESTINATION, od);
		InsertJobExpr (buffer);
		free(od);
	}
}

void
SetArguments()
{
	ArgList arglist;
	char	*args1 = condor_param( Arguments1, ATTR_JOB_ARGUMENTS1 );
		// NOTE: no ATTR_JOB_ARGUMENTS2 in the following,
		// because that is the same as Arguments1
	char    *args2 = condor_param( Arguments2 );
	char *allow_arguments_v1 = condor_param( AllowArgumentsV1 );
	bool args_success = true;
	MyString error_msg;

	if(args2 && args1 && !isTrue(allow_arguments_v1)) {
		fprintf(stderr,
		 "\nERROR: If you wish to specify both 'arguments' and\n"
		 "'arguments2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_arguments_v1=true.\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}

	if(args2) {
		args_success = arglist.AppendArgsV2Quoted(args2,&error_msg);
	}
	else if(args1) {
		args_success = arglist.AppendArgsV1WackedOrV2Quoted(args1,&error_msg);
	}

	if(!args_success) {
		if(error_msg.IsEmpty()) {
			error_msg = "ERROR in arguments.";
		}
		fprintf(stderr,"\n%s\nThe full arguments you specified were: %s\n",
				error_msg.Value(),
				args2 ? args2 : args1);
		DoCleanup(0,0,NULL);
		exit(1);
	}

	MyString strbuffer;
	MyString value;
	bool MyCondorVersionRequiresV1 = false;
	if ( !DumpClassAdToFile ) {
		MyCondorVersionRequiresV1 = arglist.InputWasV1() || arglist.CondorVersionRequiresV1(MySchedd->version());
	}
	if(MyCondorVersionRequiresV1) {
		args_success = arglist.GetArgsStringV1Raw(&value,&error_msg);
		strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_ARGUMENTS1,
					   value.EscapeChars("\"",'\\').Value());
	}
	else {
		args_success = arglist.GetArgsStringV2Raw(&value,&error_msg);
		strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_ARGUMENTS2,
					   value.EscapeChars("\"",'\\').Value());
	}

	if(!args_success) {
		fprintf(stderr,"\nERROR: failed to insert arguments: %s\n",
				error_msg.Value());
		DoCleanup(0,0,NULL);
		exit(1);
	}

	InsertJobExpr (strbuffer);

	if( JobUniverse == CONDOR_UNIVERSE_JAVA && arglist.Count() == 0)
	{
		fprintf(stderr, "\nERROR: In Java universe, you must specify the class name to run.\nExample:\n\narguments = MyClass\n\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if(args1) free(args1);
	if(args2) free(args2);
	if(allow_arguments_v1) free(allow_arguments_v1);
}

//
// SetDeferral()
// Inserts the job deferral time into the ad if present
// This needs to be called before SetRequirements()
//
void
SetJobDeferral() {
		//
		// Job Deferral Time
		// Only update the job ad if they provided a deferral time
		// We will only be able to validate the deferral time when the
		// Starter evaluates it and tries to set the timer for it
		//
	MyString buffer;
	char *temp = condor_param( DeferralTime, ATTR_DEFERRAL_TIME );
	if ( temp != NULL ) {
		// make certain the input is valid
		non_negative_int_fail(DeferralTime, temp);
			
		buffer.formatstr( "%s = %s", ATTR_DEFERRAL_TIME, temp );
		InsertJobExpr (buffer);
		free( temp );
		NeedsJobDeferral = true;
	}
	
		//
		// If this job needs the job deferral functionality, we
		// need to make sure we always add in the DeferralWindow
		// and the DeferralPrepTime attributes.
		// We have a separate if clause because SetCronTab() can
		// also set NeedsJobDeferral
		//
	if ( NeedsJobDeferral ) {		
			//
			// Job Deferral Window
			// The window allows for some slack if the Starter
			// misses the exact execute time for a job
			//
			// NOTE: There are two separate attributes, CronWindow and
			// DeferralWindow, but they are mapped to the same attribute
			// in the job's classad (ATTR_DEFERRAL_WINDOW). This is just
			// it is less confusing for users that are using one feature but
			// not the other. CronWindow overrides DeferralWindow if they
			// both are set. The manual should talk about this.
			//
		const char *windowParams[] = { CronWindow, DeferralWindow };
		const char *windowAltParams[]  = { ATTR_CRON_WINDOW, ATTR_DEFERRAL_WINDOW };
		int ctr;
		for (ctr = 0; ctr < 2; ctr++) {
			temp = condor_param( windowParams[ctr], windowAltParams[ctr] );
			if ( temp != NULL ) {
				break;
			}
		} // FOR
			//
			// If we have a parameter from the job file, use that value
			//
		if ( temp != NULL ){
			
			// make certain the input is valid
			non_negative_int_fail(DeferralWindow, temp);
			
			buffer.formatstr(  "%s = %s", ATTR_DEFERRAL_WINDOW, temp );	
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			buffer.formatstr( "%s = %d",
				   ATTR_DEFERRAL_WINDOW, JOB_DEFERRAL_WINDOW_DEFAULT );
		}
		InsertJobExpr (buffer);
		
			//
			// Job Deferral Prep Time
			// This is how many seconds before the job should run it is
			// sent over to the starter
			//
			// NOTE: There are two separate attributes, CronPrepTime and
			// DeferralPrepTime, but they are mapped to the same attribute
			// in the job's classad (ATTR_DEFERRAL_PREP_TIME). This is just
			// it is less confusing for users that are using one feature but
			// not the other. CronPrepTime overrides DeferralPrepTime if they
			// both are set. The manual should talk about this.
			//
		const char *prepParams[] = { CronPrepTime, DeferralPrepTime };
		const char *prepAltParams[]  = { ATTR_CRON_PREP_TIME, ATTR_DEFERRAL_PREP_TIME };
		for (ctr = 0; ctr < 2; ctr++) {
			temp = condor_param( prepParams[ctr], prepAltParams[ctr] );
			if ( temp != NULL ) {
				break;
			}
		} // FOR
			//
			// If we have a parameter from the job file, use that value
			//
		if ( temp != NULL ){
			// make certain the input is valid
			non_negative_int_fail(DeferralPrepTime, temp);
			
			buffer.formatstr(  "%s = %s", ATTR_DEFERRAL_PREP_TIME, temp );	
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			buffer.formatstr( "%s = %d",
				   ATTR_DEFERRAL_PREP_TIME, JOB_DEFERRAL_PREP_TIME_DEFAULT );
		}
		InsertJobExpr (buffer);
		
			//
			// Schedd Polling Interval
			// We need make sure we have this in the job ad as well
			// because it is used by the Requirements expression generated
			// in check_requirements() for job deferral 
			//
		temp = param( "SCHEDD_INTERVAL" );
		if ( temp != NULL ) {
			buffer.formatstr( "%s = %s", ATTR_SCHEDD_INTERVAL, temp );
			free( temp );
		} else {
			buffer.formatstr( "%s = %d", ATTR_SCHEDD_INTERVAL,
										SCHEDD_INTERVAL_DEFAULT );
		}
		InsertJobExpr (buffer);
	
			//
			// Validation
			// Because the scheduler universe doesn't use a Starter,
			// we can't let them use the job deferral feature
			//
		if ( JobUniverse == CONDOR_UNIVERSE_SCHEDULER ) {
			fprintf( stderr, "\nJob deferral scheduling does not work for scheduler "
							 "universe jobs.\n"
							 "Consider submitting this job using the local "
							 "universe, instead\n" );
			DoCleanup( 0, 0, NULL );
			fprintf( stderr, "Error in submit file\n" );
			exit(1);
		} // validation	
	}
}

// parse an expression string, and validate it, then wrap it in parens
// if needed in preparation for appending another expression string with the given operator
static bool check_expr_and_wrap_for_op(std::string &expr_str, classad::Operation::OpKind op)
{
	ExprTree * tree = NULL;
	bool valid_expr = (0 == ParseClassAdRvalExpr(expr_str.c_str(), tree));
	if (valid_expr && tree) { // figure out if we need to add parens
		ExprTree * expr = WrapExprTreeInParensForOp(tree, op);
		if (expr != tree) {
			tree = expr;
			expr_str.clear();
			ExprTreeToString(tree, expr_str);
		}
	}
	delete tree;
	return valid_expr;
}

void SetJobRetries()
{
	std::string erc, ehc;
	condor_param_exists(OnExitRemoveCheck, ATTR_ON_EXIT_REMOVE_CHECK, erc);
	condor_param_exists(OnExitHoldCheck, ATTR_ON_EXIT_HOLD_CHECK, ehc);

	long long num_retries = param_integer("DEFAULT_JOB_MAX_RETRIES", 10);
	long long success_code = 0;
	std::string retry_until;

	bool enable_retries = false;
	if (condor_param_long_exists(MaxRetries, ATTR_JOB_MAX_RETRIES, num_retries)) { enable_retries = true; }
	if (condor_param_long_exists(SuccessExitCode, ATTR_JOB_SUCCESS_EXIT_CODE, success_code, true)) { enable_retries = true; }
	if (condor_param_exists(RetryUntil, NULL, retry_until)) { enable_retries = true; }
	if ( ! enable_retries)
	{
		// if none of these knobs are defined, then there are no retries.
		// Just insert the default on-exit-hold and on-exit-remove expressions
		if (erc.empty()) {
			job->Assign (ATTR_ON_EXIT_REMOVE_CHECK, true);
		} else {
			erc.insert(0, ATTR_ON_EXIT_REMOVE_CHECK "=");
			InsertJobExpr(erc.c_str());
		}
		if (ehc.empty()) {
			job->Assign (ATTR_ON_EXIT_HOLD_CHECK, false);
		} else {
			ehc.insert(0, ATTR_ON_EXIT_HOLD_CHECK "=");
			InsertJobExpr(ehc.c_str());
		}
		return;
	}

	// if there is a retry_until value, figure out of it is an fultility exit code or an expression
	// and validate it.
	if ( ! retry_until.empty()) {
		ExprTree * tree = NULL;
		bool valid_retry_until = (0 == ParseClassAdRvalExpr(retry_until.c_str(), tree));
		if (valid_retry_until && tree) {
			ClassAd tmp;
			StringList refs;
			tmp.GetExprReferences(retry_until.c_str(), &refs, &refs);
			long long futility_code;
			if (refs.isEmpty() && string_is_long_param(retry_until.c_str(), futility_code)) {
				if (futility_code < INT_MIN || futility_code > INT_MAX) {
					valid_retry_until = false;
				} else {
					retry_until.clear();
					formatstr(retry_until, ATTR_ON_EXIT_CODE " == %d", (int)futility_code);
				}
			} else {
				ExprTree * expr = WrapExprTreeInParensForOp(tree, classad::Operation::LOGICAL_OR_OP);
				if (expr != tree) {
					tree = expr;
					retry_until.clear();
					ExprTreeToString(tree, retry_until);
				}
			}
		}
		delete tree;

		if ( ! valid_retry_until) {
			fprintf(stderr, "\nERROR: %s=%s is invalid, it must be an integer or a boolean expression.\n", RetryUntil, retry_until.c_str());
			DoCleanup(0,0,NULL);
			exit(1);
		}
	}

	job->Assign(ATTR_JOB_MAX_RETRIES, num_retries);

	// Build the appropriate OnExitRemove expression, we will fill in success exit status value and other clauses later.
	const char * basic_exit_remove_expr = ATTR_ON_EXIT_REMOVE_CHECK " = "
		ATTR_NUM_JOB_COMPLETIONS " > " ATTR_JOB_MAX_RETRIES " || " ATTR_ON_EXIT_CODE " == ";

	// build the sub expression that checks for exit codes that should end retries
	std::string code_check;
	if (success_code != 0) {
		job->Assign(ATTR_JOB_SUCCESS_EXIT_CODE, success_code);
		code_check = ATTR_JOB_SUCCESS_EXIT_CODE;
	} else {
		formatstr(code_check, "%d", (int)success_code);
	}
	if ( ! retry_until.empty()) {
		code_check += " || ";
		code_check += retry_until;
	}

	// paste up the final OnExitRemove expression and insert it into the job.
	std::string onexitrm(basic_exit_remove_expr);
	onexitrm += code_check;

	// if the user also supplied an on_exit_remove expression, || it in.
	if ( ! erc.empty()) {
		if ( ! check_expr_and_wrap_for_op(erc, classad::Operation::LOGICAL_OR_OP)) {
			fprintf(stderr, "\nERROR: %s=%s is invalid, it must be a boolean expression.\n", OnExitRemoveCheck, erc.c_str());
			DoCleanup(0,0,NULL);
			exit(1);
		}
		onexitrm += " || ";
		onexitrm += erc;
	}
	// Insert the final OnExitRemove expression into the job
	InsertJobExpr(onexitrm.c_str());

	// paste up the final OnExitHold expression and insert it into the job.
	if (ehc.empty()) {
		job->Assign (ATTR_ON_EXIT_HOLD_CHECK, false);
	} else {
		ehc.insert(0, ATTR_ON_EXIT_HOLD_CHECK "=");
		InsertJobExpr(ehc.c_str());
	}
}

class EnvFilter : public Env
{
public:
	EnvFilter( const char *env1, const char *env2 )
			: m_env1( env1 ),
			  m_env2( env2 ) {
		return;
	};
	virtual ~EnvFilter( void ) { };
	virtual bool ImportFilter( const MyString &var,
							   const MyString &val ) const;
private:
	const char	*m_env1;
	const char	*m_env2;
};
bool
EnvFilter::ImportFilter( const MyString & var, const MyString &val ) const
{
	if( !m_env2 && m_env1 && !IsSafeEnvV1Value(val.Value())) {
		// We silently filter out anything that is not expressible
		// in the 'environment1' syntax.  This avoids breaking
		// our ability to submit jobs to older startds that do
		// not support 'environment2' syntax.
		return false;
	}
	if( !IsSafeEnvV2Value(val.Value()) ) {
		// Silently filter out environment values containing
		// unsafe characters.  Example: newlines cause the
		// schedd to EXCEPT in 6.8.3.
		return false;
	}
	MyString existing_val;
	if ( GetEnv( var, existing_val ) ) {
		// Don't override submit file environment settings --
		// check if environment variable is already set.
		return false;
	}
	return true;
}

void
SetEnvironment()
{
	char *env1 = condor_param( Environment1, ATTR_JOB_ENVIRONMENT1 );
		// NOTE: no ATTR_JOB_ENVIRONMENT2 in the following,
		// because that is the same as Environment1
	char *env2 = condor_param( Environment2 );
	char *allow_environment_v1 = condor_param( AllowEnvironmentV1 );
	bool allow_v1 = isTrue(allow_environment_v1);
	char *shouldgetenv = condor_param( GetEnvironment, "get_env" );
	char *allowscripts = condor_param( AllowStartupScript,
									   "AllowStartupScript" );
	EnvFilter envobject( env1, env2 );
    MyString varname;

	if( env1 && env2 && !allow_v1 ) {
		fprintf(stderr,
		 "\nERROR: If you wish to specify both 'environment' and\n"
		 "'environment2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_environment_v1=true.\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}

	bool env_success;
	char const *environment_string = env2 ? env2 : env1;
	MyString error_msg;
	if(env2) {
		env_success = envobject.MergeFromV2Quoted(env2,&error_msg);
	}
	else {
		env_success = envobject.MergeFromV1RawOrV2Quoted(env1,&error_msg);
	}

	if(!env_success) {
		fprintf(stderr,
				"\n%s\nThe environment you specified was: '%s'\n",
				error_msg.Value(),environment_string);
		DoCleanup(0,0,NULL);
		exit(1);
	}

	if (allowscripts && (*allowscripts=='T' || *allowscripts=='t') ) {
		envobject.SetEnv("_CONDOR_NOCHECK","1");
	}

	// grab user's environment if getenv == TRUE
	if ( shouldgetenv && (toupper(shouldgetenv[0]) == 'T') ) {
		envobject.Import( );
	}

	//There may already be environment info in the ClassAd from SUBMIT_ATTRS.
	//Check for that now.

	bool ad_contains_env1 = job->LookupExpr(ATTR_JOB_ENVIRONMENT1);
	bool ad_contains_env2 = job->LookupExpr(ATTR_JOB_ENVIRONMENT2);

	bool MyCondorVersionRequiresV1 = false;
	if ( !DumpClassAdToFile ) {
		MyCondorVersionRequiresV1 = envobject.InputWasV1() 
			|| envobject.CondorVersionRequiresV1(MySchedd->version());
	}
	bool insert_env1 = MyCondorVersionRequiresV1;
	bool insert_env2 = !insert_env1;

	if(!env1 && !env2 && envobject.Count() == 0 && \
	   (ad_contains_env1 || ad_contains_env2)) {
			// User did not specify any environment, but SUBMIT_ATTRS did.
			// Do not do anything (i.e. avoid overwriting with empty env).
		insert_env1 = insert_env2 = false;
	}

	// If we are going to write new environment into the ad and if there
	// is already environment info in the ad, make sure we overwrite
	// whatever is there.  Instead of doing things this way, we could
	// remove the existing environment settings, but there is currently no
	// function that undoes all the effects of InsertJobExpr().
	if(insert_env1 && ad_contains_env2) insert_env2 = true;
	if(insert_env2 && ad_contains_env1) insert_env1 = true;

	env_success = true;

	if(insert_env1 && env_success) {
		MyString newenv;
		MyString newenv_raw;

		env_success = envobject.getDelimitedStringV1Raw(&newenv_raw,&error_msg);
		newenv.formatstr("%s = \"%s\"",
					   ATTR_JOB_ENVIRONMENT1,
					   newenv_raw.EscapeChars("\"",'\\').Value());
		InsertJobExpr(newenv);

		// Record in the JobAd the V1 delimiter that is being used.
		// This way remote submits across platforms have a prayer.
		MyString delim_assign;
		delim_assign.formatstr("%s = \"%c\"",
							 ATTR_JOB_ENVIRONMENT1_DELIM,
							 envobject.GetEnvV1Delimiter());
		InsertJobExpr(delim_assign);
	}

	if(insert_env2 && env_success) {
		MyString newenv;
		MyString newenv_raw;

		env_success = envobject.getDelimitedStringV2Raw(&newenv_raw,&error_msg);
		newenv.formatstr("%s = \"%s\"",
					   ATTR_JOB_ENVIRONMENT2,
					   newenv_raw.EscapeChars("\"",'\\').Value());
		InsertJobExpr(newenv);
	}

	if(!env_success) {
		fprintf(stderr,
				"\nERROR: failed to insert environment into job ad: %s\n",
				error_msg.Value());
		DoCleanup(0,0,NULL);
		exit(1);
	}

	free(env2);
	free(env1);

	if( allowscripts ) {
		free(allowscripts);
	}
	if( shouldgetenv ) {
		free(shouldgetenv);
	}
	return;
}

#if !defined(WIN32)
void
ComputeRootDir()
{
	char *rootdir = condor_param( RootDir, ATTR_JOB_ROOT_DIR );

	if( rootdir == NULL ) 
	{
		JobRootdir = "/";
	} 
	else 
	{
		if( access(rootdir, F_OK|X_OK) < 0 ) {
			fprintf( stderr, "\nERROR: No such directory: %s\n",
					 rootdir );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		MyString rootdir_str = rootdir;
		check_and_universalize_path(rootdir_str);
		JobRootdir = rootdir_str;
		free(rootdir);
	}
}

void
SetRootDir()
{
	MyString buffer;
	ComputeRootDir();
	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_ROOT_DIR, JobRootdir.Value());
	InsertJobExpr (buffer);
}
#endif

void
SetRequirements()
{
	char *requirements = condor_param( Requirements, NULL );
	MyString tmp;
	MyString buffer;
	if( requirements == NULL ) 
	{
		JobRequirements = "";
	} 
	else 
	{
		JobRequirements = requirements;
		free(requirements);
	}

	check_requirements( JobRequirements.Value(), tmp );
	buffer.formatstr( "%s = %s", ATTR_REQUIREMENTS, tmp.Value());
	JobRequirements = tmp;

	InsertJobExpr (buffer);
	
	char* fs_domain = NULL;
	if( (should_transfer == STF_NO || should_transfer == STF_IF_NEEDED) 
		&& ! job->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &fs_domain) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, 
				 My_fs_domain ); 
		InsertJobExpr( buffer );
	}
	if( fs_domain ) {
		free( fs_domain );
	}
}

void
SetEncryptExecuteDir()
{
	char *enc =
		condor_param( EncryptExecuteDir, ATTR_ENCRYPT_EXECUTE_DIRECTORY );

	MyString buf;
	if (enc && isTrue(enc)) {
		HasEncryptExecuteDir = true;
	} else {
		HasEncryptExecuteDir = false;
	}
	buf.formatstr("%s = %s", ATTR_ENCRYPT_EXECUTE_DIRECTORY,
			HasEncryptExecuteDir ? "True" : "False");
	InsertJobExpr( buf.Value() );

	if (enc) free(enc);
}

void
SetTDP( void )
{
		// tdp_cmd and tdp_input are global since those effect
		// SetRequirements() an SetTransferFiles(), too. 
	tdp_cmd = condor_param( ToolDaemonCmd, ATTR_TOOL_DAEMON_CMD );
	tdp_input = condor_param( ToolDaemonInput, ATTR_TOOL_DAEMON_INPUT );
	char* tdp_args1 = condor_param( ToolDaemonArgs );
	char* tdp_args1_ext = condor_param( ToolDaemonArguments1, ATTR_TOOL_DAEMON_ARGS1 );
		// NOTE: no ATTR_TOOL_DAEMON_ARGS2 in the following,
		// because that is the same as ToolDaemonArguments1.
	char* tdp_args2 = condor_param( ToolDaemonArguments2 );
	char* allow_arguments_v1 = condor_param(AllowArgumentsV1);
	char* tdp_error = condor_param( ToolDaemonError, ATTR_TOOL_DAEMON_ERROR );
	char* tdp_output = condor_param( ToolDaemonOutput, 
									 ATTR_TOOL_DAEMON_OUTPUT );
	char* suspend_at_exec = condor_param( SuspendJobAtExec,
										  ATTR_SUSPEND_JOB_AT_EXEC );
	MyString buf;
	MyString path;

	if( tdp_cmd ) {
		HasTDP = true;
		path = tdp_cmd;
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_CMD, path.Value() );
		InsertJobExpr( buf.Value() );
	}
	if( tdp_input ) {
		path = tdp_input;
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_INPUT, path.Value() );
		InsertJobExpr( buf.Value() );
	}
	if( tdp_output ) {
		path = tdp_output;
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_OUTPUT, path.Value() );
		InsertJobExpr( buf.Value() );
		free( tdp_output );
		tdp_output = NULL;
	}
	if( tdp_error ) {
		path = tdp_error;
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_ERROR, path.Value() );
		InsertJobExpr( buf.Value() );
		free( tdp_error );
		tdp_error = NULL;
	}

	bool args_success = true;
	MyString error_msg;
	ArgList args;

	if(tdp_args1_ext && tdp_args1) {
		fprintf(stderr,
				"\nERROR: you specified both tdp_daemon_args and tdp_daemon_arguments\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}
	if(tdp_args1_ext) {
		free(tdp_args1);
		tdp_args1 = tdp_args1_ext;
		tdp_args1_ext = NULL;
	}

	if(tdp_args2 && tdp_args1 && !isTrue(allow_arguments_v1)) {
		fprintf(stderr,
		 "\nERROR: If you wish to specify both 'tool_daemon_arguments' and\n"
		 "'tool_daemon_arguments2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_arguments_v1=true.\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}

	if( tdp_args2 ) {
		args_success = args.AppendArgsV2Quoted(tdp_args2,&error_msg);
	}
	else if( tdp_args1 ) {
		args_success = args.AppendArgsV1WackedOrV2Quoted(tdp_args1,&error_msg);
	}

	if(!args_success) {
		fprintf(stderr,"ERROR: failed to parse tool daemon arguments: %s\n"
				"The arguments you specified were: %s\n",
				error_msg.Value(),
				tdp_args2 ? tdp_args2 : tdp_args1);
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	MyString args_value;
	bool MyCondorVersionRequiresV1 = false;
	if ( !DumpClassAdToFile ) {
		MyCondorVersionRequiresV1 = args.InputWasV1() || args.CondorVersionRequiresV1(MySchedd->version());
	}
	if(MyCondorVersionRequiresV1) {
		args_success = args.GetArgsStringV1Raw(&args_value,&error_msg);
		if(!args_value.IsEmpty()) {
			buf.formatstr("%s = \"%s\"",ATTR_TOOL_DAEMON_ARGS1,
						args_value.EscapeChars("\"",'\\').Value());
			InsertJobExpr( buf );
		}
	}
	else if(args.Count()) {
		args_success = args.GetArgsStringV2Raw(&args_value,&error_msg);
		if(!args_value.IsEmpty()) {
			buf.formatstr("%s = \"%s\"",ATTR_TOOL_DAEMON_ARGS2,
						args_value.EscapeChars("\"",'\\').Value());
			InsertJobExpr( buf );
		}
	}

	if(!args_success) {
		fprintf(stderr,"ERROR: failed to insert tool daemon arguments: %s\n",
				error_msg.Value());
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if( suspend_at_exec ) {
		buf.formatstr( "%s = %s", ATTR_SUSPEND_JOB_AT_EXEC,
					 isTrue(suspend_at_exec) ? "TRUE" : "FALSE" );
		InsertJobExpr( buf.Value() );
		free( suspend_at_exec );
		suspend_at_exec = NULL;
	}

	free(tdp_args1);
	free(tdp_args2);
	free(tdp_args1_ext);
	free(allow_arguments_v1);
}

void
SetRunAsOwner()
{
	char *run_as_owner = condor_param(RunAsOwner, ATTR_JOB_RUNAS_OWNER);
	bool bRunAsOwner=false;
	if (run_as_owner == NULL) {
		return;
	}
	else {
		bRunAsOwner = isTrue(run_as_owner);
		free(run_as_owner);
	}

	MyString buffer;
	buffer.formatstr(  "%s = %s", ATTR_JOB_RUNAS_OWNER, bRunAsOwner ? "True" : "False" );
	InsertJobExpr (buffer);

#if defined(WIN32)
	// make sure we have a CredD
	// (RunAsOwner is global for use in SetRequirements(),
	//  the memory is freed() there)
	if( bRunAsOwner && NULL == ( RunAsOwnerCredD = param("CREDD_HOST") ) ) {
		fprintf(stderr,
				"\nERROR: run_as_owner requires a valid CREDD_HOST configuration macro\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}
#endif
}

void 
SetLoadProfile()
{
    char *load_profile_param = condor_param (
        LoadProfile, 
        ATTR_JOB_LOAD_PROFILE );

    if ( NULL == load_profile_param ) {
        return;
    }

    if ( !isTrue ( load_profile_param ) ) {
        free ( load_profile_param );
        return;
    }
    
    MyString buffer;
    buffer.formatstr ( "%s = True", ATTR_JOB_LOAD_PROFILE );
    InsertJobExpr ( buffer );

    /* If we've been called it means that SetRunAsOwner() has already 
    made sure we have a CredD.  This will become more important when
    we allow random users to load their profile (which will only at 
    that point be the user's registry).  The limitation is to stop
    heavy weight users profiles; you know the ones: they have20+ GBs 
    of music, thousands of pictures and a few random movies from 
    caching their profile on the local machine (which may be someone's
    laptop, which may already be running low on disk-space). */
}

void
SetRank()
{
	MyString rank;
	char *orig_pref = condor_param( Preferences, NULL );
	char *orig_rank = condor_param( Rank, NULL );
	char *default_rank = NULL;
	char *append_rank = NULL;
	MyString buffer;

	switch( JobUniverse ) {
	case CONDOR_UNIVERSE_STANDARD:
		default_rank = param( "DEFAULT_RANK_STANDARD" );
		append_rank = param( "APPEND_RANK_STANDARD" );
		break;
	case CONDOR_UNIVERSE_VANILLA:
		default_rank = param( "DEFAULT_RANK_VANILLA" );
		append_rank = param( "APPEND_RANK_VANILLA" );
		break;
	default:
		default_rank = NULL;
		append_rank = NULL;
	}

		// If they're not yet defined, or they're defined but empty,
		// try the generic, non-universe-specific versions.
	if( ! default_rank || ! default_rank[0]  ) {
		if (default_rank) { free(default_rank); default_rank = NULL; }
		default_rank = param("DEFAULT_RANK");
	}
	if( ! append_rank || ! append_rank[0]  ) {
		if (append_rank) { free(append_rank); append_rank = NULL; }
		append_rank = param("APPEND_RANK");
	}

		// If any of these are defined but empty, treat them as
		// undefined, or else, we get nasty errors.  -Derek W. 8/21/98
	if( default_rank && !default_rank[0] ) {
		free(default_rank);
		default_rank = NULL;
	}
	if( append_rank && !append_rank[0] ) {
		free(append_rank);
		append_rank = NULL;
	}

		// If we've got a rank to append to something that's already 
		// there, we need to enclose the origional in ()'s
	if( append_rank && (orig_rank || orig_pref || default_rank) ) {
		rank += "(";
	}		

	if( orig_pref && orig_rank ) {
		fprintf( stderr, "\nERROR: %s and %s may not both be specified "
				 "for a job\n", Preferences, Rank );
		exit(1);
	} else if( orig_rank ) {
		rank +=  orig_rank;
	} else if( orig_pref ) {
		rank += orig_pref;
	} else if( default_rank ) {
		rank += default_rank;
	} 

	if( append_rank ) {
		if( rank.Length() > 0 ) {
				// We really want to add this expression to our
				// existing Rank expression, not && it, since Rank is
				// just a float.  If we && it, we're forcing the whole
				// expression to now be a bool that evaluates to
				// either 0 or 1, which is obviously not what we
				// want.  -Derek W. 8/21/98
			rank += ") + (";
		} else {
			rank +=  "(";
		}
		rank += append_rank;
		rank += ")";
	}
				
	if( rank.Length() == 0 ) {
		buffer.formatstr( "%s = 0.0", ATTR_RANK );
		InsertJobExpr( buffer );
	} else {
		buffer.formatstr( "%s = %s", ATTR_RANK, rank.Value() );
		InsertJobExpr( buffer );
	}

	if ( orig_pref ) 
		free(orig_pref);
	if ( orig_rank )
		free(orig_rank);

	if (default_rank) {
		free(default_rank);
		default_rank = NULL;
	}
	if (append_rank) {
		free(append_rank);
		append_rank = NULL;
	}
}

void
ComputeIWD()
{
	char	*shortname;
	MyString	iwd;
	MyString	cwd;

	shortname = condor_param( InitialDir, ATTR_JOB_IWD );
	if( ! shortname ) {
			// neither "initialdir" nor "iwd" were there, try some
			// others, just to be safe:
		shortname = condor_param( "initial_dir", "job_iwd" );
	}

#if !defined(WIN32)
	ComputeRootDir();
	if( JobRootdir != "/" )	{	/* Rootdir specified */
		if( shortname ) {
			iwd = shortname;
		} 
		else {
			iwd = "/";
		}
	} 
	else 
#endif
	{  //for WIN32, this is a block to make {}'s match. For unix, else block.
		if( shortname  ) {
#if defined(WIN32)
			// if a drive letter or share is specified, we have a full pathname
			if( shortname[1] == ':' || (shortname[0] == '\\' && shortname[1] == '\\')) 
#else
			if( shortname[0] == '/' ) 
#endif
			{
				iwd = shortname;
			} 
			else {
				condor_getcwd( cwd );
				iwd.formatstr( "%s%c%s", cwd.Value(), DIR_DELIM_CHAR, shortname );
			}
		} 
		else {
			condor_getcwd( iwd );
		}
	}

	compress( iwd );
	check_and_universalize_path(iwd);
	check_iwd( iwd.Value() );
	JobIwd = iwd;

	if ( shortname )
		free(shortname);
}

void
SetIWD()
{
	MyString buffer;
	ComputeIWD();
	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_IWD, JobIwd.Value());
	InsertJobExpr (buffer);
}

void
check_iwd( char const *iwd )
{
	MyString pathname;

#if defined(WIN32)
	pathname = iwd;
#else
	pathname.formatstr( "%s/%s", JobRootdir.Value(), iwd );
#endif
	compress( pathname );

	if( access(pathname.Value(), F_OK|X_OK) < 0 ) {
		fprintf( stderr, "\nERROR: No such directory: %s\n", pathname.Value() );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}

void
SetUserLog()
{
	static const char* submit_names[] = { UserLogFile, DagmanLogFile, 0 };
	static const char* jobad_attribute_names[] = { ATTR_ULOG_FILE, ATTR_DAGMAN_WORKFLOW_LOG, 0 };
	for(const char** p = &submit_names[0], **q = &jobad_attribute_names[0];
			*p && *q; ++p, ++q) {
		char *ulog_entry = condor_param( *p, *q );

		if ( ulog_entry && strcmp( ulog_entry, "" ) != 0 ) {
			std::string buffer;
				// Note:  I don't think the return value here can ever
				// be NULL.  wenger 2016-10-04
			const char* ulog_pcc = full_path( ulog_entry );
			if(ulog_pcc) {
				std::string ulog(ulog_pcc);
				if ( !DumpClassAdToFile && !DashDryRun ) {
					// check that the log is a valid path
					if ( !DisableFileChecks ) {
						FILE* test = safe_fopen_wrapper_follow(ulog.c_str(), "a+", 0664);
						if (!test) {
							fprintf(stderr,
									"\nERROR: Invalid log file: \"%s\" (%s)\n", ulog.c_str(),
									strerror(errno));
							exit( 1 );
						} else {
							fclose(test);
						}
					}

					// Check that the log file isn't on NFS
					bool nfs_is_error = param_boolean("LOG_ON_NFS_IS_ERROR", false);
					bool nfs = false;

					if ( nfs_is_error ) {
						if ( fs_detect_nfs( ulog.c_str(), &nfs ) != 0 ) {
							fprintf(stderr,
									"\nWARNING: Can't determine whether log file %s is on NFS\n",
									ulog.c_str() );
						} else if ( nfs ) {

							fprintf(stderr,
									"\nERROR: Log file %s is on NFS.\nThis could cause"
									" log file corruption. Condor has been configured to"
									" prohibit log files on NFS.\n",
									ulog.c_str() );

							DoCleanup(0,0,NULL);
							exit( 1 );

						}
					}
				}

				MyString mulog(ulog.c_str());
				check_and_universalize_path(mulog);
				buffer += mulog.Value();
				UserLogSpecified = true;
			}
			std::string logExpr(*q);
			logExpr += " = ";
			logExpr += "\"";
			logExpr += buffer;
			logExpr += "\"";
			InsertJobExpr(logExpr.c_str());
			free(ulog_entry);
		}
	}
}

void
SetUserLogXML()
{
	MyString buffer;
	char *use_xml = condor_param(UseLogUseXML,
								 ATTR_ULOG_USE_XML);

	if (use_xml) {
		if ('T' == *use_xml || 't' == *use_xml) {
			UseXMLInLog = true;
			buffer.formatstr( "%s = TRUE", ATTR_ULOG_USE_XML);
			InsertJobExpr( buffer );
		} else {
			buffer.formatstr( "%s = FALSE", ATTR_ULOG_USE_XML);
		}
		free(use_xml);
	}
	return;
}


void
SetCoreSize()
{
	char *size = condor_param( CoreSize, "core_size" );
	long coresize = 0;
	MyString buffer;

	if (size == NULL) {
#if defined(HPUX) || defined(WIN32) /* RLIMIT_CORE not supported */
		size = "";
#else
		struct rlimit rl;
		if ( getrlimit( RLIMIT_CORE, &rl ) == -1) {
			EXCEPT("getrlimit failed");
		}

		// this will effectively become the hard limit for core files when
		// the job is executed
		coresize = (long)rl.rlim_cur;
#endif
	} else {
		coresize = atoi(size);
		free(size);
	}

	buffer.formatstr( "%s = %ld", ATTR_CORE_SIZE, coresize);
	InsertJobExpr(buffer);
}


void
SetJobLease( void )
{
	static bool warned_too_small = false;
	long lease_duration = 0;
	auto_free_ptr tmp(condor_param( "job_lease_duration", ATTR_JOB_LEASE_DURATION ));
	if( ! tmp ) {
		if( universeCanReconnect(JobUniverse)) {
				/*
				  if the user didn't define a lease duration, but is
				  submitting a job from a universe that supports
				  reconnect and is NOT trying to use streaming I/O, we
				  want to define a default of 20 minutes so that
				  reconnectable jobs can survive schedd crashes and
				  the like...
				*/
			lease_duration = 40 * 60;
		} else {
				// not defined and can't reconnect, we're done.
			return;
		}
	} else {
		char *endptr = NULL;
		lease_duration = strtol(tmp.ptr(), &endptr, 10);
		if (endptr != tmp.ptr()) {
			while (isspace(*endptr)) {
				endptr++;
			}
		}
		bool is_number = (endptr != tmp.ptr() && *endptr == '\0');
		if ( ! is_number) {
			lease_duration = 0; // set to zero to indicate it's an expression
		} else {
			if (lease_duration == 0) {
				// User explicitly didn't want a lease, so we're done.
				return;
			}
			if (lease_duration < 20) {
				if (! warned_too_small) { 
					fprintf(stderr, "\nWARNING: %s less than 20 seconds is not "
							"allowed, using 20 instead\n",
							ATTR_JOB_LEASE_DURATION);
					warned_too_small = true;
				}
				lease_duration = 20;
			}
		}
	}
	MyString val = ATTR_JOB_LEASE_DURATION;
	val += "=";
	if (lease_duration) {
		val += lease_duration;
	} else if (tmp) {
		val += tmp.ptr();
	}
	InsertJobExpr( val.Value() );
}


void
SetForcedAttributes()
{
	MyString buffer;

	for (classad::References::const_iterator it = forced_submit_attrs.begin(); it != forced_submit_attrs.end(); ++it) {
		char * value = param(it->c_str());
		if ( ! value)
			continue;
	#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
		buffer.formatstr( "%s = %s", it->c_str(), value);
		InsertJobExpr(buffer);
	#else
		this is unsupported...
	#endif
		free(value);
	}

	HASHITER it = hash_iter_begin(SubmitMacroSet);
	for( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char *my_name = hash_iter_key(it);
		if ( ! starts_with_ignore_case(my_name, "MY.")) continue;
		const char * name = my_name+sizeof("MY.")-1;

		char * value = condor_param(my_name); // lookup and expand macros.
	#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
		buffer.formatstr( "%s = %s", name, (value && value[0]) ? value : "undefined" );
		InsertJobExpr(buffer);
	#else
		if (value && value[0]) {
			buffer.formatstr( "%s = %s", name, value);

			// Call InserJobExpr with checkcluster set to false.
			// This will result in forced attributes always going
			// into the proc ad, not the cluster ad.  This allows
			// us to easily remove attributes with the "-" command.
			InsertJobExpr(buffer);
			SetNoClusterAttr(name);
		} else {
			job->Delete( name );
		}
	#endif
		if (value) free(value);
	}
	hash_iter_delete(&it);
}

void
SetGridParams()
{
	char *tmp;
	MyString buffer;
	FILE* fp;

	if ( JobUniverse != CONDOR_UNIVERSE_GRID )
		return;

	tmp = condor_param( GridResource, ATTR_GRID_RESOURCE );
	if ( tmp ) {
			// TODO validate number of fields in grid_resource?

		buffer.formatstr( "%s = \"%s\"", ATTR_GRID_RESOURCE, tmp );
		InsertJobExpr( buffer );

		if ( strstr(tmp,"$$") ) {
				// We need to perform matchmaking on the job in order
				// to fill GridResource.
			buffer.formatstr("%s = FALSE", ATTR_JOB_MATCHED);
			InsertJobExpr (buffer);
			buffer.formatstr("%s = 0", ATTR_CURRENT_HOSTS);
			InsertJobExpr (buffer);
			buffer.formatstr("%s = 1", ATTR_MAX_HOSTS);
			InsertJobExpr (buffer);
		}

		if ( strcasecmp( tmp, "ec2" ) == 0 ) {
			fprintf(stderr, "\nERROR: EC2 grid jobs require a "
					"service URL\n");
			DoCleanup( 0, 0, NULL );
			exit( 1 );
		}
		

		free( tmp );

	} else {
			// TODO Make this allowable, triggering matchmaking for
			//   GridResource
		fprintf(stderr, "\nERROR: No resource identifier was found.\n" );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( JobGridType == NULL ||
		 strcasecmp (JobGridType, "gt2") == MATCH ||
		 strcasecmp (JobGridType, "gt5") == MATCH ||
		 strcasecmp (JobGridType, "nordugrid") == MATCH ) {

		if( (tmp = condor_param(GlobusResubmit,ATTR_GLOBUS_RESUBMIT_CHECK)) ) {
			buffer.formatstr( "%s = %s", ATTR_GLOBUS_RESUBMIT_CHECK, tmp );
			free(tmp);
			InsertJobExpr (buffer);
		} else {
			buffer.formatstr( "%s = FALSE", ATTR_GLOBUS_RESUBMIT_CHECK);
			InsertJobExpr (buffer);
		}
	}

	if ( (tmp = condor_param(GridShell, ATTR_USE_GRID_SHELL)) ) {

		if( tmp[0] == 't' || tmp[0] == 'T' ) {
			buffer.formatstr( "%s = TRUE", ATTR_USE_GRID_SHELL );
			InsertJobExpr( buffer );
		}
		free(tmp);
	}

	if ( JobGridType == NULL ||
		 strcasecmp (JobGridType, "gt2") == MATCH ||
		 strcasecmp (JobGridType, "gt5") == MATCH ) {

		buffer.formatstr( "%s = %d", ATTR_GLOBUS_STATUS,
				 GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED );
		InsertJobExpr (buffer);

		buffer.formatstr( "%s = 0", ATTR_NUM_GLOBUS_SUBMITS );
		InsertJobExpr (buffer);
	}

	buffer.formatstr( "%s = False", ATTR_WANT_CLAIMING );
	InsertJobExpr(buffer);

	if( (tmp = condor_param(GlobusRematch,ATTR_REMATCH_CHECK)) ) {
		buffer.formatstr( "%s = %s", ATTR_REMATCH_CHECK, tmp );
		free(tmp);
		InsertJobExpr (buffer);
	}

	if( (tmp = condor_param(GlobusRSL, ATTR_GLOBUS_RSL)) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_GLOBUS_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if( (tmp = condor_param(NordugridRSL, ATTR_NORDUGRID_RSL)) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_NORDUGRID_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if( (tmp = condor_param(CreamAttributes, ATTR_CREAM_ATTRIBUTES)) ) {
		InsertJobExprString ( ATTR_CREAM_ATTRIBUTES, tmp );
		free( tmp );
	}

	if( (tmp = condor_param(BatchQueue, ATTR_BATCH_QUEUE)) ) {
		InsertJobExprString ( ATTR_BATCH_QUEUE, tmp );
		free( tmp );
	}

	if ( (tmp = condor_param( KeystoreFile, ATTR_KEYSTORE_FILE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_KEYSTORE_FILE, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "unicore" ) == 0 ) {
		fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
				"parameter\n", KeystoreFile );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( (tmp = condor_param( KeystoreAlias, ATTR_KEYSTORE_ALIAS )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_KEYSTORE_ALIAS, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "unicore" ) == 0 ) {
		fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
				"parameter\n", KeystoreAlias );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( (tmp = condor_param( KeystorePassphraseFile,
							  ATTR_KEYSTORE_PASSPHRASE_FILE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_KEYSTORE_PASSPHRASE_FILE, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "unicore" ) == 0 ) {
		fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
				"parameter\n", KeystorePassphraseFile );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}
	
	//
	// EC2 grid-type submit attributes
	//
	if ( (tmp = condor_param( EC2AccessKeyId, ATTR_EC2_ACCESS_KEY_ID )) ) {
		// check public key file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open public key file %s (%s)\n", 
							 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);

			StatInfo si(full_path(tmp));
			if (si.IsDirectory()) {
				fprintf(stderr, "\nERROR: %s is a directory\n", full_path(tmp));
				exit(1);
			}
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_ACCESS_KEY_ID, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "ec2" ) == 0 ) {
		fprintf(stderr, "\nERROR: EC2 jobs require a \"%s\" parameter\n", EC2AccessKeyId );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}
	
	if ( (tmp = condor_param( EC2SecretAccessKey, ATTR_EC2_SECRET_ACCESS_KEY )) ) {
		// check private key file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open private key file %s (%s)\n", 
							 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);

			StatInfo si(full_path(tmp));
			if (si.IsDirectory()) {
				fprintf(stderr, "\nERROR: %s is a directory\n", full_path(tmp));
				exit(1);
			}
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SECRET_ACCESS_KEY, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "ec2" ) == 0 ) {
		fprintf(stderr, "\nERROR: EC2 jobs require a \"%s\" parameter\n", EC2SecretAccessKey );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	bool bKeyPairPresent=false;

	// EC2KeyPair is not a necessary parameter
	if( (tmp = condor_param( EC2KeyPair, ATTR_EC2_KEY_PAIR )) ||
	    (tmp = condor_param( EC2KeyPairAlt, ATTR_EC2_KEY_PAIR )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_KEY_PAIR, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
		bKeyPairPresent=true;
	}

	// EC2KeyPairFile is not a necessary parameter
	if( (tmp = condor_param( EC2KeyPairFile, ATTR_EC2_KEY_PAIR_FILE )) ||
	    (tmp = condor_param( EC2KeyPairFileAlt, ATTR_EC2_KEY_PAIR_FILE )) ) {
	    if (bKeyPairPresent)
	    {
	      fprintf(stderr, "\nWARNING: EC2 job(s) contain both ec2_keypair && ec2_keypair_file, ignoring ec2_keypair_file\n");
	    }
	    else
	    {
	      // for the relative path, the keypair output file will be written to the IWD
	      buffer.formatstr( "%s = \"%s\"", ATTR_EC2_KEY_PAIR_FILE, full_path(tmp) );
	      free( tmp );
	      InsertJobExpr( buffer.Value() );
	    }
	}

	// Optional.
	if( (tmp = condor_param( EC2SecurityGroups, ATTR_EC2_SECURITY_GROUPS )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SECURITY_GROUPS, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// Optional.
	if( (tmp = condor_param( EC2SecurityIDs, ATTR_EC2_SECURITY_IDS )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SECURITY_IDS, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	if ( (tmp = condor_param( EC2AmiID, ATTR_EC2_AMI_ID )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_AMI_ID, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "ec2" ) == 0 ) {
		fprintf(stderr, "\nERROR: EC2 jobs require a \"%s\" parameter\n", EC2AmiID );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}
	
	// EC2InstanceType is not a necessary parameter
	if( (tmp = condor_param( EC2InstanceType, ATTR_EC2_INSTANCE_TYPE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_INSTANCE_TYPE, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
	
	// EC2VpcSubnet is not a necessary parameter
	if( (tmp = condor_param( EC2VpcSubnet, ATTR_EC2_VPC_SUBNET )) ) {
		buffer.formatstr( "%s = \"%s\"",ATTR_EC2_VPC_SUBNET , tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
	
	// EC2VpcIP is not a necessary parameter
	if( (tmp = condor_param( EC2VpcIP, ATTR_EC2_VPC_IP )) ) {
		buffer.formatstr( "%s = \"%s\"",ATTR_EC2_VPC_IP , tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
		
	// EC2ElasticIP is not a necessary parameter
    if( (tmp = condor_param( EC2ElasticIP, ATTR_EC2_ELASTIC_IP )) ) {
        buffer.formatstr( "%s = \"%s\"", ATTR_EC2_ELASTIC_IP, tmp );
        free( tmp );
        InsertJobExpr( buffer.Value() );
    }
	
	bool HasAvailabilityZone=false;
	// EC2AvailabilityZone is not a necessary parameter
    if( (tmp = condor_param( EC2AvailabilityZone, ATTR_EC2_AVAILABILITY_ZONE )) ) {
        buffer.formatstr( "%s = \"%s\"", ATTR_EC2_AVAILABILITY_ZONE, tmp );
        free( tmp );
        InsertJobExpr( buffer.Value() );
		HasAvailabilityZone=true;
    }
	
	// EC2EBSVolumes is not a necessary parameter
    if( (tmp = condor_param( EC2EBSVolumes, ATTR_EC2_EBS_VOLUMES )) ) {
		if( validate_disk_param(tmp, 2, 2) == false ) 
        {
			fprintf(stderr, "\nERROR: 'ec2_ebs_volumes' has incorrect format.\n"
					"The format shoud be like "
					"\"<instance_id>:<devicename>\"\n"
					"e.g.> For single volume: ec2_ebs_volumes = vol-35bcc15e:hda1\n"
					"      For multiple disks: ec2_ebs_volumes = "
					"vol-35bcc15e:hda1,vol-35bcc16f:hda2\n");
			DoCleanup(0,0,NULL);
			exit(1);
		}
		
		if (!HasAvailabilityZone)
		{
			fprintf(stderr, "\nERROR: 'ec2_ebs_volumes' requires 'ec2_availability_zone'\n");
			DoCleanup(0,0,NULL);
			exit(1);
		}
		
        buffer.formatstr( "%s = \"%s\"", ATTR_EC2_EBS_VOLUMES, tmp );
        free( tmp );
        InsertJobExpr( buffer.Value() );
    }

	// EC2SpotPrice is not a necessary parameter
	if( (tmp = condor_param( EC2SpotPrice, ATTR_EC2_SPOT_PRICE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SPOT_PRICE, tmp);
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// EC2BlockDeviceMapping is not a necessary parameter
	if( (tmp = condor_param( EC2BlockDeviceMapping, ATTR_EC2_BLOCK_DEVICE_MAPPING )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_BLOCK_DEVICE_MAPPING, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
		bKeyPairPresent=true;
	}

	// EC2UserData is not a necessary parameter
	if( (tmp = condor_param( EC2UserData, ATTR_EC2_USER_DATA )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_USER_DATA, tmp);
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// EC2UserDataFile is not a necessary parameter
	if( (tmp = condor_param( EC2UserDataFile, ATTR_EC2_USER_DATA_FILE )) ) {
		// check user data file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open user data file %s (%s)\n", 
								 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_USER_DATA_FILE, 
				full_path(tmp) );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// You can only have one IAM [Instance] Profile, so you can only use
	// one of the ARN or the Name.
	bool bIamProfilePresent = false;
	if( (tmp = condor_param( EC2IamProfileArn, ATTR_EC2_IAM_PROFILE_ARN )) ) {
		bIamProfilePresent = true;
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_IAM_PROFILE_ARN, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	}

	if( (tmp = condor_param( EC2IamProfileName, ATTR_EC2_IAM_PROFILE_NAME )) ) {
		if( bIamProfilePresent ) {
			fprintf( stderr, "\nWARNING: EC2 job(s) contain both %s and %s; ignoring %s.\n", EC2IamProfileArn, EC2IamProfileName, EC2IamProfileName );
		} else {
			buffer.formatstr( "%s = \"%s\"", ATTR_EC2_IAM_PROFILE_NAME, tmp );
			InsertJobExpr( buffer.Value() );
		}
		free( tmp );
	}

	//
	// Handle arbitrary EC2 RunInstances parameters.
	//
	StringList paramNames;
	if( (tmp = condor_param( EC2ParamNames, ATTR_EC2_PARAM_NAMES )) ) {
		paramNames.initializeFromString( tmp );
		free( tmp );
	}

	unsigned prefixLength = strlen( EC2ParamPrefix );
	HASHITER smsIter = hash_iter_begin( SubmitMacroSet );
	for( ; ! hash_iter_done( smsIter ); hash_iter_next( smsIter ) ) {
		const char * key = hash_iter_key( smsIter );

		if( strcasecmp( key, EC2ParamNames ) == 0 ) {
			continue;
		}

		if( strncasecmp( key, EC2ParamPrefix, prefixLength ) != 0 ) {
			continue;
		}

		const char * paramName = &key[prefixLength];
		const char * paramValue = hash_iter_value( smsIter );
		buffer.formatstr( "%s_%s = \"%s\"", ATTR_EC2_PARAM_PREFIX, paramName, paramValue );
		InsertJobExpr( buffer.Value() );
		set_condor_param_used( key );

		bool found = false;
		paramNames.rewind();
		char * existingPN = NULL;
		while( (existingPN = paramNames.next()) != NULL ) {
			std::string converted = existingPN;
			std::replace( converted.begin(), converted.end(), '.', '_' );
			if( strcasecmp( converted.c_str(), paramName ) == 0 ) {
				found = true;
				break;
			}
		}
		if( ! found ) {
			paramNames.append( paramName );
		}
	}
	hash_iter_delete( & smsIter );

	if( ! paramNames.isEmpty() ) {
		char * paramNamesStr = paramNames.print_to_delimed_string( ", " );
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_PARAM_NAMES, paramNamesStr );
		free( paramNamesStr );
		InsertJobExpr( buffer.Value() );
	}


		//
		// Handle EC2 tags - don't require user to specify the list of tag names
		//
		// Collect all the EC2 tag names, then param for each
		//
		// EC2TagNames is needed because EC2 tags are case-sensitive
		// and ClassAd attribute names are not. We build it for the
		// user, but also let the user override entries in it with
		// their own case preference. Ours will always be lower-cased.
		//

	StringList tagNames;
	if ((tmp = condor_param(EC2TagNames, ATTR_EC2_TAG_NAMES))) {
		tagNames.initializeFromString(tmp);
		free(tmp); tmp = NULL;
	}

	HASHITER it = hash_iter_begin(SubmitMacroSet);
	int prefix_len = strlen(ATTR_EC2_TAG_PREFIX);
	for (;!hash_iter_done(it); hash_iter_next(it)) {
		const char *key = hash_iter_key(it);
		const char *name = NULL;
		if (!strncasecmp(key, ATTR_EC2_TAG_PREFIX, prefix_len) &&
			key[prefix_len]) {
			name = &key[prefix_len];
		} else if (!strncasecmp(key, "ec2_tag_", 8) &&
				   key[8]) {
			name = &key[8];
		} else {
			continue;
		}

		if (strncasecmp(name, "Names", 5) &&
			!tagNames.contains_anycase(name)) {
			tagNames.append(name);
		}
	}
	hash_iter_delete(&it);

	std::stringstream ss;
	char *tagName;
	tagNames.rewind();
	while ((tagName = tagNames.next())) {
			// XXX: Check that tagName does not contain an equal sign (=)
		std::string tag;
		std::string tagAttr(ATTR_EC2_TAG_PREFIX); tagAttr.append(tagName);
		std::string tagCmd("ec2_tag_"); tagCmd.append(tagName);
		char *value = NULL;
		if ((value = condor_param(tagCmd.c_str(), tagAttr.c_str()))) {
			buffer.formatstr("%s = \"%s\"", tagAttr.c_str(), value);
			InsertJobExpr(buffer.Value());
			free(value); value = NULL;
		} else {
				// XXX: Should never happen, we just searched for the names, error or something
		}
	}

		// For compatibility with the AWS Console, set the Name tag to
		// be the executable, which is just a label for EC2 jobs
	tagNames.rewind();
	if (!tagNames.contains_anycase("Name")) {
		if (JobUniverse == CONDOR_UNIVERSE_GRID &&
			JobGridType != NULL &&
			(strcasecmp( JobGridType, "ec2" ) == MATCH)) {
			bool wantsNameTag = condor_param_bool( "WantNameTag", NULL, true );
			if( wantsNameTag ) {
				char *ename = condor_param(Executable, ATTR_JOB_CMD); // !NULL by now
				tagNames.append("Name");
				buffer.formatstr("%sName = \"%s\"", ATTR_EC2_TAG_PREFIX, ename);
				InsertJobExpr(buffer);
				free(ename); ename = NULL;
			}
		}
	}

	if ( !tagNames.isEmpty() ) {
		buffer.formatstr("%s = \"%s\"",
					ATTR_EC2_TAG_NAMES, tagNames.print_to_delimed_string(","));
		InsertJobExpr(buffer.Value());
	}

	if ( (tmp = condor_param( BoincAuthenticatorFile,
							  ATTR_BOINC_AUTHENTICATOR_FILE )) ) {
		// check authenticator file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open authenticator file %s (%s)\n", 
								 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_BOINC_AUTHENTICATOR_FILE,
						  full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "boinc" ) == 0 ) {
		fprintf(stderr, "\nERROR: BOINC jobs require a \"%s\" parameter\n", BoincAuthenticatorFile );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	//
	// GCE grid-type submit attributes
	//
	if ( (tmp = condor_param( GceAuthFile, ATTR_GCE_AUTH_FILE )) ) {
		// check auth file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open auth file %s (%s)\n", 
						 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);

			StatInfo si(full_path(tmp));
			if (si.IsDirectory()) {
				fprintf(stderr, "\nERROR: %s is a directory\n", full_path(tmp));
				exit(1);
			}
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_AUTH_FILE, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "gce" ) == 0 ) {
		fprintf(stderr, "\nERROR: GCE jobs require a \"%s\" parameter\n", GceAuthFile );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( (tmp = condor_param( GceImage, ATTR_GCE_IMAGE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_IMAGE, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "gce" ) == 0 ) {
		fprintf(stderr, "\nERROR: GCE jobs require a \"%s\" parameter\n", GceImage );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( (tmp = condor_param( GceMachineType, ATTR_GCE_MACHINE_TYPE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_MACHINE_TYPE, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && strcasecmp( JobGridType, "gce" ) == 0 ) {
		fprintf(stderr, "\nERROR: GCE jobs require a \"%s\" parameter\n", GceMachineType );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	// GceMetadata is not a necessary parameter
	// This is a comma-separated list of name/value pairs
	if( (tmp = condor_param( GceMetadata, ATTR_GCE_METADATA )) ) {
		StringList list( tmp, "," );
		char *list_str = list.print_to_string();
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_METADATA, list_str );
		InsertJobExpr( buffer.Value() );
		free( list_str );
	}

	// GceMetadataFile is not a necessary parameter
	if( (tmp = condor_param( GceMetadataFile, ATTR_GCE_METADATA_FILE )) ) {
		// check metadata file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open metadata file %s (%s)\n", 
								 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_METADATA_FILE, 
				full_path(tmp) );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// GcePreemptible is not a necessary parameter
	if( (tmp = condor_param( GcePreemptible, ATTR_GCE_PREEMPTIBLE )) ) {
		buffer.formatstr( "%s = %s", ATTR_GCE_PREEMPTIBLE, isTrue(tmp) ? "True" : "False" );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	}

	// GceJsonFile is not a necessary parameter
	if( (tmp = condor_param( GceJsonFile, ATTR_GCE_JSON_FILE )) ) {
		// check json file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open json file %s (%s)\n",
								 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);
		}
		InsertJobExprString( ATTR_GCE_JSON_FILE, full_path( tmp ) );
		free( tmp );
	}

	// CREAM clients support an alternate representation for resources:
	//   host.edu:8443/cream-batchname-queuename
	// Transform this representation into our regular form:
	//   host.edu:8443/ce-cream/services/CREAM2 batchname queuename
	if ( JobGridType != NULL && strcasecmp (JobGridType, "cream") == MATCH ) {
		tmp = condor_param( GridResource, ATTR_GRID_RESOURCE );
		MyString resource = tmp;
		free( tmp );

		int pos = resource.FindChar( ' ', 0 );
		if ( pos >= 0 && resource.FindChar( ' ', pos + 1 ) < 0 ) {
			int pos2 = resource.find( "://", pos + 1 );
			if ( pos2 < 0 ) {
				pos2 = pos + 1;
			} else {
				pos2 = pos2 + 3;
			}
			if ( ( pos = resource.find( "/cream-", pos2 ) ) >= 0 ) {
				// We found the shortened form
				resource.replaceString( "/cream-", "/ce-cream/services/CREAM2 ", pos );
				pos += 26;
				if ( ( pos2 = resource.find( "-", pos ) ) >= 0 ) {
					resource.setChar( pos2, ' ' );
				}

				buffer.formatstr( "%s = \"%s\"", ATTR_GRID_RESOURCE,
								resource.Value() );
				InsertJobExpr( buffer );
			}
		}
	}
}

void
SetGSICredentials()
{
	char *tmp;
	MyString buffer;
	bool use_proxy = false;

		// Find the X509 user proxy
		// First param for it in the submit file. If it's not there
		// and the job type requires an x509 proxy (globus, nordugrid),
		// then check the usual locations (as defined by GSI) and
		// bomb out if we can't find it.

	char *proxy_file = condor_param( X509UserProxy );
	tmp = condor_param( UseX509UserProxy );
	if ( tmp ) {
		if( tmp[0] == 'T' || tmp[0] == 't' ) {
			use_proxy = true;
		}
		free( tmp );
	}

	if ( JobUniverse == CONDOR_UNIVERSE_GRID &&
		 JobGridType != NULL &&
		 (strcasecmp (JobGridType, "gt2") == MATCH ||
		  strcasecmp (JobGridType, "gt5") == MATCH ||
		  strcasecmp (JobGridType, "cream") == MATCH ||
		  strcasecmp (JobGridType, "nordugrid") == MATCH) ) {

		use_proxy = true;
	}

	if ( proxy_file == NULL && use_proxy ) {

		proxy_file = get_x509_proxy_filename();
		if ( proxy_file == NULL ) {

			fprintf( stderr, "\nERROR: Can't determine proxy filename\n" );
			fprintf( stderr, "X509 user proxy is required for this job.\n");
			exit (1);
		}
	}

	if (proxy_file != NULL) {
		if ( proxy_file[0] == '#' ) {
			buffer.formatstr( "%s=\"%s\"", ATTR_X509_USER_PROXY_SUBJECT, 
						   &proxy_file[1]);
			InsertJobExpr(buffer);	

//			(void) buffer.sprintf( "%s=\"%s\"", ATTR_X509_USER_PROXY, 
//						   proxy_file);
//			InsertJobExpr(buffer);	
			free( proxy_file );
		} else {
			char *full_proxy_file = strdup( full_path( proxy_file ) );
			free( proxy_file );
			proxy_file = full_proxy_file;
#if defined(HAVE_EXT_GLOBUS)
// this code should get torn out at some point (8.7.0) since the SchedD now
// manages these attributes securely and the values provided by submit should
// not be trusted.  in the meantime, though, we try to provide some cross
// compatibility between the old and new.  i also didn't indent this properly
// so as not to churn the old code.  -zmiller

		bool submit_sends_x509 = true;
		CondorVersionInfo cvi(MySchedd ? MySchedd->version() : NULL);
		if (cvi.built_since_version(8, 5, 8)) {
			submit_sends_x509 = false;
		}

		if(submit_sends_x509) {

			if ( check_x509_proxy(proxy_file) != 0 ) {
				fprintf( stderr, "\nERROR: %s\n", x509_error_string() );
				exit( 1 );
			}

			/* Insert the proxy expiration time into the ad */
			time_t proxy_expiration;
			proxy_expiration = x509_proxy_expiration_time(proxy_file);
			if (proxy_expiration == -1) {
				fprintf( stderr, "\nERROR: %s\n", x509_error_string() );
				exit( 1 );
			}

			(void) buffer.formatstr( "%s=%li", ATTR_X509_USER_PROXY_EXPIRATION, 
						   proxy_expiration);
			InsertJobExpr(buffer);	
	

			/* Insert the proxy subject name into the ad */
			char *proxy_subject;
			proxy_subject = x509_proxy_identity_name(proxy_file);

			if ( !proxy_subject ) {
				fprintf( stderr, "\nERROR: %s\n", x509_error_string() );
				exit( 1 );
			}

			(void) buffer.formatstr( "%s=\"%s\"", ATTR_X509_USER_PROXY_SUBJECT, 
						   proxy_subject);
			InsertJobExpr(buffer);	
			free( proxy_subject );

			/* Insert the proxy email into the ad */
			char *proxy_email;
			proxy_email = x509_proxy_email(proxy_file);

			if ( proxy_email ) {
				InsertJobExprString(ATTR_X509_USER_PROXY_EMAIL, proxy_email);
				free( proxy_email );
			}

			/* Insert the VOMS attributes into the ad */
			char *voname = NULL;
			char *firstfqan = NULL;
			char *quoted_DN_and_FQAN = NULL;

			int error = extract_VOMS_info_from_file( proxy_file, 0, &voname, &firstfqan, &quoted_DN_and_FQAN);
			if ( error ) {
				if (error == 1) {
					// no attributes, skip silently.
				} else {
					// log all other errors
					fprintf( stderr, "\nWARNING: unable to extract VOMS attributes (proxy: %s, erro: %i). continuing \n", proxy_file, error );
				}
			} else {
				InsertJobExprString(ATTR_X509_USER_PROXY_VONAME, voname);	
				free( voname );

				InsertJobExprString(ATTR_X509_USER_PROXY_FIRST_FQAN, firstfqan);	
				free( firstfqan );

				InsertJobExprString(ATTR_X509_USER_PROXY_FQAN, quoted_DN_and_FQAN);	
				free( quoted_DN_and_FQAN );
			}

			// When new classads arrive, all this should be replaced with a
			// classad holding the VOMS atributes.  -zmiller

		}
// this is the end of the big, not-properly indented block (see above) that
// causes submit to send the x509 attributes only when talking to older
// schedds.  at some point, probably 8.7.0, this entire block should be ripped
// out. -zmiller
#endif

			(void) buffer.formatstr( "%s=\"%s\"", ATTR_X509_USER_PROXY, 
						   proxy_file);
			InsertJobExpr(buffer);	
			free( proxy_file );
		}
	}

	tmp = condor_param(DelegateJobGSICredentialsLifetime,ATTR_DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME);
	if( tmp ) {
		char *endptr=NULL;
		int lifetime = strtol(tmp,&endptr,10);
		if( !endptr || *endptr != '\0' ) {
			fprintf(stderr,"\nERROR: invalid integer setting %s = %s\n",DelegateJobGSICredentialsLifetime,tmp);
			exit( 1 );
		}
		InsertJobExprInt(ATTR_DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME,lifetime);
		free(tmp);
	}

	//ckireyev: MyProxy-related crap
	if ((tmp = condor_param (ATTR_MYPROXY_HOST_NAME))) {
		buffer.formatstr(  "%s = \"%s\"", ATTR_MYPROXY_HOST_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_SERVER_DN))) {
		buffer.formatstr(  "%s = \"%s\"", ATTR_MYPROXY_SERVER_DN, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_PASSWORD))) {
		if (myproxy_password == NULL) {
			myproxy_password = tmp;
		}
	}

	if ((tmp = condor_param (ATTR_MYPROXY_CRED_NAME))) {
		buffer.formatstr(  "%s = \"%s\"", ATTR_MYPROXY_CRED_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if (myproxy_password) {
		buffer.formatstr(  "%s = %s", ATTR_MYPROXY_PASSWORD, myproxy_password);
		InsertJobExpr (buffer);
	}

	if ((tmp = condor_param (ATTR_MYPROXY_REFRESH_THRESHOLD))) {
		buffer.formatstr(  "%s = %s", ATTR_MYPROXY_REFRESH_THRESHOLD, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_NEW_PROXY_LIFETIME))) { 
		buffer.formatstr(  "%s = %s", ATTR_MYPROXY_NEW_PROXY_LIFETIME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	// END MyProxy-related crap
}

void
SetSendCredential()
{
	if (!sent_credential_to_credd) {
		return;
	}

	// add it to the job ad (starter needs to know this value)
	MyString buffer;
	(void) buffer.formatstr( "%s = True", ATTR_JOB_SEND_CREDENTIAL);
	InsertJobExpr(buffer);
}


#if !defined(WIN32)
// this allocates memory, free() it when you're done.
char*
findKillSigName( const char* submit_name, const char* attr_name )
{
	char *sig = condor_param( submit_name, attr_name );
	char *signame = NULL;
	const char *tmp;
	int signo;

	if (sig) {
		signo = atoi(sig);
		if( signo ) {
				// looks like they gave us an actual number, map that
				// into a string for the classad:
			tmp = signalName( signo );
			if( ! tmp ) {
				fprintf( stderr, "\nERROR: invalid signal %s\n", sig );
				exit( 1 );
			}
			signame = strdup( tmp );
		} else {
				// should just be a string, let's see if it's valid:
			signo = signalNumber( sig );
			if( signo == -1 ) {
				fprintf( stderr, "\nERROR: invalid signal %s\n", sig );
				exit( 1 );
			}
				// cool, just use what they gave us.
			signame = strupr(sig);
		}
	}
	return signame;
}


void
SetKillSig()
{
	char* sig_name;
	char* timeout;
	MyString buffer;

	sig_name = findKillSigName( KillSig, ATTR_KILL_SIG );
	if( ! sig_name ) {
		switch(JobUniverse) {
		case CONDOR_UNIVERSE_STANDARD:
			sig_name = strdup( "SIGTSTP" );
			break;
		case CONDOR_UNIVERSE_VANILLA:
			// Don't define sig_name for Vanilla Universe
			sig_name = NULL;
			break;
		default:
			sig_name = strdup( "SIGTERM" );
			break;
		}
	}

	if ( sig_name ) {
		buffer.formatstr( "%s=\"%s\"", ATTR_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
	}

	sig_name = findKillSigName( RmKillSig, ATTR_REMOVE_KILL_SIG );
	if( sig_name ) {
		buffer.formatstr( "%s=\"%s\"", ATTR_REMOVE_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
		sig_name = NULL;
	}

	sig_name = findKillSigName( HoldKillSig, ATTR_HOLD_KILL_SIG );
	if( sig_name ) {
		buffer.formatstr( "%s=\"%s\"", ATTR_HOLD_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
		sig_name = NULL;
	}

	timeout = condor_param( KillSigTimeout, ATTR_KILL_SIG_TIMEOUT );
	if( timeout ) {
		buffer.formatstr( "%s=%d", ATTR_KILL_SIG_TIMEOUT, atoi(timeout) );
		InsertJobExpr( buffer );
		free( timeout );
		sig_name = NULL;
	}
}
#endif  // of ifndef WIN32

#if 1
  // qslice etc now in condor_utils...
#else
class qslice {
public:
	qslice() : flags(0), start(0), end(0), step(0) {}
	qslice(int _start) : flags(1|2), start(_start), end(0), step(0) {}
	qslice(int _start, int _end) : flags(1|2|4), start(_start), end(_end), step(0) {}
	~qslice() {}
	// set the slice by parsing a string [x:y:z], where
	// the enclosing [] are required
	// x,y & z are integers, y and z are optional
	char *set(char* str) {
		flags = 0;
		if (*str == '[') {
			char * p = str;
			char * pend=NULL;
			flags |= 1;
			int val = (int)strtol(p+1, &pend, 10);
			if ( ! pend || (*pend != ':' && *pend != ']')) { flags = 0; return str; }
			start = val; if (pend > p+1) flags |= 2;
			p = pend;
			if (*p == ']') return p;
			val = (int)strtol(p+1, &pend, 10);
			if ( ! pend || (*pend != ':' && *pend != ']')) { flags = 0; return str; }
			end = val; if (pend > p+1) flags |= 4;
			p = pend;
			if (*p == ']') return p;
			val = (int)strtol(p+1, &pend, 10);
			if ( ! pend || *pend != ']') { flags = 0; return str; }
			step = val; if (pend > p+1) flags |= 8;
			return pend+1;
		}
		return str;
	}
	bool  initialized() { return flags & 1; }

	// convert ix based on slice start & step, returns true if translated ix is within slice start and length.
	// input ix is assumed to be 0 based and increasing.
	bool translate(int & ix, int len) {
		if (!(flags & 1)) return ix >= 0 && ix < len;
		int im = (flags&8) ? step : 1;
		if (im <= 0) {
			ASSERT(0); // TODO: implement negative iteration.
		} else {
			int is = 0;   if (flags&2) { is = (start < 0) ? start+len : start; }
			int ie = len; if (flags&4) { ie = is + ((end < 0) ? end+len : end); }
			int iy = is + (ix*im);
			ix = iy;
			return ix >= is && ix < ie;
		}
	}

	// check to see if ix is selected for by the slice. negative iteration is ignored 
	bool selected(int ix, int len) {
		if (!(flags&1)) return ix >= 0 && ix < len;
		int is = 0; if (flags&2) { is = (start < 0) ? start+len : start; }
		int ie = len; if (flags&4) { ie = (end < 0) ? end+len : end; }
		return ix >= is && ix < ie && ( !(flags&8) || (0 == ((ix-is) % step)) );
	}

	int to_string(char * buf, int cch) {
		char sz[16*3];
		if ( ! (flags&1)) return 0;
		char * p = sz;
		*p++  = '[';
		if (flags&2) { p += sprintf(p,"%d", start); }
		*p++ = ':';
		if (flags&4) { p += sprintf(p,"%d", end); }
		*p++ = ':';
		if (flags&8) { p += sprintf(p,"%d", step); }
		*p++ = ']';
		*p = 0;
		strncpy(buf, sz, cch); buf[cch-1] = 0;
		return (int)(p - sz);
	}

private:
	int flags; // 1==initialized, 2==start set, 4==length set, 8==step set
	int start;
	int end;
	int step;
};
#endif

struct _qtoken { const char * name; int id; };

// scan for a keyword from set of tokens, the keywords must be surrounded by whitespace
// or terminated by (.  if a token is found token_id is set, otherwise it is left untouched.
// the return value is a pointer to where scanning left off.
char * queue_token_scan(char * ptr, const struct _qtoken tokens[], int ctokens, char** pptoken, int & token_id, bool scan_until_match)
{
	char *ptok = NULL;   // pointer to start of current token in pqargs when scanning for keyword
	char tokenbuf[sizeof("matching")+1] = ""; // temporary buffer to hold a potential keyword while scanning
	int  maxtok = (int)sizeof(tokenbuf)-1;
	int  cchtok = 0;

	char * p = ptr;

	while (*p) {
		int ch = *p;
		if (isspace(ch) || ch == '(') {
			if (cchtok >= 1 && cchtok <= maxtok) {
				tokenbuf[cchtok] = 0;
				int ix = 0;
				for (ix = 0; ix < ctokens; ++ix) {
					if (MATCH == strcasecmp(tokenbuf, tokens[ix].name))
						break;
				}
				if (ix < ctokens) { token_id = tokens[ix].id; *pptoken = ptok; break; }
			}
			if ( ! scan_until_match) { *pptoken = ptok; break; }
			cchtok = 0;
		} else {
			if ( ! cchtok) { ptok = p; }
			if (cchtok < maxtok) { tokenbuf[cchtok] = ch; }
			++cchtok;
		}
		++p;
	}

	return p;
}

#if 0 // this is now in submit_utils
enum {
	foreach_not=0,
	foreach_in,
	foreach_from,
	foreach_matching,
	foreach_matching_files,
	foreach_matching_dirs,
	foreach_matching_any,
};
#endif

enum {
	PARSE_ERROR_INVALID_QNUM_EXPR = -2,
	PARSE_ERROR_QNUM_OUT_OF_RANGE = -3,
	PARSE_ERROR_UNEXPECTED_KEYWORD = -4,
	PARSE_ERROR_BAD_SLICE = -5,
};

// parse a the arguments for a Queue statement. this will be of the form
//
//    [<num-expr>] [[<var>[,<var2>]] in|from|matching [<slice>][<tokening>] (<items>)]
// 
//  {} indicates optional, <> indicates argument type rather than literal text, | is either or
//
//  <num-expr> is any classad expression that parses to an int it defines the number of
//             procs to queue per item in <items>.  If not present 1 is used.
//  <var>      is a variable name, case insensitive, case preserving, must begin with alpha and contain only alpha, numbers and _
//  in|from|matching  only one of these case-insensitive keywords may occur, these control interpretation of <items>
//  <slice>    is a python style slice controlling the start,end & step through the <items>
//  <tokening> arguments that control tokenizing <items>.
//  <items>    is a list of items to iterate and queue. the () surrounding items are optional, if they exist then
//             items may span multiple lines, in which case the final ) must be on a line by itself.
//
// The basic parsing strategy is:
//    1) find the in|from|matching keyword by scanning for whitespace or ( delimited words
//       if NOT FOUND, set end_num to end of input string and goto step 5 (because the whole input is <num-expr>)
//       else set end_num to start of keyword.
//    2) parse forwards from end of keyword looking for (
//       if no ( is found, parse remainder of line as single-line itemlist
//       if ( is found look to see if last char on line is )
//          if found both ( and ) parse content as a single-line itemlist
//          else set items_filename appropriately based on keyword.
//    3) FUTURE WORK: parse characters between keyword and ( as <slice> and <tokening>
//    4) parse backwards from start of keyword while you see valid VAR,VAR2,etc (basically look for bare numbers or non-alphanum chars)
//       set end_num to first char that cannot be VAR,VAR2
//       if VARS found, parse into vars stringlist.
///   4) eval from start of line to end_num and set queue_num.
//
int parse_queue_args (
	char * pqargs,      // in:  queue line, THIS MAY BE MODIFIED!! \0 will be inserted to delimit things
	int & foreach_mode, // out: the mode of operation for foreach
	int & queue_num,    // out: the count of processes to queue for each item
	StringList& vars,   // out: user defined variable names
	StringList& items,  // out: list of items to iterate over
	qslice & slice,     // out: may be initialized to slice if "[]" is parsed.
	MyString & items_filename) // out: file to read list of items from, returns "<" to indicate list should be read from submit file.
{
	foreach_mode = foreach_not;
	vars.clearAll();
	items.clearAll();
	items_filename.clear();

	// empty queue args means queue 1
	if ( ! *pqargs) {
		queue_num = 1;
		return 0;
	}

	char *p = pqargs;    // pointer to current char while scanning
	char *ptok = NULL;   // pointer to start of current token in pqargs when scanning for keyword
	static const struct _qtoken foreach_tokens[] = { {"in", foreach_in }, {"from", foreach_from}, {"matching", foreach_matching} };
	p = queue_token_scan(p, foreach_tokens, COUNTOF(foreach_tokens), &ptok, foreach_mode, true);

	// for now assume that p points to the end of the queue count expression.
	char * pnum_end = p;

	// if we found a in,from, or matching keyword. we use that to anchor the rest of the parse
	// before the keyword is the optional vars list, and before that is the optional queue count.
	// after the keyword is the start of the items list.
	if (foreach_mode != foreach_not) {
		// if we found a keyword, then p points to the start of the itemlist
		// and we have to scan backwords to find the end of the queue count.
		while (*p && isspace(*p)) ++p;

		// check for qualifiers after the foreach keyword
		if (*p != '(') {
			static const struct _qtoken quals[] = { {"files", 1 }, {"dirs", 2}, {"any", 3} };
			for (;;) {
				char * p2 = p;
				int qual = -1;
				char * ptmp = NULL;
				p2 = queue_token_scan(p2, quals, COUNTOF(quals), &ptmp, qual, false);
				if (ptmp && *ptmp == '[') { qual = 4; }
				if (qual <= 0)
					break;

				switch (qual)
				{
				case 1:
					if (foreach_mode == foreach_matching) foreach_mode = foreach_matching_files;
					else return PARSE_ERROR_UNEXPECTED_KEYWORD;
					break;

				case 2:
					if (foreach_mode == foreach_matching) foreach_mode = foreach_matching_dirs;
					else return PARSE_ERROR_UNEXPECTED_KEYWORD;
					break;

				case 3:
					if (foreach_mode == foreach_matching) foreach_mode = foreach_matching_any;
					else return PARSE_ERROR_UNEXPECTED_KEYWORD;
					break;

				case 4:
					p2 = slice.set(ptmp);
					if ( ! slice.initialized()) return PARSE_ERROR_BAD_SLICE;
					if (*p2 == ']') ++p2;
					break;

				default:
					break;
				}

				if (p == p2) break; // just in case we don't advance.
				p = p2;
				while (*p && isspace(*p)) ++p;
			}
		}

		// parse the itemlist. this can be all on a single line, or spanning multiple lines.
		// we only parse the first line here, and set items_filename to indicate how to proceed
		char * plist = p;
		bool one_line_list = false;
		if (*plist == '(') {
			int cch = strlen(plist);
			if (plist[cch-1] == ')') { plist[cch-1] = 0; ++plist; one_line_list = true; }
		}
		if (*plist == '(') {
			// this is a multiline list, if it is to be read from fp_submit, we can't do that here 
			// because reading from fp_submit will invalidate pqargs. instead we append whatever we
			// find on the current line, and then set the filename to "<" to tell the caller to read
			// the remainder from the submit file.
			++plist;
			while (isspace(*plist)) ++plist;
			if (*plist) {
				if (foreach_mode == foreach_from) {
					items.append(plist);
				} else {
					items.initializeFromString(plist);
				}
			}
			items_filename = "<";
		} else if (foreach_mode == foreach_from) {
			while (isspace(*plist)) ++plist;
			if (one_line_list) {
				items.append(plist);
			} else {
				items_filename = plist;
				items_filename.trim();
			}
		} else {
			while (isspace(*plist)) ++plist;
			items.initializeFromString(plist);
		}

		// trim trailing whitespace before the in,from, or matching keyword.
		char * pvars = ptok;
		while (pvars > pqargs && isspace(pvars[-1])) { --pvars; }

		// walk backwards until we get to something that can't be loop variable.
		// so we scan backwards over alpha, command and space, but only scan over
		// numbers if they are preceeded by alpha. this allows variables to be VAR1 but not 1VAR
		if (pvars > pqargs) {
			*pvars = 0; // null terminate the vars
			char * pt = pvars;
			while (pt > pqargs) {
				char ch = pt[-1];
				if (isdigit(ch)) {
					--pt;
					while (pt > pqargs) {
						ch = pt[-1];
						if (isalpha(ch)) { break; }
						if ( ! isdigit(ch)) { ch = '!'; break; } // force break out of outer loop
						--pt;
					}
				}
				if (isspace(ch) || isalpha(ch) || ch == ',' || ch == '_' || ch == '.') {
					pvars = pt-1;
				} else {
					break;
				}
				--pt;
			}
			// pvars should now point to a null-terminated string that is the var set.
			vars.initializeFromString(pvars);
		}
		// whatever remains from pvars to the start of the args is the queue count.
		pnum_end = pvars;
	}

	// parse the queue count.
	while (pnum_end > pqargs && isspace(pnum_end[-1])) { --pnum_end; }
	if (pnum_end > pqargs) {
		*pnum_end = 0;
		long long value = -1;
		if ( ! string_is_long_param(pqargs, value)) {
			return PARSE_ERROR_INVALID_QNUM_EXPR;
		} else if (value < 0 || value >= INT_MAX) {
			return PARSE_ERROR_QNUM_OUT_OF_RANGE;
		}
		queue_num = (int)value;
	} else {
		queue_num = 1;
	}

	return 0;
}

#endif // USE_SUBMIT_UTILS

// Check to see if this is a queue statement, if it is, return a pointer to the queue arguments.
// 
const char * is_queue_statement(const char * line)
{
	const int cchQueue = sizeof("queue")-1;
	if (starts_with_ignore_case(line, "queue") && (0 == line[cchQueue] || isspace(line[cchQueue]))) {
		const char * pqargs = line+cchQueue;
		while (*pqargs && isspace(*pqargs)) ++pqargs;
		return pqargs;
	}
	return NULL;
}

#ifdef USE_SUBMIT_UTILS

int iterate_queue_foreach(int queue_num, StringList & vars, StringList & items, qslice & slice)
{
	int rval;
	const char* token_seps = ", \t";
	const char* token_ws = " \t";

	int queue_item_opts = 0;
	if (submit_hash.submit_param_bool("SubmitWarnEmptyFields", "submit_warn_empty_fields", true)) {
		queue_item_opts |= QUEUE_OPT_WARN_EMPTY_FIELDS;
	}
	if (submit_hash.submit_param_bool("SubmitFailEmptyFields", "submit_fail_empty_fields", false)) {
		queue_item_opts |= QUEUE_OPT_FAIL_EMPTY_FIELDS;
	}

	if (queue_num > 0) {
		if ( ! MyQ) {
			int rval = queue_connect();
			if (rval < 0)
				return rval;
		}

		rval = queue_begin(vars, NewExecutable || (ClusterId < 0)); // called before iterating items
		if (rval < 0)
			return rval;

		char * item = NULL;
		if (items.isEmpty()) {
			rval = queue_item(queue_num, vars, item, 0, queue_item_opts, token_seps, token_ws);
		} else {
			int citems = items.number();
			items.rewind();
			int item_index = 0;
			while ((item = items.next())) {
				if (slice.selected(item_index, citems)) {
					rval = queue_item(queue_num, vars, item, item_index, queue_item_opts, token_seps, token_ws);
					if (rval < 0)
						break;
				}
				++item_index;
			}
		}

		queue_end(vars, false);
		if (rval < 0)
			return rval;
	}

	return 0;
}

#endif

MyString last_submit_executable;
MyString last_submit_cmd;

int SpecialSubmitPreQueue(const char* queue_args, bool from_file, MACRO_SOURCE& source, MACRO_SET& macro_set, std::string & errmsg)
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");
	int rval = 0;

	GotQueueCommand = true;

	// parse the extra lines before doing $ expansion on the queue line
	ErrContext.phase = PHASE_DASH_APPEND;
	ExtraLineNo = 0;
	extraLines.Rewind();
	const char * exline;
	while (extraLines.Next(exline)) {
		++ExtraLineNo;
		rval = Parse_config_string(source, 1, exline, macro_set, ctx);
		if (rval < 0)
			return rval;
		rval = 0;
	}
	ExtraLineNo = 0;
	ErrContext.phase = PHASE_QUEUE;
	ErrContext.step = -1;

	if (DashDryRun && DumpSubmitHash) {
		auto_free_ptr expanded_queue_args(expand_macro(queue_args, macro_set, ctx));
		char * pqargs = expanded_queue_args.ptr();
		ASSERT(pqargs);
		fprintf(stdout, "\n----- Queue arguments -----\nSpecified: %s\nExpanded : %s", queue_args, pqargs);
	}

	if ( ! queueCommandLine.empty() && from_file) {
		errmsg = "-queue argument conflicts with queue statement in submit file";
		return -1;
	}

	// HACK! the queue function uses a global flag to know whether or not to ask for a new cluster
	// In 8.2 this flag is set whenever the "executable" or "cmd" value is set in the submit file.
	// As of 8.3, we don't believe this is still necessary, but we are afraid to change it. This
	// code is *mostly* the same, but will differ in cases where the users is setting cmd or executble
	// to the *same* value between queue statements. - hopefully this is close enough.
#ifdef USE_SUBMIT_UTILS
	MyString cur_submit_executable(lookup_macro_exact_no_default(SUBMIT_KEY_Executable, macro_set, 0));
#else
	MyString cur_submit_executable(lookup_macro_exact_no_default(Executable, macro_set, 0));
#endif
	if (last_submit_executable != cur_submit_executable) {
		NewExecutable = true;
		last_submit_executable = cur_submit_executable;
	}
	MyString cur_submit_cmd(lookup_macro_exact_no_default("cmd", macro_set, 0));
	if (last_submit_cmd != cur_submit_cmd) {
		NewExecutable = true;
		last_submit_cmd = cur_submit_cmd;
	}

	return rval;
}

int SpecialSubmitPostQueue()
{
	if (dash_interactive && !InteractiveSubmitFile) {
		// for interactive jobs, just one queue statement
		// a return of 1 tells it to stop scanning, but is not an error.
		return 1;
	}
	ErrContext.phase = PHASE_READ_SUBMIT;
	return 0;
}

// this gets called while parsing the submit file to process lines
// that don't look like valid key=value pairs.  That *should* only be queue lines for now.
// return 0 to keep scanning the file, ! 0 to stop scanning. non-zero return values will
// be passed back out of Parse_macros
//
int SpecialSubmitParse(void* pv, MACRO_SOURCE& source, MACRO_SET& macro_set, char * line, std::string & errmsg)
{
	FILE* fp_submit = (FILE*)pv;
	int rval = 0;
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");

	// Check to see if this is a queue statement.
	//
	const char * queue_args = is_queue_statement(line);
	if (queue_args) {

		rval = SpecialSubmitPreQueue(queue_args, fp_submit!=NULL, source, macro_set, errmsg);
		if (rval) {
			return rval;
		}

		auto_free_ptr expanded_queue_args(expand_macro(queue_args, macro_set, ctx));
		char * pqargs = expanded_queue_args.ptr();
		ASSERT(pqargs);

		// set glob expansion options from submit statements.
		int expand_options = 0;
#ifdef USE_SUBMIT_UTILS
		if (submit_hash.submit_param_bool("SubmitWarnEmptyMatches", "submit_warn_empty_matches", true)) {
			expand_options |= EXPAND_GLOBS_WARN_EMPTY;
		}
		if (submit_hash.submit_param_bool("SubmitFailEmptyMatches", "submit_fail_empty_matches", false)) {
			expand_options |= EXPAND_GLOBS_FAIL_EMPTY;
		}
		if (submit_hash.submit_param_bool("SubmitWarnDuplicateMatches", "submit_warn_duplicate_matches", true)) {
			expand_options |= EXPAND_GLOBS_WARN_DUPS;
		}
		if (submit_hash.submit_param_bool("SubmitAllowDuplicateMatches", "submit_allow_duplicate_matches", false)) {
			expand_options |= EXPAND_GLOBS_ALLOW_DUPS;
		}
		char* parm = submit_hash.submit_param("SubmitMatchDirectories", "submit_match_directories");
#else
		if (condor_param_bool("SubmitWarnEmptyMatches", "submit_warn_empty_matches", true)) {
			expand_options |= EXPAND_GLOBS_WARN_EMPTY;
		}
		if (condor_param_bool("SubmitFailEmptyMatches", "submit_fail_empty_matches", false)) {
			expand_options |= EXPAND_GLOBS_FAIL_EMPTY;
		}
		if (condor_param_bool("SubmitWarnDuplicateMatches", "submit_warn_duplicate_matches", true)) {
			expand_options |= EXPAND_GLOBS_WARN_DUPS;
		}
		if (condor_param_bool("SubmitAllowDuplicateMatches", "submit_allow_duplicate_matches", false)) {
			expand_options |= EXPAND_GLOBS_ALLOW_DUPS;
		}
		char* parm = condor_param("SubmitMatchDirectories", "submit_match_directories");
#endif
		if (parm) {
			if (MATCH == strcasecmp(parm, "never") || MATCH == strcasecmp(parm, "no") || MATCH == strcasecmp(parm, "false")) {
				expand_options |= EXPAND_GLOBS_TO_FILES;
			} else if (MATCH == strcasecmp(parm, "only")) {
				expand_options |= EXPAND_GLOBS_TO_DIRS;
			} else if (MATCH == strcasecmp(parm, "yes") || MATCH == strcasecmp(parm, "true")) {
				// nothing to do.
			} else {
				errmsg = parm;
				errmsg += " is not a valid value for SubmitMatchDirectories";
				return -1;
			}
			free(parm); parm = NULL;
		}

#ifdef USE_SUBMIT_UTILS
#else
		int queue_item_opts = 0;
		if (condor_param_bool("SubmitWarnEmptyFields", "submit_warn_empty_fields", true)) {
			queue_item_opts |= QUEUE_OPT_WARN_EMPTY_FIELDS;
		}
		if (condor_param_bool("SubmitFailEmptyFields", "submit_fail_empty_fields", false)) {
			queue_item_opts |= QUEUE_OPT_FAIL_EMPTY_FIELDS;
		}
#endif

		// skip whitespace before queue arguments (if any)
		while (isspace(*pqargs)) ++pqargs;

#ifdef USE_SUBMIT_UTILS
		SubmitForeachArgs o;
		int queue_modifier = 1;
		int citems = -1;
		// parse the queue arguments, handling the count and finding the in,from & matching keywords
		// on success pqargs will point to to \0 or to just after the keyword.
		rval = o.parse_queue_args(pqargs);
		queue_modifier = o.queue_num;
		if (rval < 0) {
			errmsg = "invalid Queue statement";
			return rval;
		}

		// if no loop variable specified, but a foreach mode is used. use "Item" for the loop variable.
		if (o.vars.isEmpty() && (o.foreach_mode != foreach_not)) { o.vars.append("Item"); }

		if ( ! o.items_filename.empty()) {
			if (o.items_filename == "<") {
				if ( ! fp_submit) {
					errmsg = "unexpected error while attempting to read queue items from submit file.";
					return -1;
				}
				// read items from submit file until we see the closing brace on a line by itself.
				bool saw_close_brace = false;
				int item_list_begin_line = source.line;
				for(char * line=NULL; ; ) {
					line = getline_trim(fp_submit, source.line);
					if ( ! line) break; // null indicates end of file
					if (line[0] == '#') continue; // skip comments.
					if (line[0] == ')') { saw_close_brace = true; break; }
					if (o.foreach_mode == foreach_from) {
						o.items.append(line);
					} else {
						o.items.initializeFromString(line);
					}
				}
				if ( ! saw_close_brace) {
					formatstr(errmsg, "Reached end of file without finding closing brace ')'"
						" for Queue command on line %d", item_list_begin_line);
					return -1;
				}
			} else if (o.items_filename == "-") {
				int lineno = 0;
				for (char* line=NULL;;) {
					line = getline_trim(stdin, lineno);
					if ( ! line) break;
					if (o.foreach_mode == foreach_from) {
						o.items.append(line);
					} else {
						o.items.initializeFromString(line);
					}
				}
			} else {
				MACRO_SOURCE ItemsSource;
				FILE * fp = Open_macro_source(ItemsSource, o.items_filename.Value(), false, macro_set, errmsg);
				if ( ! fp) {
					return -1;
				}
				for (char* line=NULL;;) {
					line = getline_trim(fp, ItemsSource.line);
					if ( ! line) break;
					o.items.append(line);
				}
				rval = Close_macro_source(fp, ItemsSource, macro_set, 0);
			}
		}

		switch (o.foreach_mode) {
		case foreach_in:
		case foreach_from:
			// itemlist is already correct
			// PRAGMA_REMIND("do argument validation here?")
			citems = o.items.number();
			break;

		case foreach_matching:
		case foreach_matching_files:
		case foreach_matching_dirs:
		case foreach_matching_any:
			if (o.foreach_mode == foreach_matching_files) {
				expand_options &= ~EXPAND_GLOBS_TO_DIRS;
				expand_options |= EXPAND_GLOBS_TO_FILES;
			} else if (o.foreach_mode == foreach_matching_dirs) {
				expand_options &= ~EXPAND_GLOBS_TO_FILES;
				expand_options |= EXPAND_GLOBS_TO_DIRS;
			} else if (o.foreach_mode == foreach_matching_any) {
				expand_options &= ~(EXPAND_GLOBS_TO_FILES|EXPAND_GLOBS_TO_DIRS);
			}
			citems = submit_expand_globs(o.items, expand_options, errmsg);
			if ( ! errmsg.empty()) {
				fprintf(stderr, "\n%s: %s", citems >= 0 ? "WARNING" : "ERROR", errmsg.c_str());
				errmsg.clear();
			}
			if (citems < 0) return citems;
			break;

		default:
		case foreach_not:
			// to simplify the loop below, set a single empty item into the itemlist.
			citems = 1;
			break;
		}
#else
		StringList vars;
		StringList items;
		qslice     slice;
		MyString   items_filename;
		const char* token_seps = ", \t";
		const char* token_ws = " \t";
		int queue_modifier = 1;
		int foreach_mode = foreach_not;
		int citems = -1;

		// parse the queue arguments, handling the count and finding the in,from & matching keywords
		// on success pqargs will point to to \0 or to just after the keyword.
		rval = parse_queue_args(pqargs, foreach_mode, queue_modifier, vars, items, slice, items_filename);
		if (rval < 0) {
			errmsg = "invalid Queue statement";
			return rval;
		}

		// if no loop variable specified, but a foreach mode is used. use "Item" for the loop variable.
		if (vars.isEmpty() && (foreach_mode != foreach_not)) { vars.append("Item"); }

		if ( ! items_filename.empty()) {
			if (items_filename == "<") {
				if ( ! fp_submit) {
					errmsg = "unexpected error while attempting to read queue items from submit file.";
					return -1;
				}
				// read items from submit file until we see the closing brace on a line by itself.
				bool saw_close_brace = false;
				int item_list_begin_line = source.line;
				for(char * line=NULL; ; ) {
					line = getline_trim(fp_submit, source.line);
					if ( ! line) break; // null indicates end of file
					if (line[0] == '#') continue; // skip comments.
					if (line[0] == ')') { saw_close_brace = true; break; }
					if (foreach_mode == foreach_from) {
						items.append(line);
					} else {
						items.initializeFromString(line);
					}
				}
				if ( ! saw_close_brace) {
					formatstr(errmsg, "Reached end of file without finding closing brace ')'"
						" for Queue command on line %d", item_list_begin_line);
					return -1;
				}
			} else if (items_filename == "-") {
				int lineno = 0;
				for (char* line=NULL;;) {
					line = getline_trim(stdin, lineno);
					if ( ! line) break;
					if (foreach_mode == foreach_from) {
						items.append(line);
					} else {
						items.initializeFromString(line);
					}
				}
			} else {
				MACRO_SOURCE ItemsSource;
				FILE * fp = Open_macro_source(ItemsSource, items_filename.Value(), false, SubmitMacroSet, errmsg);
				if ( ! fp) {
					return -1;
				}
				for (char* line=NULL;;) {
					line = getline_trim(fp, ItemsSource.line);
					if ( ! line) break;
					items.append(line);
				}
				rval = Close_macro_source(fp, ItemsSource, SubmitMacroSet, 0);
			}
		}

		switch (foreach_mode) {
		case foreach_in:
		case foreach_from:
			// itemlist is already correct
			// PRAGMA_REMIND("do argument validation here?")
			citems = items.number();
			break;

		case foreach_matching:
		case foreach_matching_files:
		case foreach_matching_dirs:
		case foreach_matching_any:
			if (foreach_mode == foreach_matching_files) {
				expand_options &= ~EXPAND_GLOBS_TO_DIRS;
				expand_options |= EXPAND_GLOBS_TO_FILES;
			} else if (foreach_mode == foreach_matching_dirs) {
				expand_options &= ~EXPAND_GLOBS_TO_FILES;
				expand_options |= EXPAND_GLOBS_TO_DIRS;
			} else if (foreach_mode == foreach_matching_any) {
				expand_options &= ~(EXPAND_GLOBS_TO_FILES|EXPAND_GLOBS_TO_DIRS);
			}
			citems = submit_expand_globs(items, expand_options, errmsg);
			if ( ! errmsg.empty()) {
				fprintf(stderr, "\n%s: %s", citems >= 0 ? "WARNING" : "ERROR", errmsg.c_str());
				errmsg.clear();
			}
			if (citems < 0) return citems;
			break;

		default:
		case foreach_not:
			// to simplify the loop below, set a single empty item into the itemlist.
			citems = 1;
			break;
		}
#endif

		if (queue_modifier > 0 && citems > 0) {
		#ifdef USE_SUBMIT_UTILS
			iterate_queue_foreach(queue_modifier, o.vars, o.items, o.slice);
		#else
			if ( ! MyQ) {
				int rval = queue_connect();
				if (rval < 0)
					return rval;
			}

			rval = queue_begin(vars, NewExecutable || (ClusterId < 0)); // called before iterating items
			if (rval < 0)
				return rval;

			char * item = NULL;
			if (items.isEmpty()) {
				rval = queue_item(queue_modifier, vars, item, 0, queue_item_opts, token_seps, token_ws);
			} else {
				items.rewind();
				int item_index = 0;
				while ((item = items.next())) {
					if (slice.selected(item_index, citems)) {
						rval = queue_item(queue_modifier, vars, item, item_index, queue_item_opts, token_seps, token_ws);
						if (rval < 0)
							break;
					}
					++item_index;
				}
			}

			queue_end(vars, false);
			if (rval < 0)
				return rval;
		#endif
		}

		rval = SpecialSubmitPostQueue();
		if (rval)
			return rval;
		return 0; // keep scanning
	}
	return -1;
}

int read_submit_file(FILE * fp)
{
	ErrContext.phase = PHASE_READ_SUBMIT;

	std::string errmsg;
#ifdef USE_SUBMIT_UTILS
  #ifdef USE_HALTING_PARSER
	int rval = 0;
	while (rval == 0) {
		char * qline = NULL;
		rval = submit_hash.parse_file_up_to_q_line(fp, FileMacroSource, errmsg, &qline);
		if (rval || ! qline)
			break;

		const char * queue_args = is_queue_statement(qline);
		if ( ! queue_args)
			break;

		// Do last minute things that happen just before the queue statement
		// NOTE: it is by design that the ClusterId can only change once per queue statement
		// even if the queue iteration changes the "cmd". It's important for the job factory
		// that the cluster does not change.
		rval = SpecialSubmitPreQueue(queue_args, true, FileMacroSource, submit_hash.macros(), errmsg);
		if (rval)
			break;

		SubmitForeachArgs o;
		rval = submit_hash.parse_q_args(queue_args, o, errmsg);
		if (rval)
			break;

		rval = submit_hash.load_q_foreach_items(fp, FileMacroSource, o, errmsg);
		if (rval)
			break;

		rval = iterate_queue_foreach(o.queue_num, o.vars, o.items, o.slice);
		if (rval)
			break;

		rval = SpecialSubmitPostQueue();
		if (rval) {
			// a return of 1 means stop scanning, but i
			if (rval == 1) rval = 0;
			break;
		}
	}
  #else
	int rval = submit_hash.parse_file(fp, FileMacroSource, errmsg, SpecialSubmitParse, fp);
  #endif
#else
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");

	MacroStreamYourFile ms(fp, FileMacroSource);
	int rval = Parse_macros(ms,
		0, SubmitMacroSet, READ_MACROS_SUBMIT_SYNTAX,
		&ctx, errmsg,
		SpecialSubmitParse, fp);
#endif

	if( rval < 0 ) {
		fprintf (stderr, "\nERROR: on Line %d of submit file: %s\n", FileMacroSource.line, errmsg.c_str());
#ifdef USE_SUBMIT_UTILS
		if (submit_hash.error_stack()) {
			std::string errstk(submit_hash.error_stack()->getFullText());
			if ( ! errstk.empty()) {
				fprintf(stderr, "%s", errstk.c_str());
			}
			submit_hash.error_stack()->clear();
		}
#else
		if (SubmitMacroSet.errors) {
			fprintf(stderr, "%s", SubmitMacroSet.errors->getFullText().c_str());
			SubmitMacroSet.errors->clear();
		}
#endif
	} else {
		ErrContext.phase = PHASE_QUEUE_ARG;

		// if a -queue command line option was specified, and the submit file did not have a queue statement
		// then parse the command line argument now (as if it was appended to the submit file)
		if ( rval == 0 && ! GotQueueCommand && ! queueCommandLine.empty()) {
			char * line = strdup(queueCommandLine.c_str()); // copy cmdline so we can destructively parse it.
			ASSERT(line);
#ifdef USE_SUBMIT_UTILS
			rval = submit_hash.process_q_line(FileMacroSource, line, errmsg, SpecialSubmitParse, NULL);
#else
			rval = SpecialSubmitParse(NULL, FileMacroSource, SubmitMacroSet, line, errmsg);
#endif
			free(line);
		}
		if (rval < 0) {
			fprintf (stderr, "\nERROR: while processing -queue command line option: %s\n", errmsg.c_str());
		}
	}

	return rval;
}

#ifdef USE_SUBMIT_UTILS
#else
char *
condor_param( const char* name)
{
	return condor_param(name, NULL);
}

void param_and_insert_unique_items(const char * param_name, classad::References & attrs)
{
	auto_free_ptr value(param(param_name));
	if (value) {
		StringTokenIterator it(value);
		const std::string * item;
		while ((item = it.next_string())) {
			attrs.insert(*item);
		}
	}
}

char *
condor_param( const char* name, const char* alt_name )
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT",3);

	bool used_alt = false;
	const char *pval = lookup_macro(name, SubmitMacroSet, ctx);
	char * pval_expanded = NULL;

#ifdef SUBMIT_ATTRS_IS_ALSO_CONDOR_PARAM
	// TODO: change this to use the defaults table from SubmitMacroSet
	static classad::References submit_attrs;
	static bool submit_attrs_initialized = false;
	if ( ! submit_attrs_initialized) {
		param_and_insert_unique_items("SUBMIT_ATTRS", submit_attrs);
		param_and_insert_unique_items("SUBMIT_EXPRS", submit_attrs);
		param_and_insert_unique_items("SYSTEM_SUBMIT_ATTRS", submit_attrs);
		submit_attrs_initialized = true;
	}
#endif

	if( ! pval && alt_name ) {
		pval = lookup_macro(alt_name, SubmitMacroSet, ctx);
		used_alt = true;
	}

	if( ! pval ) {
#ifdef SUBMIT_ATTRS_IS_ALSO_CONDOR_PARAM // for (broken) legacy behavior
			// if the value isn't in the submit file, check in the
			// submit_exprs list and use that as a default.  
		if ( ! submit_attrs.empty()) {
			if (submit_attrs.find(name) != submit_attrs.end()) {
				return param(name);
			}
			if (alt_name && (submit_attrs.find(alt_name) != submit_attrs.end())) {
				return param(alt_name);
			}
		}
#endif
		return NULL;
	}

	ErrContext.macro_name = used_alt ? alt_name : name;
	ErrContext.raw_macro_val = pval;
	pval_expanded = expand_macro(pval, SubmitMacroSet, ctx);

	if( pval == NULL ) {
		fprintf( stderr, "\nERROR: Failed to expand macros in: %s\n",
				 used_alt ? alt_name : name );
		exit(1);
	}

	if( * pval_expanded == '\0' ) {
		free( pval_expanded );
		return NULL;
	}

	ErrContext.macro_name = NULL;
	ErrContext.raw_macro_val = NULL;

	return  pval_expanded;
}


void
set_condor_param( const char *name, const char *value )
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");
	insert_macro(name, value, SubmitMacroSet, DefaultMacro, ctx);
}
#endif

#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
  // To facilitate processing of job status from the
  // job_queue.log, the ATTR_JOB_STATUS attribute should not
  // be stored within the cluster ad. Instead, it should be
  // directly part of each job ad. This change represents an
  // increase in size for the job_queue.log initially, but
  // the ATTR_JOB_STATUS is guaranteed to be specialized for
  // each job so the two approaches converge. -mattf 1 June 09
  // tj 2015 - it also wastes space in the schedd, so we should probably stop doign this. 
static const char * no_cluster_attrs[] = {
	ATTR_JOB_STATUS,
};
bool IsNoClusterAttr(const char * name) {
	for (size_t ii = 0; ii < COUNTOF(no_cluster_attrs); ++ii) {
		if (MATCH == strcasecmp(name, no_cluster_attrs[ii]))
			return true;
	}
	return false;
}
#else
void SetNoClusterAttr(const char * name)
{
	NoClusterCheckAttrs.append( name );
}
#endif

#ifdef USE_SUBMIT_UTILS
#else

// stuff a live value into submit's hashtable.
// IT IS UP TO THE CALLER TO INSURE THAT live_value IS VALID FOR THE LIFE OF THE HASHTABLE
// this function is intended for use during queue iteration to stuff
// changing values like $(Cluster) and $(Process) into the hashtable.
// The pointer passed in as live_value may be changed at any time to
// affect subsequent macro expansion of name.
// live values are automatically marked as 'used'.
//
void
set_live_submit_variable( const char *name, const char *live_value, bool force_used /*=true*/ )
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");
	MACRO_ITEM* pitem = find_macro_item(name, NULL, SubmitMacroSet);
	if ( ! pitem) {
		insert_macro(name, "", SubmitMacroSet, LiveMacro, ctx);
		pitem = find_macro_item(name, NULL, SubmitMacroSet);
	}
	ASSERT(pitem);
	pitem->raw_value = live_value;
	if (SubmitMacroSet.metat && force_used) {
		MACRO_META* pmeta = &SubmitMacroSet.metat[pitem - SubmitMacroSet.table];
		pmeta->use_count += 1;
	}
}

void
set_condor_param_used( const char *name ) 
{
	increment_macro_use_count(name, SubmitMacroSet);
}

int
strcmpnull(const char *str1, const char *str2)
{
	if( str1 && str2 ) {
		return strcmp(str1, str2);
	}
	return (str1 || str2);
}
#endif

void
connect_to_the_schedd()
{
	if ( ActiveQueueConnection ) {
		// we are already connected; do not connect again
		return;
	}

	setupAuthentication();

	CondorError errstack;
	ActualScheddQ *ScheddQ = new ActualScheddQ();
	if( ScheddQ->Connect(*MySchedd, errstack) == 0 ) {
		delete ScheddQ;
		if( ScheddName ) {
			fprintf( stderr, 
					"\nERROR: Failed to connect to queue manager %s\n%s\n",
					 ScheddName, errstack.getFullText(true).c_str() );
		} else {
			fprintf( stderr, 
				"\nERROR: Failed to connect to local queue manager\n%s\n",
				errstack.getFullText(true).c_str() );
		}
		exit(1);
	}

	MyQ = ScheddQ;
	ActiveQueueConnection = TRUE;
}


int queue_connect()
{
	if ( ! ActiveQueueConnection)
	{
		if (DumpClassAdToFile || DashDryRun) {
			SimScheddQ* SimQ = new SimScheddQ();
			if (DumpFileIsStdout) {
				SimQ->Connect(stdout, false, false);
			} else if (DashDryRun) {
				FILE* dryfile = NULL;
				bool  free_it = false;
				if (MATCH == strcmp(DashDryRunOutName, "-")) {
					dryfile = stdout; free_it = false;
				} else {
					dryfile = safe_fopen_wrapper_follow(DashDryRunOutName,"w");
					if ( ! dryfile) {
						fprintf( stderr, "\nERROR: Failed to open file -dry-run output file (%s)\n", strerror(errno));
						exit(1);
					}
					free_it = true;
				}
				SimQ->Connect(dryfile, free_it, (DashDryRun&2)!=0);
			} else {
				SimQ->Connect(DumpFile, true, false);
				DumpFile = NULL;
			}
			MyQ = SimQ;
		} else {
			connect_to_the_schedd();
		}
		ASSERT(MyQ);
	}
	ActiveQueueConnection = MyQ != NULL;
	return MyQ ? 0 : -1;
}

#ifdef USE_SUBMIT_UTILS
// buffers used while processing the queue statement to inject $(Cluster) and $(Process) into the submit hash table.
static char ClusterString[20]="1", ProcessString[20]="0", EmptyItemString[] = "";
MACRO_ITEM* find_submit_item(const char * name) { return submit_hash.lookup_exact(name); }
#define set_live_submit_variable submit_hash.set_live_submit_variable
#else
MACRO_ITEM* find_submit_item(const char * name) { return find_macro_item(name, NULL, SubmitMacroSet); }
#endif

int queue_begin(StringList & vars, bool new_cluster)
{
	if ( ! MyQ)
		return -1;

	// establish live buffers for $(Cluster) and $(Process), and other loop variables
	// Because the user might already be using these variables, we can only set the explicit ones
	// unconditionally, the others we must set only when not already set by the user.
#ifdef USE_SUBMIT_UTILS
	set_live_submit_variable(SUBMIT_KEY_Cluster, ClusterString);
	set_live_submit_variable(SUBMIT_KEY_Process, ProcessString);
#else
	set_live_submit_variable(Cluster, ClusterString);
	set_live_submit_variable(Process, ProcessString);
#endif
	
	vars.rewind();
	char * var;
	while ((var = vars.next())) { set_live_submit_variable(var, EmptyItemString, false); }

	// optimize the macro set for lookups if we inserted anything.  we expect this to happen only once.
#ifdef USE_SUBMIT_UTILS
	submit_hash.optimize();
#else
	if (SubmitMacroSet.sorted < SubmitMacroSet.size) {
		optimize_macros(SubmitMacroSet);
	}
#endif

	if (new_cluster) {
		// if we have already created the maximum number of clusters, error out now.
		if (DashMaxClusters > 0 && ClustersCreated >= DashMaxClusters) {
			fprintf(stderr, "\nERROR: Number of submitted clusters would exceed %d and -single-cluster was specified\n", DashMaxClusters);
			exit(1);
		}

		if ((ClusterId = MyQ->get_NewCluster()) < 0) {
			fprintf(stderr, "\nERROR: Failed to create cluster\n");
			if ( ClusterId == -2 ) {
				fprintf(stderr,
					"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
			}
			exit(1);
		}

		++ClustersCreated;

#ifdef USE_SUBMIT_UTILS
		delete gClusterAd; gClusterAd = NULL;
#else
		// We only need to call init_job_ad the second time we see
		// a new Executable, because we call init_job_ad() in main()
		if ( !IsFirstExecutable ) {
			init_job_ad();
		}
		IsFirstExecutable = false;
#endif
	}

	if (DashDryRun && DumpSubmitHash) {
		fprintf(stdout, "\n----- submit hash at queue begin -----\n");
		int flags = (DumpSubmitHash & 8) ? HASHITER_SHOW_DUPS : 0;
#ifdef USE_SUBMIT_UTILS
		submit_hash.dump(stdout, flags);
#else
		HASHITER it = hash_iter_begin(SubmitMacroSet, flags);
		for ( ; ! hash_iter_done(it); hash_iter_next(it)) {
			const char * key = hash_iter_key(it);
			if (key && key[0] == '$') continue; // dont dump meta params.
			const char * val = hash_iter_value(it);
			//fprintf(stdout, "%s%s = %s\n", it.is_def ? "d " : "  ", key, val ? val : "NULL");
			fprintf(stdout, "  %s = %s\n", key, val ? val : "NULL");
		}
		hash_iter_delete(&it);
#endif
		fprintf(stdout, "-----\n");
	}

	return 0;
}

// queue N for a single item from the foreach itemlist.
// if there is no item list (i.e the old Queue N syntax) then item will be NULL.
//
int queue_item(int num, StringList & vars, char * item, int item_index, int options, const char * delims, const char * ws)
{
	ErrContext.item_index = item_index;

	if ( ! MyQ)
		return -1;

	int rval = 0; // default to success (if num == 0 we will return this)
	bool check_empty = options & (QUEUE_OPT_WARN_EMPTY_FIELDS|QUEUE_OPT_FAIL_EMPTY_FIELDS);
	bool fail_empty = options & QUEUE_OPT_FAIL_EMPTY_FIELDS;

	// UseStrict = condor_param_bool("Submit.IsStrict", "Submit_IsStrict", UseStrict);

	// If there are loop variables, destructively tokenize item and stuff the tokens into the submit hashtable.
	if ( ! vars.isEmpty())  {

		if ( ! item) {
			item = EmptyItemString;
			EmptyItemString[0] = '\0';
		}

		// set the first loop variable unconditionally, we set it initially to the whole item
		// we may later truncate that item when we assign fields to other loop variables.
		vars.rewind();
		char * var = vars.next();
		char * data = item;
		set_live_submit_variable(var, data, false);

		// if there is more than a single loop variable, then assign them as well
		// we do this by destructively null terminating the item for each var
		// the last var gets all of the remaining item text (if any)
		while ((var = vars.next())) {
			// scan for next token separator
			while (*data && ! strchr(delims, *data)) ++data;
			// null terminate the previous token and advance to the start of the next token.
			if (*data) {
				*data++ = 0;
				// skip leading separators and whitespace
				while (*data && strchr(ws, *data)) ++data;
				set_live_submit_variable(var, data, false);
			}
		}

		if (check_empty) {
			vars.rewind();
			std::string empties;
			while ((var = vars.next())) {
				MACRO_ITEM* pitem = find_submit_item(var);
				if ( ! pitem || (pitem->raw_value != EmptyItemString && 0 == strlen(pitem->raw_value))) {
					if ( ! empties.empty()) empties += ",";
					empties += var;
				}
			}
			if ( ! empties.empty()) {
				fprintf (stderr, "\n%s: Empty value(s) for %s at ItemIndex=%d!", fail_empty ? "ERROR" : "WARNING", empties.c_str(), item_index);
				if (fail_empty) {
					return -4;
				}
			}
		}
	}

	if (DashDryRun && DumpSubmitHash) {
		fprintf(stdout, "----- submit hash changes for ItemIndex = %d -----\n", item_index);
		char * var;
		vars.rewind();
		while ((var = vars.next())) {
			MACRO_ITEM* pitem = find_submit_item(var);
			fprintf (stdout, "  %s = %s\n", var, pitem ? pitem->raw_value : "");
		}
		fprintf(stdout, "-----\n");
	}

	/* queue num jobs */
	for (int ii = 0; ii < num; ++ii) {
	#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
	#else
		NoClusterCheckAttrs.clearAll();
	#endif
		ErrContext.step = ii;

		if ( ClusterId == -1 ) {
			fprintf( stderr, "\nERROR: Used queue command without "
						"specifying an executable\n" );
			exit(1);
		}

		// if we have already created the maximum number of jobs, error out now.
		if (DashMaxJobs > 0 && JobsCreated >= DashMaxJobs) {
			fprintf(stderr, "\nERROR: Number of submitted jobs would exceed -maxjobs (%d)\n", DashMaxJobs);
			DoCleanup(0,0,NULL);
			exit(1);
		}

		ProcId = MyQ->get_NewProc (ClusterId);

		if ( ProcId < 0 ) {
			fprintf(stderr, "\nERROR: Failed to create proc\n");
			if ( ProcId == -2 ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
			} else if( ProcId == -3 ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_PER_OWNER\n");
			} else if( ProcId == -4 ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_PER_SUBMISSION\n");
			}
			DoCleanup(0,0,NULL);
			exit(1);
		}

		if (MaxProcsPerCluster && ProcId >= MaxProcsPerCluster) {
			fprintf(stderr, "\nERROR: number of procs exceeds SUBMIT_MAX_PROCS_IN_CLUSTER\n");
			DoCleanup(0,0,NULL);
			exit(-1);
		}

		// update the live $(Cluster), $(Process) and $(Step) string buffers
		(void)sprintf(ClusterString, "%d", ClusterId);
		(void)sprintf(ProcessString, "%d", ProcId);
#ifdef USE_SUBMIT_UTILS
#else
		(void)sprintf(RowString, "%d", item_index);
		(void)sprintf(StepString, "%d", ii);
#endif

		// we move this outside the above, otherwise it appears that we have 
		// received no queue command (or several, if there were multiple ones)
		GotNonEmptyQueueCommand = 1;

#ifdef USE_SUBMIT_UTILS
		ClassAd * job = submit_hash.make_job_ad(
			JOB_ID_KEY(ClusterId,ProcId),
			item_index, ii,
			dash_interactive, dash_remote,
			check_sub_file, NULL);
		if ( ! job) {
			print_errstack(stderr, submit_hash.error_stack());
			DoCleanup(0,0,NULL);
			exit(1);
		}

		int JobUniverse = submit_hash.getUniverse();
		SendLastExecutable(); // if spooling the exe, send it now.
		SetSendCredentialInAd( job );
		NewExecutable = false;
		// write job ad to schedd or dump to file, depending on what type MyQ is
		rval = SendJobAd(job, gClusterAd);
#else

#if !defined(WIN32)
		SetRootDir();	// must be called very early
#endif
		SetIWD();		// must be called very early

		if (NewExecutable || (JobUniverse == CONDOR_UNIVERSE_MIN)) {
			SetUniverse();
		}
		SetExecutable();
		NewExecutable = false;

		SetDescription();
		SetMachineCount();

		// We couldn't set NODE until we knew this was a parallel universe job because dagman also uses NODE
		// ParallelNodeString defaults to #pArAlLeLnOdE# for universe MPI, we need it to expand as "#MpInOdE#" instead.
		if (JobUniverse == CONDOR_UNIVERSE_MPI || JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
			if (JobUniverse == CONDOR_UNIVERSE_MPI) { strcpy(ParallelNodeString, "#MpInOdE#"); }
			NodeMacroDef.psz = ParallelNodeString;
			//set_live_submit_variable("Node", ParallelNodeString);
		}

		// Note: Due to the unchecked use of global variables everywhere in
		// condor_submit, the order that we call the below Set* functions
		// is very important!
		SetJobStatus();
		SetPriority();
		SetMaxJobRetirementTime();
		SetEnvironment();
		SetNotification();
		SetWantRemoteIO();
		SetNotifyUser();
		SetEmailAttributes();
		SetRemoteInitialDir();
		SetExitRequirements();
		SetOutputDestination();
		SetWantGracefulRemoval();
		SetJobMaxVacateTime();

        // really a command, needs to happen before any calls to check_open
		SetJobDisableFileChecks();

		SetUserLog();
		SetUserLogXML();
		SetCoreSize();
#if !defined(WIN32)
		SetKillSig();
#endif
		SetRank();
		SetStdFile( 0 );
		SetStdFile( 1 );
		SetStdFile( 2 );
		SetFileOptions();
		SetFetchFiles();
		SetCompressFiles();
		SetAppendFiles();
		SetLocalFiles();
		SetEncryptExecuteDir();
		SetTDP();			// before SetTransferFile() and SetRequirements()
		SetTransferFiles();	 // must be called _before_ SetImageSize() 
		SetRunAsOwner();
        SetLoadProfile();
		SetPerFileEncryption();  // must be called _before_ SetRequirements()
		SetImageSize();		// must be called _after_ SetTransferFiles()
        SetRequestResources();

		SetSimpleJobExprs();

			//
			// SetCronTab() must be called before SetJobDeferral()
			// Both of these need to be called before SetRequirements()
			//
		SetCronTab();
		SetJobDeferral();
		SetJobRetries();
		
			//
			// Must be called _after_ SetTransferFiles(), SetJobDeferral(),
			// SetCronTab(), and SetPerFileEncryption()
			//
		SetRequirements();	
		
		SetJobLease();		// must be called _after_ SetStdFile(0,1,2)

		SetRemoteAttrs();
		SetJobMachineAttrs();

		SetPeriodicHoldCheck();
		SetPeriodicRemoveCheck();
		SetNoopJob();
		SetNoopJobExitSignal();
		SetNoopJobExitCode();
		SetLeaveInQueue();
		SetArguments();
		SetGridParams();
		SetGSICredentials();
		SetSendCredential();
		SetMatchListLen();
		SetDAGNodeName();
		SetDAGManJobId();
		SetJarFiles();
		SetJavaVMArgs();
		SetParallelStartupScripts(); //JDB
		SetConcurrencyLimits();
		SetAccountingGroup();
		SetVMParams();
		SetLogNotes();
		SetUserNotes();
		SetStackSize();

			// This must come after all things that modify the input file list
		FixupTransferInputFiles();

			// SetForcedAttributes should be last so that it trumps values
			// set by normal submit attributes
		SetForcedAttributes();

		// write job ad to schedd or dump to file, depending on what type MyQ is
		rval = SaveClassAd();
#endif
		switch( rval ) {
		case 0:			/* Success */
		case 1:
			break;
		default:		/* Failed for some other reason... */
			fprintf( stderr, "\nERROR: Failed to queue job.\n" );
			exit(1);
		}

		++JobsCreated;
	
		if (verbose && ! DumpFileIsStdout) {
			fprintf(stdout, "\n** Proc %d.%d:\n", ClusterId, ProcId);
			fPrintAd (stdout, *job);
		} else if ( ! terse && ! DumpFileIsStdout && ! DumpSubmitHash) {
			fprintf(stdout, ".");
		}

		if (CurrentSubmitInfo == -1 || SubmitInfo[CurrentSubmitInfo].cluster != ClusterId) {
			CurrentSubmitInfo++;
			SubmitInfo[CurrentSubmitInfo].cluster = ClusterId;
			SubmitInfo[CurrentSubmitInfo].firstjob = ProcId;
		}
		SubmitInfo[CurrentSubmitInfo].lastjob = ProcId;

		// SubmitInfo[x].lastjob controls how many submit events we
		// see in the user log.  For parallel jobs, we only want
		// one "job" submit event per cluster, no matter how many 
		// Procs are in that it.  Setting lastjob to zero makes this so.

		if (JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
			SubmitInfo[CurrentSubmitInfo].lastjob = 0;
		}

#ifdef USE_SUBMIT_UTILS
		if ( ! gClusterAd) {
			gClusterAd = new ClassAd(*job);
			for (size_t ii = 0; ii < COUNTOF(no_cluster_attrs); ++ii) {
				gClusterAd->Delete(no_cluster_attrs[ii]);
			}
		}

		// If spooling entire job "sandbox" to the schedd, then we need to keep
		// the classads in an array to later feed into the filetransfer object.
		if ( dash_remote ) {
			ClassAd * tmp = new ClassAd(*job);
			tmp->Assign(ATTR_CLUSTER_ID, ClusterId);
			tmp->Assign(ATTR_PROC_ID, ProcId);
			if (0 == ProcId) {
				JobAdsArrayLastClusterIndex = JobAdsArrayLen;
			} else {
				// proc ad to cluster ad (if there is one)
				tmp->ChainToAd(JobAdsArray[JobAdsArrayLastClusterIndex]);
			}
			JobAdsArray[ JobAdsArrayLen++ ] = tmp;
			return true;
		}

		submit_hash.delete_job_ad();
		job = NULL;
#else
		if ( ProcId == 0 ) {
			delete ClusterAd;
			ClusterAd = new ClassAd( *job );

			// Remove attributes that were forced into the proc 0 ad
			// from our copy of the cluster ad.
		#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
			for (size_t ii = 0; ii < COUNTOF(no_cluster_attrs); ++ii) {
				ClusterAd->Delete(no_cluster_attrs[ii]);
			}
		#else
			const char *attr;
			NoClusterCheckAttrs.rewind();
			while ( (attr = NoClusterCheckAttrs.next()) ) {
				ClusterAd->Delete( attr );
			}
		#endif
		}

		if ( job_ad_saved == false ) {
			delete job;
		}
		job = new ClassAd();
		job_ad_saved = false;
#endif
	}

	ErrContext.step = -1;

	return rval;
}

void queue_end(StringList & vars, bool /*fEof*/)   // called when done processing each Queue statement and at end of submit file
{
	// set live submit variables to generate reasonable unused-item messages.
	if ( ! vars.isEmpty())  {
		vars.rewind();
		char * var = vars.next();
		while (var) {
			set_live_submit_variable(var, "<Queue_item>", false);
			var = vars.next();
		}
	}
}

#ifdef USE_SUBMIT_UTILS
#undef set_live_submit_variable
#else

bool
check_requirements( char const *orig, MyString &answer )
{
	bool	checks_opsys = false;
	bool	checks_arch = false;
	bool	checks_disk = false;
	bool	checks_cpus = false;
	bool	checks_mem = false;
	//bool	checks_reqmem = false;
	bool	checks_fsdomain = false;
	bool	checks_ckpt_arch = false;
	bool	checks_file_transfer = false;
	bool	checks_file_transfer_plugin_methods = false;
	bool	checks_per_file_encryption = false;
	bool	checks_mpi = false;
	bool	checks_tdp = false;
	bool	checks_encrypt_exec_dir = false;
#if defined(WIN32)
	bool	checks_credd = false;
#endif
	char	*ptr;
	MyString ft_clause;

	if( strlen(orig) ) {
		answer.formatstr( "(%s)", orig );
	} else {
		answer = "";
	}

	switch( JobUniverse ) {
	case CONDOR_UNIVERSE_VANILLA:
		ptr = param( "APPEND_REQ_VANILLA" );
		break;
	case CONDOR_UNIVERSE_STANDARD:
		ptr = param( "APPEND_REQ_STANDARD" );
		break;
	case CONDOR_UNIVERSE_VM:
		ptr = param( "APPEND_REQ_VM" );
		break;
	default:
		ptr = NULL;
		break;
	} 
	if( ptr == NULL ) {
			// Didn't find a per-universe version, try the generic,
			// non-universe specific one:
		ptr = param( "APPEND_REQUIREMENTS" );
	}

	if( ptr != NULL ) {
			// We found something to append.  
		if( answer.Length() ) {
				// We've already got something in requirements, so we
				// need to append an AND clause.
			answer += " && (";
		} else {
				// This is the first thing in requirements, so just
				// put this as the first clause.
			answer += "(";
		}
		answer += ptr;
		answer += ")";
		free( ptr );
	}

	if ( JobUniverse == CONDOR_UNIVERSE_GRID ) {
		// We don't want any defaults at all w/ Globus...
		// If we don't have a req yet, set to TRUE
		if ( answer[0] == '\0' ) {
			answer = "TRUE";
		}
		return true;
	}

	ClassAd req_ad;
	StringList job_refs;      // job attrs referenced by requirements
	StringList machine_refs;  // machine attrs referenced by requirements

		// Insert dummy values for attributes of the job to which we
		// want to detect references.  Otherwise, unqualified references
		// get classified as external references.
	req_ad.Assign(ATTR_REQUEST_MEMORY,0);
	req_ad.Assign(ATTR_CKPT_ARCH,"");

	req_ad.GetExprReferences(answer.Value(),&job_refs,&machine_refs);

	checks_arch = IsDockerJob || machine_refs.contains_anycase( ATTR_ARCH );
	checks_opsys = IsDockerJob || machine_refs.contains_anycase( ATTR_OPSYS ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_AND_VER ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_LONG_NAME ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_SHORT_NAME ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_NAME ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_LEGACY );
	checks_disk =  machine_refs.contains_anycase( ATTR_DISK );
	checks_cpus =   machine_refs.contains_anycase( ATTR_CPUS );
	checks_tdp =  machine_refs.contains_anycase( ATTR_HAS_TDP );
	checks_encrypt_exec_dir = machine_refs.contains_anycase( ATTR_ENCRYPT_EXECUTE_DIRECTORY );
#if defined(WIN32)
	checks_credd = machine_refs.contains_anycase( ATTR_LOCAL_CREDD );
#endif

	if( JobUniverse == CONDOR_UNIVERSE_STANDARD ) {
		checks_ckpt_arch = job_refs.contains_anycase( ATTR_CKPT_ARCH );
	}
	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		checks_mpi = machine_refs.contains_anycase( ATTR_HAS_MPI );
	}
	if( mightTransfer(JobUniverse) ) { 
		switch( should_transfer ) {
		case STF_IF_NEEDED:
		case STF_NO:
			checks_fsdomain = machine_refs.contains_anycase(
										  ATTR_FILE_SYSTEM_DOMAIN ); 
			break;
		case STF_YES:
			checks_file_transfer = machine_refs.contains_anycase(
											   ATTR_HAS_FILE_TRANSFER );
			checks_file_transfer_plugin_methods = machine_refs.contains_anycase(
											   ATTR_HAS_FILE_TRANSFER_PLUGIN_METHODS );
			checks_per_file_encryption = machine_refs.contains_anycase(
										   ATTR_HAS_PER_FILE_ENCRYPTION );
			break;
		}
	}

	checks_mem = machine_refs.contains_anycase(ATTR_MEMORY);
	//checks_reqmem = job_refs.contains_anycase(ATTR_REQUEST_MEMORY);

	if( JobUniverse == CONDOR_UNIVERSE_JAVA ) {
		if( answer[0] ) {
			answer += " && ";
		}
		answer += "TARGET." ATTR_HAS_JAVA;
	} else if ( JobUniverse == CONDOR_UNIVERSE_VM ) {
		// For vm universe, we require the same archicture.
		if( !checks_arch ) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "(TARGET.Arch == \"";
			answer += ArchMacroDef.psz;
			answer += "\")";
		}
		// add HasVM to requirements
		bool checks_vm = false;
		checks_vm = machine_refs.contains_anycase( ATTR_HAS_VM );
		if( !checks_vm ) {
			answer += "&& (TARGET.";
			answer += ATTR_HAS_VM;
			answer += " =?= true)";
		}
		// add vm_type to requirements
		bool checks_vmtype = false;
		checks_vmtype = machine_refs.contains_anycase( ATTR_VM_TYPE);
		if( !checks_vmtype ) {
			answer += " && (TARGET.";
			answer += ATTR_VM_TYPE;
			answer += " == \"";
			answer += VMType.Value();
			answer += "\")";
		}
		// check if the number of executable VM is more than 0
		bool checks_avail = false;
		checks_avail = machine_refs.contains_anycase(ATTR_VM_AVAIL_NUM);
		if( !checks_avail ) {
			answer += " && (TARGET.";
			answer += ATTR_VM_AVAIL_NUM;
			answer += " > 0)";
		}
	} else if (IsDockerJob) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "TARGET.HasDocker";
	} else {
		if( !checks_arch ) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "(TARGET.Arch == \"";
			answer += ArchMacroDef.psz;
			answer += "\")";
		}

		if( !checks_opsys ) {
			answer += " && (TARGET.OpSys == \"";
			answer += OpsysMacroDef.psz;
			answer += "\")";
		}
	}

	if ( JobUniverse == CONDOR_UNIVERSE_STANDARD && !checks_ckpt_arch ) {
		answer += " && ((CkptArch == TARGET.Arch) ||";
		answer += " (CkptArch =?= UNDEFINED))";
		answer += " && ((CkptOpSys == TARGET.OpSys) ||";
		answer += "(CkptOpSys =?= UNDEFINED))";
	}

	if( !checks_disk ) {
		if (job->Lookup(ATTR_REQUEST_DISK)) {
			if ( ! RequestDiskIsZero) {
				answer += " && (TARGET.Disk >= RequestDisk)";
			}
		}
		else if ( JobUniverse == CONDOR_UNIVERSE_VM ) {
			// VM universe uses Total Disk 
			// instead of Disk for Condor slot
			answer += " && (TARGET.TotalDisk >= DiskUsage)";
		}else {
			answer += " && (TARGET.Disk >= DiskUsage)";
		}
	} else {
		if (JobUniverse != CONDOR_UNIVERSE_VM) {
			if ( ! RequestDiskIsZero && job->Lookup(ATTR_REQUEST_DISK)) {
				answer += " && (TARGET.Disk >= RequestDisk)";
			}
			if ( ! already_warned_requirements_disk && param_boolean("ENABLE_DEPRECATION_WARNINGS", false)) {
				fprintf(stderr, 
						"\nWARNING: Your Requirements expression refers to TARGET.Disk. "
						"This is obsolete. Set request_disk and condor_submit will modify the "
						"Requirements expression as needed.\n");
				already_warned_requirements_disk = true;
			}
		}
	}

	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		// The memory requirement for VM universe will be 
		// added in SetVMRequirements 
#if 1
		if ( ! RequestMemoryIsZero && job->Lookup(ATTR_REQUEST_MEMORY)) {
			answer += " && (TARGET.Memory >= RequestMemory)";
		}
		if (checks_mem) {
			if ( ! already_warned_requirements_mem && param_boolean("ENABLE_DEPRECATION_WARNINGS", false)) {
				fprintf(stderr, 
						"\nWARNING: your Requirements expression refers to TARGET.Memory. "
						"This is obsolete. Set request_memory and condor_submit will modify the "
						"Requirements expression as needed.\n");
				already_warned_requirements_mem = true;
			}
		}
#else
		if ( !checks_mem ) {
			answer += " && ( (TARGET.Memory * 1024) >= ImageSize ) ";
		}
		if ( !checks_reqmem ) {
			answer += " && ( ( RequestMemory * 1024 ) >= ImageSize ) ";
		}
#endif
	}

	if ( JobUniverse != CONDOR_UNIVERSE_GRID ) {
		if ( ! checks_cpus && ! RequestCpusIsZeroOrOne && job->Lookup(ATTR_REQUEST_CPUS)) {
			answer += " && (TARGET.Cpus >= RequestCpus)";
		}
	}

    // identify any custom pslot resource reqs and add them in:
    HASHITER it = hash_iter_begin(SubmitMacroSet);
    for (;  !hash_iter_done(it);  hash_iter_next(it)) {
        const char * key = hash_iter_key(it);
        // if key is not of form "request_xxx", ignore it:
        if ( ! starts_with_ignore_case(key, RequestPrefix)) continue;
        // if key is one of the predefined request_cpus, request_memory, etc, also ignore it,
        // those have their own special handling:
        if (fixedReqRes.count(key) > 0) continue;
        const char * rname = key + strlen(RequestPrefix);
        // resource name should be nonempty
        if ( ! *rname) continue;
        std::string clause;
        if (stringReqRes.count(rname) > 0)
            formatstr(clause, " && regexp(%s%s, TARGET.%s)", ATTR_REQUEST_PREFIX, rname, rname);
        else
            formatstr(clause, " && (TARGET.%s%s >= %s%s)", "", rname, ATTR_REQUEST_PREFIX, rname);
        answer += clause;
    }
    hash_iter_delete(&it);

	if( HasTDP && !checks_tdp ) {
		answer += " && (TARGET.";
		answer += ATTR_HAS_TDP;
		answer += ")";
	}

	if( HasEncryptExecuteDir && !checks_encrypt_exec_dir ) {
		answer += " && (TARGET.";
		answer += ATTR_HAS_ENCRYPT_EXECUTE_DIRECTORY;
		answer += ")";
	}

	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		if( ! checks_mpi ) {
			answer += " && (TARGET.";
			answer += ATTR_HAS_MPI;
			answer += ")";
		}
	}


	if( mightTransfer(JobUniverse) ) {
			/* 
			   This is a kind of job that might be using file transfer
			   or a shared filesystem.  so, tack on the appropriate
			   clause to make sure we're either at a machine that
			   supports file transfer, or that we're in the same file
			   system domain.
			*/

		switch( should_transfer ) {
		case STF_NO:
				// no file transfer used.  if there's nothing about
				// the FileSystemDomain yet, tack on a clause for
				// that. 
			if( ! checks_fsdomain ) {
				answer += " && (TARGET.";
				answer += ATTR_FILE_SYSTEM_DOMAIN;
				answer += " == MY.";
				answer += ATTR_FILE_SYSTEM_DOMAIN;
				answer += ")";
			} 
			break;
			
		case STF_YES:
				// we're definitely going to use file transfer.  
			if( ! checks_file_transfer ) {
				answer += " && (TARGET.";
				answer += ATTR_HAS_FILE_TRANSFER;
				if (!checks_per_file_encryption && NeedsPerFileEncryption) {
					answer += " && TARGET.";
					answer += ATTR_HAS_PER_FILE_ENCRYPTION;
				}

				if( (!checks_file_transfer_plugin_methods) ) {
					// check input
					char* file_list = condor_param( TransferInputFiles, "TransferInputFiles" );
					char* tmp_ptr;
					if(file_list) {
						StringList files(file_list, ",");
						files.rewind();
						while ( (tmp_ptr=files.next()) ) {
							if (IsUrl(tmp_ptr)){
								MyString plugintype = getURLType(tmp_ptr);
								answer += " && stringListMember(\"";
								answer += plugintype;
								answer += "\",HasFileTransferPluginMethods)";
							}
						}
						free(file_list);
					}

					// check output
					tmp_ptr = condor_param( OutputDestination, "OutputDestination" );
					if (tmp_ptr) {
						if (IsUrl(tmp_ptr)){
							MyString plugintype = getURLType(tmp_ptr);
							answer += " && stringListMember(\"";
							answer += plugintype;
							answer += "\",HasFileTransferPluginMethods)";
						}
						free (tmp_ptr);
					}
				}

				// close of the file transfer requirements
				answer += ")";
			}
			break;
			
		case STF_IF_NEEDED:
				// we may or may not use file transfer, so require
				// either the same FS domain OR the ability to file
				// transfer.  if the user already refered to fs
				// domain, but explictly turned on IF_NEEDED, assume
				// they know what they're doing. 
			if( ! checks_fsdomain ) {
				ft_clause = " && ((TARGET.";
				ft_clause += ATTR_HAS_FILE_TRANSFER;
				if (NeedsPerFileEncryption) {
					ft_clause += " && TARGET.";
					ft_clause += ATTR_HAS_PER_FILE_ENCRYPTION;
				}
				ft_clause += ") || (TARGET.";
				ft_clause += ATTR_FILE_SYSTEM_DOMAIN;
				ft_clause += " == MY.";
				ft_clause += ATTR_FILE_SYSTEM_DOMAIN;
				ft_clause += "))";
				answer += ft_clause.Value();
			}
			break;
		}
	}

		//
		// Job Deferral
		// If the flag was set true by SetJobDeferral() then we will
		// add the requirement in for this feature
		//
	if ( NeedsJobDeferral ) {
			//
			// Add the HasJobDeferral to the attributes so that it will
			// match with a Starter that can actually provide this feature
			// We only need to do this if the job's universe isn't local
			// This is because the schedd doesn't pull out the Starter's
			// machine ad when trying to match local universe jobs
			//
			//
		if ( JobUniverse != CONDOR_UNIVERSE_LOCAL ) {
			answer += " && TARGET." ATTR_HAS_JOB_DEFERRAL;
		}
		
			//
			//
			// Prepare our new requirement attribute
			// This nifty expression will evaluate to true when
			// the job's prep start time is before the next schedd
			// polling interval. We also have a nice clause to prevent
			// our job from being matched and executed after it could
			// possibly run
			//
			//	( ( time() + ATTR_SCHEDD_INTERVAL ) >= 
			//	  ( ATTR_DEFERRAL_TIME - ATTR_DEFERRAL_PREP_TIME ) )
			//  &&
			//    ( time() < 
			//    ( ATTR_DEFFERAL_TIME + ATTR_DEFERRAL_WINDOW ) )
			//  )
			//
		MyString attrib;
		attrib.formatstr( "( ( time() + %s ) >= ( %s - %s ) ) && "\
						"( time() < ( %s + %s ) )",
						ATTR_SCHEDD_INTERVAL,
						ATTR_DEFERRAL_TIME,
						ATTR_DEFERRAL_PREP_TIME,
						ATTR_DEFERRAL_TIME,
						ATTR_DEFERRAL_WINDOW );
		answer += " && (";
		answer += attrib.Value();
		answer += ")";			
	} // !seenBefore

#if defined(WIN32)
		//
		// Windows CredD
		// run_as_owner jobs on Windows require that the remote starter can
		// reach the same credd as the submit machine. We add the following:
		//   - HasWindowsRunAsOwner
		//   - LocalCredd == <CREDD_HOST> (if LocalCredd not found)
		//
	if ( RunAsOwnerCredD ) {
		MyString tmp_rao = " && (TARGET.";
		tmp_rao += ATTR_HAS_WIN_RUN_AS_OWNER;
		if (!checks_credd) {
			tmp_rao += " && (TARGET.";
			tmp_rao += ATTR_LOCAL_CREDD;
			tmp_rao += " =?= \"";
			tmp_rao += RunAsOwnerCredD;
			tmp_rao += "\")";
		}
		tmp_rao += ")";
		answer += tmp_rao.Value();
		free(RunAsOwnerCredD);
	}
#endif

	/* if the user specified they want this feature, add it to the requirements */
	return true;
}



char const *
full_path(const char *name, bool use_iwd)
{
	static MyString	pathname;
	char const *p_iwd;
	MyString realcwd;

	if ( use_iwd ) {
		ASSERT(JobIwd.Length());
		p_iwd = JobIwd.Value();
	} else {
		condor_getcwd(realcwd);
		p_iwd = realcwd.Value();
	}

#if defined(WIN32)
	if ( name[0] == '\\' || name[0] == '/' || (name[0] && name[1] == ':') ) {
		pathname = name;
	} else {
		pathname.formatstr( "%s\\%s", p_iwd, name );
	}
#else

	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		pathname.formatstr( "%s%s", JobRootdir.Value(), name );
	} else {	/* relative to iwd which is relative to the root */
		pathname.formatstr( "%s/%s/%s", JobRootdir.Value(), p_iwd, name );
	}
#endif

	compress( pathname );

	return pathname.Value();
}

// check directories for input and output.  for now we do nothing
// other than to make sure that it actually IS a directory on Windows
// On Linux we already know that by the error code from safe_open below
//
static bool
check_directory( const char* pathname, int /*flags*/, int err )
{
#if defined(WIN32)
	// Make sure that it actually is a directory
	DWORD dwAttribs = GetFileAttributes(pathname);
	if (INVALID_FILE_ATTRIBUTES == dwAttribs)
		return false;
	return (dwAttribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	// will just do nothing here and leave
	// it up to the runtime to nicely report errors.
	(void)pathname;
	return (err == EISDIR);
#endif
}

void
check_open( const char *name, int flags )
{
	MyString strPathname;
	StringList *list;

		/* The user can disable file checks on a per job basis, in such a
		   case we avoid adding the files to CheckFilesWrite/Read.
		*/
	if ( JobDisableFileChecks ) return;

	/* No need to check for existence of the Null file. */
	if( strcmp(name, NULL_FILE) == MATCH ) {
		return;
	}

	if ( IsUrl( name ) || strstr(name, "$$(") ) {
		return;
	}

	strPathname = full_path(name);

	// is the last character a path separator?
	int namelen = strlen(name);
	bool trailing_slash = namelen > 0 && IS_ANY_DIR_DELIM_CHAR(name[namelen-1]);

		/* This is only for MPI.  We test for our string that
		   we replaced "$(NODE)" with, and replace it with "0".  Thus, 
		   we will really only try and access the 0th file only */
	if ( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		strPathname.replaceString("#MpInOdE#", "0");
	} else if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL ) {
		strPathname.replaceString("#pArAlLeLnOdE#", "0");
	}


	/* If this file as marked as append-only, do not truncate it here */

	auto_free_ptr append_files(condor_param( AppendFiles, ATTR_APPEND_FILES ));
	if (append_files.ptr()) {
		list = new StringList(append_files.ptr(), ",");
		if(list->contains_withwildcard(name)) {
			flags = flags & ~O_TRUNC;
		}
		delete list;
	}

	bool dryrun_create = DashDryRun && ((flags & (O_CREAT|O_TRUNC)) != 0);
	if (DashDryRun) {
		flags = flags & ~(O_CREAT|O_TRUNC);
	}

	if ( !DisableFileChecks ) {
		int fd = safe_open_wrapper_follow(strPathname.Value(),flags | O_LARGEFILE,0664);
		if ((fd < 0) && (errno == ENOENT) && dryrun_create) {
			// we are doing dry-run, and the input flags were to create/truncate a file
			// we stripped the create/truncate flags, now we treate a 'file does not exist' error
			// as success since O_CREAT would have made it (probably).
		} else if ( fd < 0 ) {
			// note: Windows does not set errno to EISDIR for directories, instead you get back EACCESS (or ENOENT?)
			if( (trailing_slash || errno == EISDIR || errno == EACCES) &&
	                   check_directory( strPathname.Value(), flags, errno ) ) {
					// Entries in the transfer output list may be
					// files or directories; no way to tell in
					// advance.  When there is already a directory by
					// the same name, it is not obvious what to do.
					// Therefore, we will just do nothing here and leave
					// it up to the runtime to nicely report errors.
				return;
			}
			fprintf( stderr, "\nERROR: Can't open \"%s\"  with flags 0%o (%s)\n",
					 strPathname.Value(), flags, strerror( errno ) );
			DoCleanup(0,0,NULL);
			exit( 1 );
		} else {
			(void)close( fd );
		}
	}

	// Queue files for testing access if not already queued
	int junk;
	if( flags & O_WRONLY )
	{
		if ( CheckFilesWrite.lookup(strPathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesWrite.insert(strPathname,junk);
		}
	}
	else
	{
		if ( CheckFilesRead.lookup(strPathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesRead.insert(strPathname,junk);
		}
	}
}
#endif


void
usage()
{
	fprintf( stderr, "Usage: %s [options] [<attrib>=<value>] [- | <submit-file>]\n", MyName );
	fprintf( stderr, "    [options] are\n" );
	fprintf( stderr, "\t-terse  \t\tDisplay terse output, jobid ranges only\n" );
	fprintf( stderr, "\t-verbose\t\tDisplay verbose output, jobid and full job ClassAd\n" );
	fprintf( stderr, "\t-debug  \t\tDisplay debugging output\n" );
	fprintf( stderr, "\t-append <line>\t\tadd line to submit file before processing\n"
					 "\t              \t\t(overrides submit file; multiple -a lines ok)\n" );
	fprintf( stderr, "\t-queue <queue-opts>\tappend Queue statement to submit file before processing\n"
					 "\t                   \t(submit file must not already have a Queue statement)\n" );
	fprintf( stderr, "\t-batch-name <name>\tappend a line to submit file that sets the batch name\n"
					/* "\t                  \t(overrides batch_name in submit file)\n" */);
	fprintf( stderr, "\t-disable\t\tdisable file permission checks\n" );
	fprintf( stderr, "\t-dry-run <filename>\tprocess submit file and write ClassAd attributes to <filename>\n"
					 "\t        \t\tbut do not actually submit the job(s) to the SCHEDD\n" );
	fprintf( stderr, "\t-maxjobs <maxjobs>\tDo not submit if number of jobs would exceed <maxjobs>.\n" );
	fprintf( stderr, "\t-single-cluster\t\tDo not submit if more than one ClusterId is needed.\n" );
	fprintf( stderr, "\t-unused\t\t\ttoggles unused or unexpanded macro warnings\n"
					 "\t       \t\t\t(overrides config file; multiple -u flags ok)\n" );
	//fprintf( stderr, "\t-force-mpi-universe\tAllow submission of obsolete MPI universe\n );
	fprintf( stderr, "\t-dump <filename>\tWrite job ClassAds to <filename> instead of\n"
					 "\t                \tsubmitting to a schedd.\n" );
#if !defined(WIN32)
	fprintf( stderr, "\t-interactive\t\tsubmit an interactive session job\n" );
#endif
	fprintf( stderr, "\t-name <name>\t\tsubmit to the specified schedd\n" );
	fprintf( stderr, "\t-remote <name>\t\tsubmit to the specified remote schedd\n"
					 "\t              \t\t(implies -spool)\n" );
    fprintf( stderr, "\t-addr <ip:port>\t\tsubmit to schedd at given \"sinful string\"\n" );
	fprintf( stderr, "\t-spool\t\t\tspool all files to the schedd\n" );
	fprintf( stderr, "\t-password <password>\tspecify password to MyProxy server\n" );
	fprintf( stderr, "\t-pool <host>\t\tUse host as the central manager to query\n" );
	fprintf( stderr, "\t-stm <method>\t\tHow to move a sandbox into HTCondor\n" );
	fprintf( stderr, "\t             \t\t<methods> is one of: stm_use_schedd_only\n" );
	fprintf( stderr, "\t             \t\t                     stm_use_transferd\n" );

	fprintf( stderr, "\t<attrib>=<value>\tSet <attrib>=<value> before reading the submit file.\n" );

	fprintf( stderr, "\n    If <submit-file> is omitted or is -, input is read from stdin.\n"
					 "    Use of - implies verbose output unless -terse is specified\n");
}


extern "C" {
int
DoCleanup(int,int,const char*)
{
	if( JobsCreated ) 
	{
		// TerminateCluster() may call EXCEPT() which in turn calls 
		// DoCleanup().  This lead to infinite recursion which is bad.
		JobsCreated = 0;
		if ( ! ActiveQueueConnection) {
			if (DumpClassAdToFile) {
			} else {
				CondorError errstack;
				ActualScheddQ *ScheddQ = (ActualScheddQ *)MyQ;
				if ( ! ScheddQ) { MyQ = ScheddQ = new ActualScheddQ(); }
				ActiveQueueConnection = ScheddQ->Connect(*MySchedd, errstack) != 0;
			}
		}
		if (ActiveQueueConnection && MyQ) {
			// Call DestroyCluster() now in an attempt to get the schedd
			// to unlink the initial checkpoint file (i.e. remove the 
			// executable we sent earlier from the SPOOL directory).
			MyQ->destroy_Cluster( ClusterId );

			// We purposefully do _not_ call DisconnectQ() here, since 
			// we do not want the current transaction to be committed.
		}
	}

	if (owner) {
		free(owner);
	}
#ifdef USE_SUBMIT_UTILS
#else
	if (ntdomain) {
		free(ntdomain);
	}
#endif
	if (myproxy_password) {
		free (myproxy_password);
	}

	return 0;		// For historical reasons...
}
} /* extern "C" */


void
init_params()
{
	char *tmp = NULL;
	MyString method;

#ifdef USE_SUBMIT_UTILS
	const char * err = init_submit_default_macros();
	if (err) {
		fprintf(stderr, "%s\n", err);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
#else
	if (SubmitMacroSet.errors) delete SubmitMacroSet.errors;
	SubmitMacroSet.errors = new CondorError();

	ArchMacroDef.psz = param( "ARCH" );

	if( ArchMacroDef.psz == NULL ) {
		fprintf(stderr, "ARCH not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	OpsysMacroDef.psz = param( "OPSYS" );
	if( OpsysMacroDef.psz == NULL ) {
		fprintf(stderr,"OPSYS not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	// also pick up the variations on opsys if they are defined.
	OpsysAndVerMacroDef.psz = param( "OPSYSANDVER" );
	if ( ! OpsysAndVerMacroDef.psz) OpsysAndVerMacroDef.psz = UnsetString;
	OpsysMajorVerMacroDef.psz = param( "OPSYSMAJORVER" );
	if ( ! OpsysMajorVerMacroDef.psz) OpsysMajorVerMacroDef.psz = UnsetString;
	OpsysVerMacroDef.psz = param( "OPSYSVER" );
	if ( ! OpsysVerMacroDef.psz) OpsysVerMacroDef.psz = UnsetString;

	SpoolMacroDef.psz = param( "SPOOL" );
	if( SpoolMacroDef.psz == NULL ) {
		fprintf(stderr,"SPOOL not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
#endif

	My_fs_domain = param( "FILESYSTEM_DOMAIN" );
		// Will always return something, since config() will put in a
		// value (full hostname) if it's not in the config file.  
	

	// The default is set as the global initializer for STMethod
	tmp = param( "SANDBOX_TRANSFER_METHOD" );
	if ( tmp != NULL ) {
		method = tmp;
		free( tmp );
		string_to_stm( method, STMethod );
	}

	WarnOnUnusedMacros =
		param_boolean_crufty("WARN_ON_UNUSED_SUBMIT_FILE_MACROS",
							 WarnOnUnusedMacros ? true : false) ? 1 : 0;

	if ( param_boolean("SUBMIT_NOACK_ON_SETATTRIBUTE",true) ) {
		setattrflags |= SetAttribute_NoAck;  // set noack flag
	} else {
		setattrflags &= ~SetAttribute_NoAck; // clear noack flag
	}

#ifdef USE_SUBMIT_UTILS
#else
    // the special "fixed" request_xxx forms
    fixedReqRes.clear();
    fixedReqRes.insert(RequestCpus);
    fixedReqRes.insert(RequestMemory);
    fixedReqRes.insert(RequestDisk);
#endif
}

#ifdef USE_SUBMIT_UTILS
#else
int
whitespace( const char *str)
{
	while( *str ) {
		if( isspace(*str++) ) {
			return( 1 );
		}
	}

	return( 0 );
}

void
compress( MyString &path )
{
	char	*src, *dst;
	char *str = strdup(path.Value());

	src = str;
	dst = str;

#ifdef WIN32
	if (*src) {
		*dst++ = *src++;	// don't compress WIN32 "share" path
	}
#endif

	while( *src ) {
		*dst++ = *src++;
		while( (*(src - 1) == '\\' || *(src - 1) == '/') && (*src == '\\' || *src == '/') ) {
			src++;
		}
	}
	*dst = '\0';

	path = str;
	free( str );
}
#endif

extern "C" {
int SetSyscalls( int foo ) { return foo; }
}


int SendJobCredential()
{
	// however, if we do need to send it for any job, we only need to do that once.
	if (sent_credential_to_credd) {
		return 0;
	}

	MyString producer;
	if(!param(producer, "SEC_CREDENTIAL_PRODUCER")) {
		// nothing to do
		return 0;
	}

	// If SEC_CREDENTIAL_PRODUCER is set to magic value CREDENTIAL_ALREADY_STORED,
	// this means that condor_submit should NOT bother spending time to send the
	// credential to the credd (because it is already there), but it SHOULD do
	// all the other work as if it did send it (such as setting the SendCredential
	// attribute so the starter will fetch the credential at job launch).
	// If SEC_CREDENTIAL_PRODUCER is anything else, then consider it to be the
	// name of a script we should spawn to create the credential.

	if ( strcasecmp(producer.Value(),"CREDENTIAL_ALREADY_STORED") != MATCH ) {
		// If we made it here, we need to spawn a credential producer process.
		dprintf(D_ALWAYS, "CREDMON: invoking %s\n", producer.c_str());
		ArgList args;
		args.AppendArg(producer);
		FILE* uber_file = my_popen(args, "r", 0);
		unsigned char *uber_ticket = NULL;
		if (!uber_file) {
			fprintf(stderr, "\nERROR: (%i) invoking %s\n", errno, producer.c_str());
			exit( 1 );
		} else {
			uber_ticket = (unsigned char*)malloc(65536);
			ASSERT(uber_ticket);
			int bytes_read = fread(uber_ticket, 1, 65536, uber_file);
			// what constitutes failure?
			my_pclose(uber_file);

			if(bytes_read == 0) {
				fprintf(stderr, "\nERROR: failed to read any data from %s!\n", producer.c_str());
				exit( 1 );
			}

			// immediately convert to base64
			char* ut64 = zkm_base64_encode(uber_ticket, bytes_read);

			// sanity check:  convert it back.
			//unsigned char *zkmbuf = 0;
			int zkmlen = -1;
			unsigned char* zkmbuf = NULL;
			zkm_base64_decode(ut64, &zkmbuf, &zkmlen);

			dprintf(D_FULLDEBUG, "CREDMON: b64: %i %i\n", bytes_read, zkmlen);
			dprintf(D_FULLDEBUG, "CREDMON: b64: %s %s\n", (char*)uber_ticket, (char*)zkmbuf);

			char preview[64];
			strncpy(preview,ut64, 63);
			preview[63]=0;

			dprintf(D_FULLDEBUG | D_SECURITY, "CREDMON: read %i bytes {%s...}\n", bytes_read, preview);

			// setup the username to query
			char userdom[256];
			char* the_username = my_username();
			char* the_domainname = my_domainname();
			sprintf(userdom, "%s@%s", the_username, the_domainname);
			free(the_username);
			free(the_domainname);

			dprintf(D_ALWAYS, "CREDMON: storing cred for user %s\n", userdom);
			Daemon my_credd(DT_CREDD);
			int store_cred_result;
			if (my_credd.locate()) {
				store_cred_result = store_cred(userdom, ut64, ADD_MODE, &my_credd);
				if ( store_cred_result != SUCCESS ) {
					fprintf( stderr, "\nERROR: store_cred failed!\n");
					exit( 1 );
				}
			} else {
				fprintf( stderr, "\nERROR: locate(credd) failed!\n");
				exit( 1 );
			}
		}
	}  // end of block to run a credential producer

	// If we made it here, we either successufully ran a credential producer, or
	// we've been told a credential has already been stored.  Either way we want
	// to set a flag that tells the rest of condor_submit that there is a stored
	// credential associated with this job.

	sent_credential_to_credd = true;

	return 0;
}

void SetSendCredentialInAd( ClassAd *job_ad )
{
	if (!sent_credential_to_credd) {
		return;
	}

	// add it to the job ad (starter needs to know this value)
	job_ad->Assign( ATTR_JOB_SEND_CREDENTIAL, true );
}

#ifdef USE_SUBMIT_UTILS

int SendLastExecutable()
{
	const char * ename = LastExecutable.empty() ? NULL : LastExecutable.Value();
	bool copy_to_spool = SpoolLastExecutable;
	MyString SpoolEname(ename);

	// spool executable if necessary
	if ( ename && copy_to_spool ) {

		bool try_ickpt_sharing = false;
		CondorVersionInfo cvi(MySchedd->version());
		if (cvi.built_since_version(7, 3, 0)) {
			try_ickpt_sharing = param_boolean("SHARE_SPOOLED_EXECUTABLES",
												true);
		}

		MyString md5;
		if (try_ickpt_sharing) {
			Condor_MD_MAC cmm;
			unsigned char* md5_raw;
			if (!cmm.addMDFile(ename)) {
				dprintf(D_ALWAYS,
						"SHARE_SPOOLED_EXECUTABLES will not be used: "
							"MD5 of file %s failed\n",
						ename);
			}
			else if ((md5_raw = cmm.computeMD()) == NULL) {
				dprintf(D_ALWAYS,
						"SHARE_SPOOLED_EXECUTABLES will not be used: "
							"no MD5 support in this Condor build\n");
			}
			else {
				for (int i = 0; i < MAC_SIZE; i++) {
					md5.formatstr_cat("%02x", static_cast<int>(md5_raw[i]));
				}
				free(md5_raw);
			}
		}
		int ret;
		if ( ! md5.IsEmpty()) {
			ClassAd tmp_ad;
			tmp_ad.Assign(ATTR_OWNER, owner);
			tmp_ad.Assign(ATTR_JOB_CMD_MD5, md5.Value());
			ret = MyQ->send_SpoolFileIfNeeded(tmp_ad);
		}
		else {
			char * chkptname = gen_ckpt_name(0, submit_hash.getClusterId(), ICKPT, 0);
			SpoolEname = chkptname;
			if (chkptname) free(chkptname);
			ret = MyQ->send_SpoolFile(SpoolEname.Value());
		}

		if (ret < 0) {
			fprintf( stderr,
						"\nERROR: Request to transfer executable %s failed\n",
						SpoolEname.Value() );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		// ret will be 0 if the SchedD gave us the go-ahead to send
		// the file. if it's not, the SchedD is using ickpt sharing
		// and already has a copy, so no need
		//
		if ((ret == 0) && MyQ->send_SpoolFileBytes(submit_hash.full_path(ename,false)) < 0) {
			fprintf( stderr,
						"\nERROR: failed to transfer executable file %s\n", 
						ename );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
	}
	return 0;
}


int SendJobAd (ClassAd * job, ClassAd * ClusterAd)
{
	ExprTree *tree = NULL;
	const char *lhstr, *rhstr;
	int  retval = 0;
	int myprocid = ProcId;

	if ( ProcId > 0 ) {
		MyQ->set_AttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId, setattrflags);
	} else {
		myprocid = -1;		// means this is a cluster ad
		if( MyQ->set_AttributeInt (ClusterId, myprocid, ATTR_CLUSTER_ID, ClusterId, setattrflags) == -1 ) {
			if( setattrflags & SetAttribute_NoAck ) {
				fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
					ClusterId, ProcId);
			} else {
				fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n",
					ATTR_CLUSTER_ID, ClusterId, ClusterId, ProcId, errno);
			}
			return -1;
		}
	}

#if defined(ADD_TARGET_SCOPING)
	if ( JobUniverse == CONDOR_UNIVERSE_SCHEDULER ||
		 JobUniverse == CONDOR_UNIVERSE_LOCAL ) {
		job->AddTargetRefs( TargetScheddAttrs, false );
	} else {
		job->AddTargetRefs( TargetMachineAttrs, false );
	}
#endif

	job->ResetExpr();
	while( job->NextExpr(lhstr, tree) ) {
		rhstr = ExprTreeToString( tree );
		if( !lhstr || !rhstr ) { 
			fprintf( stderr, "\nERROR: Null attribute name or value for job %d.%d\n",
					 ClusterId, ProcId );
			retval = -1;
		} else {
			int tmpProcId = myprocid;
			// IsNoClusterAttr tells us that an attribute should always be in the proc ad
			bool force_proc = IsNoClusterAttr(lhstr);
			if ( ProcId > 0 ) {
				// For all but the first proc, check each attribute against the value in the cluster ad.
				// If the values match, don't add the attribute to this proc ad
				if ( ! force_proc) {
					ExprTree *cluster_tree = ClusterAd->LookupExpr( lhstr );
					if ( cluster_tree && *tree == *cluster_tree ) {
						continue;
					}
				}
			} else {
				if (force_proc) {
					myprocid = ProcId;
				}
			}

			if( MyQ->set_Attribute(ClusterId, myprocid, lhstr, rhstr, setattrflags) == -1 ) {
				if( setattrflags & SetAttribute_NoAck ) {
					fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
						ClusterId, ProcId);
				} else {
					fprintf( stderr, "\nERROR: Failed to set %s=%s for job %d.%d (%d)\n",
						 lhstr, rhstr, ClusterId, ProcId, errno );
				}
				retval = -1;
			}
			myprocid = tmpProcId;
		}
		if(retval == -1) {
			return -1;
		}
	}

	if ( ProcId == 0 ) {
		if( MyQ->set_AttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId, setattrflags) == -1 ) {
			if( setattrflags & SetAttribute_NoAck ) {
				fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
					ClusterId, ProcId);
			} else {
				fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n",
					 ATTR_PROC_ID, ProcId, ClusterId, ProcId, errno );
			}
			return -1;
		}
	}

	return retval;
}

#else
int
SaveClassAd ()
{
	ExprTree *tree = NULL;
	const char *lhstr, *rhstr;
	int  retval = 0;
	int myprocid = ProcId;
	static ClassAd* current_cluster_ad = NULL;

	if ( ProcId > 0 ) {
		MyQ->set_AttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId, setattrflags);
	} else {
		myprocid = -1;		// means this is a cluster ad
		if( MyQ->set_AttributeInt (ClusterId, myprocid, ATTR_CLUSTER_ID, ClusterId, setattrflags) == -1 ) {
			if( setattrflags & SetAttribute_NoAck ) {
				fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
					ClusterId, ProcId);
			} else {
				fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n",
					ATTR_CLUSTER_ID, ClusterId, ClusterId, ProcId, errno);
			}
			return -1;
		}
	}

#if defined(ADD_TARGET_SCOPING)
	if ( JobUniverse == CONDOR_UNIVERSE_SCHEDULER ||
		 JobUniverse == CONDOR_UNIVERSE_LOCAL ) {
		job->AddTargetRefs( TargetScheddAttrs, false );
	} else {
		job->AddTargetRefs( TargetMachineAttrs, false );
	}
#endif

	job->ResetExpr();
	while( job->NextExpr(lhstr, tree) ) {
		rhstr = ExprTreeToString( tree );
		if( !lhstr || !rhstr ) { 
			fprintf( stderr, "\nERROR: Null attribute name or value for job %d.%d\n",
					 ClusterId, ProcId );
			retval = -1;
		} else {
			int tmpProcId = myprocid;
			// IsNoClusterAttr tells us that an attribute should always be in the proc ad
		#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
			bool force_proc = IsNoClusterAttr(lhstr);
			if ( ProcId > 0 ) {
				// For all but the first proc, check each attribute against the value in the cluster ad.
				// If the values match, don't add the attribute to this proc ad
				if ( ! force_proc) {
					ExprTree *cluster_tree = ClusterAd->LookupExpr( lhstr );
					if ( cluster_tree && *tree == *cluster_tree ) {
						continue;
					}
				}
			} else {
				if (force_proc) {
					myprocid = ProcId;
				}
			}
		#else
			// NoClusterCheckAttrs is a list of attributes that should
			// always go into the proc ad. For proc 0, this means
			// inserting the attribute into the proc ad instead of the
			// cluster ad.
			if ( ProcId > 0 ) {
				if ( !NoClusterCheckAttrs.contains_anycase( lhstr ) ) {
					ExprTree *cluster_tree = ClusterAd->LookupExpr( lhstr );
					if ( cluster_tree && *tree == *cluster_tree ) {
						continue;
					}
				}
			} else {
				if ( NoClusterCheckAttrs.contains_anycase( lhstr ) ) {
					myprocid = ProcId;
				}
			}
		#endif

			if( MyQ->set_Attribute(ClusterId, myprocid, lhstr, rhstr, setattrflags) == -1 ) {
				if( setattrflags & SetAttribute_NoAck ) {
					fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
						ClusterId, ProcId);
				} else {
					fprintf( stderr, "\nERROR: Failed to set %s=%s for job %d.%d (%d)\n",
						 lhstr, rhstr, ClusterId, ProcId, errno );
				}
				retval = -1;
			}
			myprocid = tmpProcId;
		}
		if(retval == -1) {
			return -1;
		}
	}

	if ( ProcId == 0 ) {
		if( MyQ->set_AttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId, setattrflags) == -1 ) {
			if( setattrflags & SetAttribute_NoAck ) {
				fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
					ClusterId, ProcId);
			} else {
				fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n",
					 ATTR_PROC_ID, ProcId, ClusterId, ProcId, errno );
			}
			return -1;
		}
	}

	// If spooling entire job "sandbox" to the schedd, then we need to keep
	// the classads in an array to later feed into the filetransfer object.
	if ( dash_remote ) {
		char tbuf[200];
		sprintf(tbuf,"%s=%d",ATTR_CLUSTER_ID, ClusterId);
		job->Insert(tbuf);
		sprintf(tbuf,"%s=%d",ATTR_PROC_ID, ProcId);
		job->Insert(tbuf);
		if ( ProcId == 0 ) {
			// new cluster -- save pointer to cluster ad
			current_cluster_ad = job;
		} else {
			// chain ad to previously seen cluster ad
			ASSERT(current_cluster_ad);
			job->ChainToAd(current_cluster_ad);
		}
		JobAdsArray[ JobAdsArrayLen++ ] = job;
		job_ad_saved = true;
	}

	return retval;
}


void
InsertJobExpr (MyString const &expr)
{
	InsertJobExpr(expr.Value());
}

void 
InsertJobExpr (const char *expr, bool from_config_file /*=false*/)
{
	std::string attr;
	ExprTree *tree = NULL;
	if ( ! ParseLongFormAttrValue(expr, attr, tree))
	{
		fprintf (stderr, "\nERROR: Parse error in expression: \n\t%s\n", expr);
		fprintf(stderr,"Error in %s. Aborting submit.\n", from_config_file ? "config file SUBMIT_ATTRS or SUBMIT_EXPRS value" : "submit file");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if (!job->Insert (attr, tree))
	{	
		fprintf(stderr,"\nERROR: Unable to insert expression: %s\n", expr);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}


void 
InsertJobExprInt(const char * name, int val)
{
	ASSERT(name);
	MyString buf;
	buf.formatstr("%s = %d", name, val);
	InsertJobExpr(buf.Value());
}

void 
InsertJobExprString(const char * name, const char * val)
{
	ASSERT(name);
	ASSERT(val);
	MyString buf;
	std::string esc;
	buf.formatstr("%s = %s", name, QuoteAdStringValue(val, esc));
	InsertJobExpr(buf.Value());
}
#endif

// hash function used by the CheckFiles read/write tables
static unsigned int 
hashFunction (const MyString &str)
{
	 int i = str.Length() - 1;
	 unsigned int hashVal = 0;
	 while (i >= 0) 
	 {
		  hashVal += (unsigned int)tolower(str[i]);
		  i--;
	 }
	 return hashVal;
}


// If our effective and real gids are different (b/c the submit binary
// is setgid) set umask(002) so that stdout, stderr and the user log
// files are created writable by group condor.  This way, people who
// run Condor as condor can still have the right permissions on their
// files for the default case of their job doesn't open its own files.
void
check_umask()
{
#ifndef WIN32
	if( getgid() != getegid() ) {
		umask( 002 );
	}
#endif
}

void 
setupAuthentication()
{
		//RendezvousDir for remote FS auth can be specified in submit file.
	char *Rendezvous = NULL;
#ifdef USE_SUBMIT_UTILS
	Rendezvous = submit_hash.submit_param( SUBMIT_KEY_RendezvousDir, "rendezvous_dir" );
#else
	Rendezvous = condor_param( RendezvousDir, "rendezvous_dir" );
#endif
	if( ! Rendezvous ) {
			// If those didn't work, try a few other variations, just
			// to be safe:
#ifdef USE_SUBMIT_UTILS
		Rendezvous = submit_hash.submit_param( "rendezvousdirectory", "rendezvous_directory" );
#else
		Rendezvous = condor_param( "rendezvousdirectory",
									"rendezvous_directory" );
#endif
	}
	if( Rendezvous ) {
		dprintf( D_FULLDEBUG,"setting RENDEZVOUS_DIRECTORY=%s\n", Rendezvous );
			//SetEnv because Authentication::authenticate() expects them there.
		SetEnv( "RENDEZVOUS_DIRECTORY", Rendezvous );
		free( Rendezvous );
	}
}

#ifdef USE_SUBMIT_UTILS
#else

bool
mightTransfer( int universe )
{
	switch( universe ) {
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_VM:
		return true;
		break;
	default:
		return false;
		break;
	}
	return false;
}


bool
isTrue( const char* attr )
{
	if( ! attr ) {
		return false;
	}
	switch( attr[0] ) {
	case 't':
	case 'T':
	case 'y':
	case 'Y':
		return true;
		break;
	default:
		break;
	}
	return false;
}

// add a vm file to transfer_input_files
void transfer_vm_file(const char *filename) 
{
	MyString fixedname;
	MyString buffer;

	if( !filename ) {
		return;
	}

	fixedname = delete_quotation_marks(filename);

	StringList transfer_file_list(NULL, ",");
	MyString transfer_input_files;

	// check whether the file is in transfer_input_files
	if( job->LookupString(ATTR_TRANSFER_INPUT_FILES,transfer_input_files) 
				== 1 ) {
		transfer_file_list.initializeFromString(transfer_input_files.Value());
		if( filelist_contains_file(fixedname.Value(), 
					&transfer_file_list, true) ) {
			// this file is already in transfer_input_files
			return;
		}
	}

	// we need to add it
	char *tmp_ptr = NULL;
	check_and_universalize_path(fixedname);
	check_open(fixedname.Value(), O_RDONLY);
	TransferInputSizeKb += calc_image_size_kb(fixedname.Value());

	transfer_file_list.append(fixedname.Value());
	tmp_ptr = transfer_file_list.print_to_string();

	buffer.formatstr( "%s = \"%s\"", ATTR_TRANSFER_INPUT_FILES, tmp_ptr);
	InsertJobExpr(buffer);
	free(tmp_ptr);

	SetImageSize();
}

bool parse_vm_option(char *value, bool& onoff)
{
	if( !value ) {
		return false;
	}

	onoff = isTrue(value);
	return true;
}

// this function parses and checks xen disk parameters
bool
validate_disk_param(const char *pszDisk, int min_params, int max_params)
{
	if( !pszDisk ) {
		return false;
	}

	const char *ptr = pszDisk;
	// skip leading white spaces
	while( *ptr && ( *ptr == ' ' )) {
		ptr++;
	}

	// parse each disk
	// e.g.) disk = filename1:hda1:w, filename2:hda2:w
	StringList disk_files(ptr, ",");
	if( disk_files.isEmpty() ) {
		return false;
	}

	disk_files.rewind();
	const char *one_disk = NULL;
	while( (one_disk = disk_files.next() ) != NULL ) {

		// found disk file
		StringList single_disk_file(one_disk, ":");
        int iNumDiskParams = single_disk_file.number();
		if( iNumDiskParams < min_params || iNumDiskParams > max_params ) {
			return false;
		}
	}
	return true;
}

// this function should be called after SetVMParams() and SetRequirements
void SetVMRequirements()
{
	MyString buffer;
	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		return;
	}

	MyString vmanswer;
	vmanswer = "(";
	vmanswer += JobRequirements;
	vmanswer += ")";

	ClassAd req_ad;
	StringList job_refs;      // job attrs referenced by requirements
	StringList machine_refs;  // machine attrs referenced by requirements

		// Insert dummy values for attributes of the job to which we
		// want to detect references.  Otherwise, unqualified references
		// get classified as external references.
	req_ad.Assign(ATTR_CKPT_ARCH,"");
	req_ad.Assign(ATTR_VM_CKPT_MAC,"");

	req_ad.GetExprReferences(vmanswer.Value(),&job_refs,&machine_refs);

	// check file system domain
	if( vm_need_fsdomain ) {
		// some files don't use file transfer.
		// so we need the same file system domain
		bool checks_fsdomain = false;
		checks_fsdomain = machine_refs.contains_anycase( ATTR_FILE_SYSTEM_DOMAIN ); 

		if( !checks_fsdomain ) {
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_FILE_SYSTEM_DOMAIN;
			vmanswer += " == MY.";
			vmanswer += ATTR_FILE_SYSTEM_DOMAIN;
			vmanswer += ")";
		}
		MyString my_fsdomain;
		if(job->LookupString(ATTR_FILE_SYSTEM_DOMAIN, my_fsdomain) != 1) {
			buffer.formatstr( "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, 
					My_fs_domain ); 
			InsertJobExpr( buffer );
		}
	}

	if( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) != MATCH ) {
		// For most virtual machine programs except Xen, 
		// it's reasonable to expect the physical memory is 
		// larger than the memory for VM.
		// Especially VM universe uses Total Memory 
		// instead of Memory for Condor slot.
		// The reason why we use Total Memory is that 
		// we may want to run smaller VM universe jobs than the number of 
		// Condor slots.
		// The maximum number of VM universe jobs on a execute machine is 
		// the number of ATTR_VM_AVAIL_NUM.
		// Generally ATTR_VM_AVAIL_NUM must be less than the number of 
		// Condor slot.
		vmanswer += " && (TARGET.";
		vmanswer += ATTR_TOTAL_MEMORY;
		vmanswer += " >= ";

		MyString mem_tmp;
		mem_tmp.formatstr("%d", VMMemoryMb);
		vmanswer += mem_tmp.Value();
		vmanswer += ")";
	}

	// add vm_memory to requirements
	bool checks_vmmemory = false;
	checks_vmmemory = machine_refs.contains_anycase(ATTR_VM_MEMORY);
	if( !checks_vmmemory ) {
		vmanswer += " && (TARGET.";
		vmanswer += ATTR_VM_MEMORY;
		vmanswer += " >= ";

		MyString mem_tmp;
		mem_tmp.formatstr("%d", VMMemoryMb);
		vmanswer += mem_tmp.Value();
		vmanswer += ")";
	}

	if( VMHardwareVT ) {
		bool checks_hardware_vt = false;
		checks_hardware_vt = machine_refs.contains_anycase(ATTR_VM_HARDWARE_VT);

		if( !checks_hardware_vt ) {
			// add hardware vt to requirements
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_VM_HARDWARE_VT;
			vmanswer += ")";
		}
	}

	if( VMNetworking ) {
		bool checks_vmnetworking = false;
		checks_vmnetworking = machine_refs.contains_anycase(ATTR_VM_NETWORKING);

		if( !checks_vmnetworking ) {
			// add vm_networking to requirements
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_VM_NETWORKING;
			vmanswer += ")";
		}

		if(	VMNetworkType.IsEmpty() == false ) {
			// add vm_networking_type to requirements
			vmanswer += " && ( stringListIMember(\"";
			vmanswer += VMNetworkType.Value();
			vmanswer += "\",";
			vmanswer += "TARGET.";
			vmanswer += ATTR_VM_NETWORKING_TYPES;
			vmanswer += ",\",\")) ";
		}
	}
			
	if( VMCheckpoint ) {
		bool checks_ckpt_arch = false;
		bool checks_vm_ckpt_mac = false;
		checks_ckpt_arch = job_refs.contains_anycase( ATTR_CKPT_ARCH );
		checks_vm_ckpt_mac = job_refs.contains_anycase( ATTR_VM_CKPT_MAC );

		if( !checks_ckpt_arch ) {
			// VM checkpoint files created on AMD 
			// can not be used in INTEL. vice versa.
			vmanswer += " && ((MY.CkptArch == Arch) ||";
			vmanswer += " (MY.CkptArch =?= UNDEFINED))";
		}
		if( !checks_vm_ckpt_mac ) { 
			// VMs with the same MAC address cannot run 
			// on the same execute machine
			vmanswer += " && ((MY.VM_CkptMac =?= UNDEFINED) || ";
			vmanswer += "(TARGET.VM_All_Guest_Macs =?= UNDEFINED) || ";
			vmanswer += "( stringListIMember(MY.VM_CkptMac, ";
			vmanswer += "TARGET.VM_All_Guest_Macs, \",\") == FALSE )) ";
		}
	}

	buffer.formatstr( "%s = %s", ATTR_REQUIREMENTS, vmanswer.Value());
	JobRequirements = vmanswer;
	InsertJobExpr (buffer);
}

void
SetConcurrencyLimits()
{
	MyString tmp = condor_param_mystring(ConcurrencyLimits, NULL);
	MyString tmp2 = condor_param_mystring(ConcurrencyLimitsExpr, NULL);

	if (!tmp.IsEmpty()) {
		if (!tmp2.IsEmpty()) {
			fprintf( stderr, "ERROR: %s and %s can't be used together\n",
					 ConcurrencyLimits, ConcurrencyLimitsExpr );
			exit( 1 );
		}
		char *str;

		tmp.lower_case();

		StringList list(tmp.Value());

		char *limit;
		list.rewind();
		while ( (limit = list.next()) ) {
			double increment;
			char *limit_cpy = strdup( limit );

			if ( !ParseConcurrencyLimit(limit_cpy, increment) ) {
				fprintf( stderr,
						 "\nERROR: Invalid concurrency limit '%s'\n",
						 limit );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}
			free( limit_cpy );
		}

		list.qsort();

		str = list.print_to_string();
		if ( str ) {
			tmp.formatstr("%s = \"%s\"", ATTR_CONCURRENCY_LIMITS, str);
			InsertJobExpr(tmp.Value());

			free(str);
		}
	} else if (!tmp2.IsEmpty()) {
		std::string expr;
		formatstr( expr, "%s = %s", ATTR_CONCURRENCY_LIMITS, tmp2.Value() );
		InsertJobExpr( expr.c_str() );
	}
}


void SetAccountingGroup() {
    // is a group setting in effect?
    char* group = condor_param(AcctGroup);

    // look for the group user setting, or default to owner
    std::string group_user;
    char* gu = condor_param(AcctGroupUser);
    if ((group == NULL) && (gu == NULL)) return; // nothing set, give up

    if (NULL == gu) {
        ASSERT(owner);
        group_user = owner;
    } else {
        group_user = gu;
        free(gu);
    }

	if ( group && !IsValidSubmitterName(group) ) {
		fprintf(stderr, "\nERROR: Invalid %s: %s\n", AcctGroup, group);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	if ( !IsValidSubmitterName(group_user.c_str()) ) {
		fprintf(stderr, "\nERROR: Invalid %s: %s\n", AcctGroupUser, group_user.c_str());
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

    // set attributes AcctGroup, AcctGroupUser and AccountingGroup on the job ad:
    std::string assign;

    if (group) {
        // If we have a group, must also specify user
        formatstr(assign, "%s = \"%s.%s\"", ATTR_ACCOUNTING_GROUP, group, group_user.c_str()); 
    } else {
        // If not, this is accounting group as user alias, just set name
        formatstr(assign, "%s = \"%s\"", ATTR_ACCOUNTING_GROUP, group_user.c_str()); 
    }
    InsertJobExpr(assign.c_str());

	if (group) {
        formatstr(assign, "%s = \"%s\"", ATTR_ACCT_GROUP, group);
        InsertJobExpr(assign.c_str());
    }

    formatstr(assign, "%s = \"%s\"", ATTR_ACCT_GROUP_USER, group_user.c_str());
    InsertJobExpr(assign.c_str());

    if (group) free(group);
}


// this function must be called after SetUniverse
void
SetVMParams()
{
	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		return;
	}

	char* tmp_ptr = NULL;
	MyString buffer;

	// VM type is already set in SetUniverse
	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_VM_TYPE, VMType.Value());
	InsertJobExpr(buffer);

	// VM checkpoint is already set in SetUniverse
	buffer.formatstr( "%s = %s", ATTR_JOB_VM_CHECKPOINT, VMCheckpoint? "TRUE":"FALSE");
	InsertJobExpr(buffer);

	// VM networking is already set in SetUniverse
	buffer.formatstr( "%s = %s", ATTR_JOB_VM_NETWORKING, VMNetworking? "TRUE":"FALSE");
	InsertJobExpr(buffer);

	buffer.formatstr( "%s = %s", ATTR_JOB_VM_VNC, VMVNC? "TRUE":"FALSE");
	InsertJobExpr(buffer);
	
	// Here we need to set networking type
	if( VMNetworking ) {
		tmp_ptr = condor_param(VM_Networking_Type, ATTR_JOB_VM_NETWORKING_TYPE);
		if( tmp_ptr ) {
			VMNetworkType = tmp_ptr;
			free(tmp_ptr);

			buffer.formatstr( "%s = \"%s\"", ATTR_JOB_VM_NETWORKING_TYPE, 
					VMNetworkType.Value());
			InsertJobExpr(buffer);
		}else {
			VMNetworkType = "";
		}
	}

	// Set memory for virtual machine
	tmp_ptr = condor_param(VM_Memory);
	if( !tmp_ptr ) {
		tmp_ptr = condor_param(ATTR_JOB_VM_MEMORY);
	}
	if( !tmp_ptr ) {
		fprintf( stderr, "\nERROR: '%s' cannot be found.\n"
				"Please specify '%s' for vm universe "
				"in your submit description file.\n", VM_Memory, VM_Memory);
		DoCleanup(0,0,NULL);
		exit(1);
	}else {
		VMMemoryMb = strtol(tmp_ptr, (char **)NULL, 10);
		free(tmp_ptr);
	}
	if( VMMemoryMb <= 0 ) {
		fprintf( stderr, "\nERROR: '%s' is incorrectly specified\n"
				"For example, for vm memroy of 128 Megabytes,\n"
				"you need to use 128 in your submit description file.\n", 
				VM_Memory);
		DoCleanup(0,0,NULL);
		exit(1);
	}
	buffer.formatstr( "%s = %d", ATTR_JOB_VM_MEMORY, VMMemoryMb);
	InsertJobExpr( buffer );

	/* 
	 * Set the number of VCPUs for this virtual machine
	 */
	tmp_ptr = condor_param(VM_VCPUS, ATTR_JOB_VM_VCPUS);
	if(tmp_ptr)
	  {
	    VMVCPUS = (int)strtol(tmp_ptr, (char**)NULL,10);
	    dprintf(D_FULLDEBUG, "VCPUS = %s", tmp_ptr);
	    free(tmp_ptr);
	  }
	if(VMVCPUS <= 0)
	  {
	    VMVCPUS = 1;
	  }
	buffer.formatstr("%s = %d", ATTR_JOB_VM_VCPUS, VMVCPUS);
	InsertJobExpr(buffer);

	/*
	 * Set the MAC address for this VM.
	 */
	tmp_ptr = condor_param(VM_MACAddr, ATTR_JOB_VM_MACADDR);
	if(tmp_ptr)
	  {
	    buffer.formatstr("%s = \"%s\"", ATTR_JOB_VM_MACADDR, tmp_ptr);
	    InsertJobExpr(buffer);
	  }

	/* 
	 * When the parameter of "vm_no_output_vm" is TRUE, 
	 * Condor will not transfer VM files back to a job user. 
	 * This parameter could be used if a job user uses 
	 * explict method to get output files from VM. 
	 * For example, if a job user uses a ftp program 
	 * to send output files inside VM to his/her dedicated machine for ouput, 
	 * Maybe the job user doesn't want to get modified VM files back. 
	 * So the job user can use this parameter
	 */
	bool vm_no_output_vm = false;
	tmp_ptr = condor_param("vm_no_output_vm");
	if( tmp_ptr ) {
		parse_vm_option(tmp_ptr, vm_no_output_vm);
		free(tmp_ptr);
		if( vm_no_output_vm ) {
			buffer.formatstr( "%s = TRUE", VMPARAM_NO_OUTPUT_VM);
			InsertJobExpr( buffer );
		}
	}

	if( (strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH) ||
		(strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_KVM) == MATCH) ) {
		bool real_xen_kernel_file = false;
		bool need_xen_root_device = false;

		if ( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			// xen_kernel is a required parameter
			char *xen_kernel = NULL;
			xen_kernel = condor_param("xen_kernel");
			if( !xen_kernel ) {
				fprintf( stderr, "\nERROR: 'xen_kernel' cannot be found.\n"
						"Please specify 'xen_kernel' for the xen virtual machine "
						"in your submit description file.\n"
						"xen_kernel must be one of "
						"\"%s\", \"%s\", <file-name>.\n", 
						XEN_KERNEL_INCLUDED, XEN_KERNEL_HW_VT);
				DoCleanup(0,0,NULL);
				exit(1);
			}else {
				if ( strcasecmp(xen_kernel, XEN_KERNEL_INCLUDED) == 0) {
					// kernel image is included in a disk image file
					// so we will use bootloader(pygrub etc.) defined 
					// in a vmgahp config file on an excute machine 
					real_xen_kernel_file = false;
					need_xen_root_device = false;
				}else if ( strcasecmp(xen_kernel, XEN_KERNEL_HW_VT) == 0) {
					// A job user want to use an unmodified OS in Xen.
					// so we require hardware virtualization.
					real_xen_kernel_file = false;
					need_xen_root_device = false;
					VMHardwareVT = true;
					buffer.formatstr( "%s = TRUE", ATTR_JOB_VM_HARDWARE_VT);
					InsertJobExpr( buffer );
				}else {
					// real kernel file
					real_xen_kernel_file = true;
					need_xen_root_device = true;
				}
				InsertJobExprString(VMPARAM_XEN_KERNEL, xen_kernel);
				free(xen_kernel);
			}

			
			// xen_initrd is an optional parameter
			char *xen_initrd = NULL;
			xen_initrd = condor_param("xen_initrd");
			if( xen_initrd ) {
				if( !real_xen_kernel_file ) {
					fprintf( stderr, "\nERROR: To use xen_initrd, "
							"xen_kernel should be a real kernel file.\n");
					DoCleanup(0,0,NULL);
					exit(1);
				}
				InsertJobExprString(VMPARAM_XEN_INITRD, xen_initrd);
				free(xen_initrd);
			}

			if( need_xen_root_device ) {
				char *xen_root = NULL;
				xen_root = condor_param("xen_root");
				if( !xen_root ) {
					fprintf( stderr, "\nERROR: '%s' cannot be found.\n"
							"Please specify '%s' for the xen virtual machine "
							"in your submit description file.\n", "xen_root", 
							"xen_root");
					DoCleanup(0,0,NULL);
					exit(1);
				}else {
					InsertJobExprString(VMPARAM_XEN_ROOT, xen_root);
					free(xen_root);
				}
			}
		}// xen only params

		// <x>_disk is a required parameter
		char *disk = condor_param("vm_disk");

		if( !disk ) {
			fprintf( stderr, "\nERROR: '%s' cannot be found.\n"
					"Please specify '%s' for the virtual machine "
					"in your submit description file.\n", 
					"<vm>_disk", "<vm>_disk");
			DoCleanup(0,0,NULL);
			exit(1);
		}else {
			if( validate_disk_param(disk) == false ) 
            {
				fprintf(stderr, "\nERROR: 'vm_disk' has incorrect format.\n"
						"The format shoud be like "
						"\"<filename>:<devicename>:<permission>\"\n"
						"e.g.> For single disk: <vm>_disk = filename1:hda1:w\n"
						"      For multiple disks: <vm>_disk = "
						"filename1:hda1:w,filename2:hda2:w\n");
				DoCleanup(0,0,NULL);
				exit(1);
			}
			InsertJobExprString( VMPARAM_VM_DISK, disk );
			free(disk);
		}

		if ( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			// xen_kernel_params is a optional parameter
			char *xen_kernel_params = NULL;
			xen_kernel_params = condor_param("xen_kernel_params");
			if( xen_kernel_params ) {
				MyString fixedvalue = delete_quotation_marks(xen_kernel_params);
				InsertJobExprString( VMPARAM_XEN_KERNEL_PARAMS, xen_kernel_params );
				free(xen_kernel_params);
			}
		}

	}else if( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_VMWARE) == MATCH ) {
		bool vmware_should_transfer_files = false;
		tmp_ptr = condor_param("vmware_should_transfer_files");
		if( parse_vm_option(tmp_ptr, vmware_should_transfer_files) == false ) {
			MyString err_msg;
			err_msg = "\nERROR: You must explicitly specify "
				"\"vmware_should_transfer_files\" "
				"in your submit description file. "
				"You need to define either: "
				"\"vmware_should_transfer_files = YES\" or "
				" \"vmware_should_transfer_files = NO\". "
				"If you define \"vmware_should_transfer_files = YES\", " 
				"vmx and vmdk files in the directory of \"vmware_dir\" "
				"will be transfered to an execute machine. "
				"If you define \"vmware_should_transfer_files = NO\", "
				"all files in the directory of \"vmware_dir\" should be "
				"accessible with a shared file system\n";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit(1);
		}
		free(tmp_ptr);

		buffer.formatstr( "%s = %s", VMPARAM_VMWARE_TRANSFER, 
				vmware_should_transfer_files ? "TRUE" : "FALSE");
		InsertJobExpr( buffer );

		if( vmware_should_transfer_files == false ) {
			vm_need_fsdomain = true;
		}

		/* In default, vmware vm universe uses snapshot disks.
		 * However, when a user job is so IO-sensitive that 
		 * the user doesn't want to use snapshot, 
		 * the user must use both 
		 * vmware_should_transfer_files = YES and 
		 * vmware_snapshot_disk = FALSE
		 * In vmware_should_transfer_files = NO 
		 * snapshot disks is always used.
		 */
		bool vmware_snapshot_disk = true;
		tmp_ptr = condor_param("vmware_snapshot_disk");
		if( tmp_ptr ) {
			parse_vm_option(tmp_ptr, vmware_snapshot_disk);
			free(tmp_ptr);
		}

		if(	!vmware_should_transfer_files && !vmware_snapshot_disk ) {
			MyString err_msg;
			err_msg = "\nERROR: You should not use both "
				"vmware_should_transfer_files = FALSE and "
				"vmware_snapshot_disk = FALSE. "
				"Not using snapshot disk in a shared file system may cause "
				"problems when multiple jobs share the same disk\n";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		buffer.formatstr( "%s = %s", VMPARAM_VMWARE_SNAPSHOTDISK,
				vmware_snapshot_disk? "TRUE" : "FALSE");
		InsertJobExpr( buffer );

		// vmware_dir is a directory that includes vmx file and vmdk files.
		char *vmware_dir = NULL;
		vmware_dir = condor_param("vmware_dir");
		if ( vmware_dir ) {
			MyString f_dirname = delete_quotation_marks(vmware_dir);
			free(vmware_dir);

			f_dirname = full_path(f_dirname.Value(), false);
			check_and_universalize_path(f_dirname);

			buffer.formatstr( "%s = \"%s\"", VMPARAM_VMWARE_DIR, f_dirname.Value());
			InsertJobExpr( buffer );

			Directory dir( f_dirname.Value() );
			dir.Rewind();
			while ( dir.Next() ) {
				if ( has_suffix( dir.GetFullPath(), ".vmx" ) ||
					 vmware_should_transfer_files ) {
					// The .vmx file is always transfered.
					transfer_vm_file( dir.GetFullPath() );
				}
			}
		}

		// Look for .vmx and .vmdk files in transfer_input_files
		StringList vmx_files;
		StringList vmdk_files;
		StringList input_files( NULL, "," );
		MyString input_files_str;
		job->LookupString( ATTR_TRANSFER_INPUT_FILES, input_files_str );
		input_files.initializeFromString( input_files_str.Value() );
		input_files.rewind();
		const char *file;
		while ( (file = input_files.next()) ) {
			if ( has_suffix( file, ".vmx" ) ) {
				vmx_files.append( condor_basename( file ) );
			} else if ( has_suffix( file, ".vmdk" ) ) {
				vmdk_files.append( condor_basename( file ) );
			}
		}

		if ( vmx_files.number() == 0 ) {
			fprintf( stderr, "\nERROR: no vmx file for vmware can be found.\n" );
			DoCleanup(0,0,NULL);
			exit(1);
		} else if ( vmx_files.number() > 1 ) {
			fprintf( stderr, "\nERROR: multiple vmx files exist. "
					 "Only one vmx file should be present.\n" );
			DoCleanup(0,0,NULL);
			exit(1);
		} else {
			vmx_files.rewind();
			buffer.formatstr( "%s = \"%s\"", VMPARAM_VMWARE_VMX_FILE,
					condor_basename(vmx_files.next()));
			InsertJobExpr( buffer );
		}

		tmp_ptr = vmdk_files.print_to_string();
		if ( tmp_ptr ) {
			buffer.formatstr( "%s = \"%s\"", VMPARAM_VMWARE_VMDK_FILES, tmp_ptr);
			InsertJobExpr( buffer );
			free( tmp_ptr );
		}
	}

	// Now all VM parameters are set
	// So we need to add necessary VM attributes to Requirements
	SetVMRequirements();
}
#endif

#ifdef USE_SUBMIT_UTILS
//PRAGMA_REMIND("make a proper queue args unit test.")
int DoUnitTests(int options)
{
	return (options > 1) ? 1 : 0;
}
#else

int DoUnitTests(int options)
{
	static const struct {
		int          rval;
		const char * args;
		int          num;
		int          mode;
		int          cvars;
		int          citems;
	} trials[] = {
	// rval  args          num    mode      vars items
		{0,  "in (a b)",     1, foreach_in,   0,  2},
		{0,  "ARG in (a,b)", 1, foreach_in,   1,  2},
		{0,  "ARG in(a)",    1, foreach_in,   1,  1},
		{0,  "ARG\tin\t(a)", 1, foreach_in,   1,  1},
		{0,  "arg IN ()",    1, foreach_in,   1,  0},
		{0,  "2 ARG in ( a, b, cd )",  2, foreach_in,   1,  3},
		{0,  "100 ARG In a, b, cd",  100, foreach_in,   1,  3},
		{0,  "  ARG In a b cd e",  1, foreach_in,   1,  4},

		{0,  "matching *.dat",               1, foreach_matching,   0,  1},
		{0,  "FILE matching *.dat",          1, foreach_matching,   1,  1},
		{0,  "glob MATCHING (*.dat *.foo)",  1, foreach_matching,   1,  2},
		{0,  "glob MATCHING files (*.foo)",  1, foreach_matching_files,   1,  1},
		{0,  " dir matching */",             1, foreach_matching,   1,  1},
		{0,  " dir matching dirs */",        1, foreach_matching_dirs,   1,  1},

		{0,  "Executable,Arguments From args.lst",  1, foreach_from,   2,  0},
		{0,  "Executable,Arguments From (sleep.exe 10)",  1, foreach_from,   2,  1},
		{0,  "9 Executable Arguments From (sleep.exe 10)",  9, foreach_from,   2,  1},

		{0,  "arg from [1]  args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [:1] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [::] args.lst",     1, foreach_from,   1,  0},

		{0,  "arg from [100::] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [:100:] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [::100] args.lst",     1, foreach_from,   1,  0},

		{0,  "arg from [100:10:5] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [10:100:5] args.lst",     1, foreach_from,   1,  0},

		{0,  "",             1, 0, 0, 0},
		{0,  "2",            2, 0, 0, 0},
		{0,  "9 - 2",        7, 0, 0, 0},
		{0,  "1 -1",         0, 0, 0, 0},
		{0,  "2*2",          4, 0, 0, 0},
		{0,  "(2+3)",        5, 0, 0, 0},
		{0,  "2 * 2 + 5",    9, 0, 0, 0},
		{0,  "2 * (2 + 5)", 14, 0, 0, 0},
		{-2, "max(2,3)",    -1, 0, 0, 0},
		{0,  "max({2,3})",   3, 0, 0, 0},

	};

	for (int ii = 0; ii < (int)COUNTOF(trials); ++ii) {
		int foreach_mode=-1;
		int queue_num=-1;
		StringList vars;
		StringList items;
		qslice     slice;
		MyString   items_filename;
		char * pqargs = strdup(trials[ii].args);
		int rval = parse_queue_args(pqargs, foreach_mode, queue_num, vars, items, slice, items_filename);

		int cvars = vars.number();
		int citems = items.number();
		bool ok = (rval == trials[ii].rval && foreach_mode == trials[ii].mode && queue_num == trials[ii].num && cvars == trials[ii].cvars && citems == trials[ii].citems);

		char * vars_list = vars.print_to_string();
		if ( ! vars_list) vars_list = strdup("");

		char * items_list = items.print_to_delimed_string("|");
		if ( ! items_list) items_list = strdup("");
		else if (strlen(items_list) > 50) { items_list[50] = 0; items_list[49] = '.'; items_list[48] = '.'; items_list[46] = '.'; }

		fprintf(stderr, "%s num:   %d/%d  mode:  %d/%d  vars:  %d/%d {%s} ", ok ? " " : "!",
				queue_num, trials[ii].num, foreach_mode, trials[ii].mode, cvars, trials[ii].cvars, vars_list);
		fprintf(stderr, "  items: %d/%d {%s}", citems, trials[ii].citems, items_list);
		if (slice.initialized()) { char sz[16*3]; slice.to_string(sz, sizeof(sz)); fprintf(stderr, " slice: %s", sz); }
		if ( ! items_filename.empty()) { fprintf(stderr, " file:'%s'\n", items_filename.Value()); }
		fprintf(stderr, "\tqargs: '%s' -> '%s'\trval: %d/%d\n", trials[ii].args, pqargs, rval, trials[ii].rval);

		free(pqargs);
		free(vars_list);
		free(items_list);
	}

	/// slice tests
	static const struct {
		const char * val;
		int start; int end;
	} slices[] = {
		{ "[:]",  0, 10 },
		{ "[0]",  0, 10 },
		{ "[0:3]",  0, 10 },
		{ "[:1]",  0, 10 },
		{ "[-1]", 0, 10 },
		{ "[:-1]", 0, 10 },
		{ "[::2]", 0, 10 },
		{ "[1::2]", 0, 10 },
	};

	for (int ii = 0; ii < (int)COUNTOF(slices); ++ii) {
		qslice slice;
		slice.set(const_cast<char*>(slices[ii].val));
		fprintf(stderr, "%s : %s\t", slices[ii].val, slice.initialized() ? "init" : " no ");
		for (int ix = slices[ii].start; ix < slices[ii].end; ++ix) {
			if (slice.selected(ix, slices[ii].end)) {
				fprintf(stderr, " %d", ix);
			}
		}
		fprintf(stderr, "\n");
	}

	return (options > 1) ? 1 : 0;
}
#endif
