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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_string.h"
#include "condor_ckpt_name.h"
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
#include "condor_parser.h"
#include "condor_scanner.h"
#include "condor_distribution.h"
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#else
// WINDOWS only
#include "store_cred.h"
#endif
#include "my_hostname.h"
#include "domain_tools.h"
#include "get_daemon_name.h"
#include "condor_qmgr.h"
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
#include "../condor_schedd.V6/scheduler.h"
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"

#include "list.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "condor_md.h"

// TODO: hashFunction() is case-insenstive, but when a MyString is the
//   hash key, the comparison in HashTable is case-sensitive. Therefore,
//   the case-insensitivity of hashFunction() doesn't complish anything.
//   CheckFilesRead, CheckFilesWrite, and ClusterAdAttrs should be
//   either completely case-sensitive (and use MyStringHash()) or
//   completely case-insensitive (and use AttrKey and AttrKeyHashFunction).
static unsigned int hashFunction( const MyString& );
#include "file_sql.h"

HashTable<AttrKey,MyString> forcedAttributes( 64, AttrKeyHashFunction );
HashTable<MyString,int> CheckFilesRead( 577, hashFunction ); 
HashTable<MyString,int> CheckFilesWrite( 577, hashFunction ); 
HashTable<MyString,int> ClusterAdAttrs( 31, hashFunction );

// Explicit template instantiation

/* For daemonCore, etc. */
DECL_SUBSYSTEM( "SUBMIT", SUBSYSTEM_TYPE_SUBMIT );

ClassAd  *job = NULL;

char	*OperatingSystem;
char	*Architecture;
char	*Spool;
char	*ScheddName = NULL;
char	*PoolName = NULL;
DCSchedd* MySchedd = NULL;
char	*My_fs_domain;
MyString    JobRequirements;
#if !defined(WIN32)
MyString    JobRootdir;
#endif
MyString    JobIwd;

int		LineNo;
int		ExtraLineNo;
int		GotQueueCommand;
SandboxTransferMethod	STMethod = STM_USE_SCHEDD_ONLY;

MyString	IckptName;	/* Pathname of spooled initial ckpt file */

unsigned int TransferInputSize;	/* total size of files transfered to exec machine */
const char	*MyName;
int		Quiet = 1;
int		WarnOnUnusedMacros = 1;
int		DisableFileChecks = 0;
int		MaxProcsPerCluster;
int	  ClusterId = -1;
int	  ProcId = -1;
int	  JobUniverse;
char *JobGridType = NULL;
int		Remote=0;
int		ClusterCreated = FALSE;
int		ActiveQueueConnection = FALSE;
bool    nice_user_setting = false;
bool	NewExecutable = false;
bool	IsFirstExecutable;
bool	UserLogSpecified = false;
bool    UseXMLInLog = false;
ShouldTransferFiles_t should_transfer = STF_NO;
bool stream_stdout_toggle = false;
bool stream_stderr_toggle = false;
bool    NeedsPerFileEncryption = false;
bool	NeedsJobDeferral = false;
bool job_ad_saved = false;	// should we deallocate the job ad after storing it?
bool HasTDP = false;
char* tdp_cmd = NULL;
char* tdp_input = NULL;
#if defined(WIN32)
char* RunAsOwnerCredD = NULL;
#endif

// For vm universe
MyString VMType;
int VMMemory = 0;
int VMVCPUS = 0;
MyString VMMACAddr;
bool VMCheckpoint = false;
bool VMNetworking = false;
MyString VMNetworkType;
bool VMHardwareVT = false;
bool vm_need_fsdomain = false;
bool xen_has_file_to_be_transferred = false;

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

List<char> extraLines;  // lines passed in via -a argument

#define PROCVARSIZE	32
BUCKET *ProcVars[ PROCVARSIZE ];

#define MEG	(1<<20)

//
// Dump ClassAd to file junk
//
MyString	DumpFileName;
bool		DumpClassAdToFile = false;
FILE		*DumpFile = NULL;

/*
**	Names of possible CONDOR variables.
*/
const char	*Cluster 		= "cluster";
const char	*Process			= "process";
const char	*Hold			= "hold";
const char	*Priority		= "priority";
const char	*Notification	= "notification";
const char	*WantRemoteIO	= "want_remote_io";
const char	*Executable		= "executable";
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

const char	*RequestCpus	= "request_cpus";
const char	*RequestMemory	= "request_memory";
const char	*RequestDisk	= "request_disk";

const char	*Universe		= "universe";
const char	*Grid_Type		= "grid_type";
const char	*MachineCount	= "machine_count";
const char	*NotifyUser		= "notify_user";
const char	*EmailAttributes = "email_attributes";
const char	*ExitRequirements = "exit_requirements";
const char	*UserLogFile	= "log";
const char    *UseLogUseXML   = "log_xml";
const char	*CoreSize		= "coresize";
const char	*NiceUser		= "nice_user";

const char	*GridResource	= "grid_resource";
const char	*X509UserProxy	= "x509userproxy";
const char	*GlobusScheduler = "globusscheduler";
const char	*GlobusJobmanagerType = "jobmanager_type";
const char    *GridShell = "gridshell";
const char	*GlobusRSL = "globus_rsl";
const char	*GlobusXML = "globus_xml";
const char	*NordugridRSL = "nordugrid_rsl";
const char	*RemoteSchedd = "remote_schedd";
const char	*RemotePool = "remote_pool";
const char	*RendezvousDir	= "rendezvousdir";
const char	*UnicoreUSite = "unicore_u_site";
const char 	*UnicoreVSite = "unicore_v_site";
const char	*KeystoreFile = "keystore_file";
const char	*KeystoreAlias = "keystore_alias";
const char	*KeystorePassphraseFile = "keystore_passphrase_file";

const char	*FileRemaps = "file_remaps";
const char	*BufferFiles = "buffer_files";
const char	*BufferSize = "buffer_size";
const char	*BufferBlockSize = "buffer_block_size";

const char	*FetchFiles = "fetch_files";
const char	*CompressFiles = "compress_files";
const char	*AppendFiles = "append_files";
const char	*LocalFiles = "local_files";

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
const char	*TransferFiles = "transfer_files";
const char	*TransferExecutable = "transfer_executable";
const char	*TransferInput = "transfer_input";
const char	*TransferOutput = "transfer_output";
const char	*TransferError = "transfer_error";

const char	*EncryptInputFiles = "encrypt_input_files";
const char	*EncryptOutputFiles = "encrypt_output_files";
const char	*DontEncryptInputFiles = "dont_encrypt_input_files";
const char	*DontEncryptOutputFiles = "dont_encrypt_output_files";

const char	*StreamInput = "stream_input";
const char	*StreamOutput = "stream_output";
const char	*StreamError = "stream_error";

const char	*CopyToSpool = "copy_to_spool";
const char	*LeaveInQueue = "leave_in_queue";

const char	*PeriodicHoldCheck = "periodic_hold";
const char	*PeriodicReleaseCheck = "periodic_release";
const char	*PeriodicRemoveCheck = "periodic_remove";
const char	*OnExitHoldCheck = "on_exit_hold";
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

//
// CronTab Parameters
// The index value below should be the # of parameters
//
const int CronFields = 5;
const char	*CronMinute		= "cron_minute";
const char	*CronHour		= "cron_hour";
const char	*CronDayOfMonth	= "cron_day_of_month";
const char	*CronMonth		= "cron_month";
const char	*CronDayOfWeek	= "cron_day_of_week";
const char	*CronWindow		= "cron_window";
const char	*CronPrepTime	= "cron_prep_time";

const char	*RunAsOwner = "run_as_owner";
#if defined(WIN32)
const char	*LoadProfile = "load_profile";
#endif

// Concurrency Limit parameters
const char    *ConcurrencyLimits = "concurrency_limits";

//
// VM universe Parameters
//
const char    *VM_Type = "vm_type";
const char    *VM_Memory = "vm_memory";
const char    *VM_VCPUS = "vm_vcpus";
const char    *VM_MACAddr = "vm_macaddr";
const char    *VM_Checkpoint = "vm_checkpoint";
const char    *VM_Networking = "vm_networking";
const char    *VM_Networking_Type = "vm_networking_type";

//
// Amazon EC2 Parameters
//
const char* AmazonPublicKey = "amazon_public_key";
const char* AmazonPrivateKey = "amazon_private_key";
const char* AmazonAmiID = "amazon_ami_id";
const char* AmazonUserData = "amazon_user_data";
const char* AmazonUserDataFile = "amazon_user_data_file";
const char* AmazonSecurityGroups = "amazon_security_groups";
const char* AmazonKeyPairFile = "amazon_keypair_file";
const char* AmazonInstanceType = "amazon_instance_type";

char const *next_job_start_delay = "next_job_start_delay";
char const *next_job_start_delay2 = "NextJobStartDelay";

const char * REMOTE_PREFIX="Remote_";

#if !defined(WIN32)
const char	*KillSig			= "kill_sig";
const char	*RmKillSig			= "remove_kill_sig";
const char	*HoldKillSig		= "hold_kill_sig";
#endif

void    SetSimpleJobExprs();
void	SetRemoteAttrs();
void 	reschedule();
void 	SetExecutable();
void 	SetUniverse();
void	SetMachineCount();
void 	SetImageSize();
int 	calc_image_size( const char *name);
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
void 	SetArguments();
void 	SetJobDeferral();
void 	SetEnvironment();
#if !defined(WIN32)
void 	ComputeRootDir();
void 	SetRootDir();
#endif
void 	SetRequirements();
void 	SetTransferFiles();
void    process_input_file_list(StringList * input_list, char *input_files, bool * files_specified);
void 	SetPerFileEncryption();
bool 	SetNewTransferFiles( void );
void 	SetOldTransferFiles( bool, bool );
void	InsertFileTransAttrs( FileTransferOutput_t when_output );
void 	SetTDP();
void	SetRunAsOwner();
#if defined(WIN32)
void    SetLoadProfile();
#endif
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
void 	check_iwd( char const *iwd );
int	read_condor_file( FILE *fp );
char * 	condor_param( const char* name, const char* alt_name );
char * 	condor_param( const char* name ); // call param with NULL as the alt
void 	set_condor_param( const char* name, const char* value);
void 	set_condor_param_used( const char* name);
void 	queue(int num);
bool 	check_requirements( char const *orig, MyString &answer );
void 	check_open( const char *name, int flags );
void 	usage();
void 	init_params();
int 	whitespace( const char *str);
void 	delete_commas( char *ptr );
void 	compress( MyString &path );
char const*full_path(const char *name, bool use_iwd=true);
void 	log_submit();
void 	get_time_conv( int &hours, int &minutes );
int	  SaveClassAd ();
void	InsertJobExpr (const char *expr, bool clustercheck = true);
void	InsertJobExpr (const MyString &expr, bool clustercheck = true);
void	InsertJobExprInt(const char * name, int val, bool clustercheck = true);
void	InsertJobExprString(const char * name, const char * val, bool clustercheck = true);
void	check_umask();
void setupAuthentication();
void	SetPeriodicHoldCheck(void);
void	SetPeriodicRemoveCheck(void);
void	SetExitHoldCheck(void);
void	SetExitRemoveCheck(void);
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
void SetVMParams();
void SetVMRequirements();
bool parse_vm_option(char *value, bool& onoff);
void transfer_vm_file(const char *filename);
bool make_vm_file_path(const char *filename, MyString& fixedname);
bool validate_xen_disk_parm(const char *xen_disk, MyString &fixedname);

char *owner = NULL;
char *ntdomain = NULL;
char *myproxy_password = NULL;
bool stream_std_file = false;

extern DLL_IMPORT_MAGIC char **environ;


extern "C" {
int SetSyscalls( int foo );
int DoCleanup(int,int,char*);
}

struct SubmitRec {
	int cluster;
	int firstjob;
	int lastjob;
	char *logfile;
	char *lognotes;
	char *usernotes;
};

ExtArray <SubmitRec> SubmitInfo(10);
int CurrentSubmitInfo = -1;

ExtArray <ClassAd*> JobAdsArray(100);
int JobAdsArrayLen = 0;

// explicit template instantiations

MyString 
condor_param_mystring( const char * name, const char * alt_name )
{
	char * result = condor_param(name, alt_name);
	MyString ret = result;
	free(result);
	return ret;
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


void
init_job_ad()
{
	MyString buffer;
	ASSERT(owner);

	if ( job ) {
		delete job;
		job = NULL;
	}

	// set up types of the ad
	if ( !job ) {
		job = new ClassAd();
		job->SetMyTypeName (JOB_ADTYPE);
		job->SetTargetTypeName (STARTD_ADTYPE);
	}

	buffer.sprintf( "%s = %d", ATTR_Q_DATE, (int)time ((time_t *) 0));
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_COMPLETION_DATE);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = \"%s\"", ATTR_OWNER, owner);
	InsertJobExpr (buffer);

#ifdef WIN32
	// put the NT domain into the ad as well
	ntdomain = my_domainname();
	buffer.sprintf( "%s = \"%s\"", ATTR_NT_DOMAIN, ntdomain);
	InsertJobExpr (buffer);

	// Publish the version of Windows we are running
	OSVERSIONINFOEX os_version_info;
	ZeroMemory ( &os_version_info, sizeof ( OSVERSIONINFOEX ) );
	os_version_info.dwOSVersionInfoSize = sizeof ( OSVERSIONINFOEX );
	BOOL ok = GetVersionEx ( (OSVERSIONINFO*) &os_version_info );
	if ( !ok ) {
		os_version_info.dwOSVersionInfoSize =
			sizeof ( OSVERSIONINFO );
		ok = GetVersionEx ( (OSVERSIONINFO*) &os_version_info );
		if ( !ok ) {
			dprintf ( D_ALWAYS, "Submit: failed to "
				"get Windows version information\n" );
		}
	}
	if ( ok ) {
		buffer.sprintf( "%s = %u", ATTR_WINDOWS_MAJOR_VERSION, 
			os_version_info.dwMajorVersion );
		InsertJobExpr ( buffer );
		buffer.sprintf( "%s = %u", ATTR_WINDOWS_MINOR_VERSION, 
			os_version_info.dwMinorVersion );
		InsertJobExpr ( buffer );
		buffer.sprintf( "%s = %u", ATTR_WINDOWS_BUILD_NUMBER, 
			os_version_info.dwBuildNumber );
		InsertJobExpr ( buffer );
		// publish the extended Windows version information if we
		// have it at our disposal
		if ( sizeof ( OSVERSIONINFOEX ) ==
			os_version_info.dwOSVersionInfoSize ) {
			buffer.sprintf ( "%s = %lu",
				ATTR_WINDOWS_SERVICE_PACK_MAJOR,
				os_version_info.wServicePackMajor );
			InsertJobExpr ( buffer );
			buffer.sprintf ( "%s = %lu",
				ATTR_WINDOWS_SERVICE_PACK_MINOR,
				os_version_info.wServicePackMinor );
			InsertJobExpr ( buffer );
			buffer.sprintf ( "%s = %lu",
				ATTR_WINDOWS_PRODUCT_TYPE,
				os_version_info.wProductType );
			InsertJobExpr ( buffer );
		}
	}
#endif

	buffer.sprintf( "%s = 0.0", ATTR_JOB_REMOTE_WALL_CLOCK);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0.0", ATTR_JOB_LOCAL_USER_CPU);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0.0", ATTR_JOB_LOCAL_SYS_CPU);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0.0", ATTR_JOB_REMOTE_USER_CPU);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0.0", ATTR_JOB_REMOTE_SYS_CPU);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_JOB_EXIT_STATUS);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_NUM_CKPTS);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_NUM_JOB_STARTS);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_NUM_RESTARTS);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_NUM_SYSTEM_HOLDS);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_JOB_COMMITTED_TIME);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_TOTAL_SUSPENSIONS);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_LAST_SUSPENSION_TIME);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = 0", ATTR_CUMULATIVE_SUSPENSION_TIME);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = FALSE", ATTR_ON_EXIT_BY_SIGNAL);
	InsertJobExpr (buffer);

	config_fill_ad( job );
}

int
main( int argc, char *argv[] )
{
	FILE	*fp;
	char	**ptr;
	char	*cmd_file = NULL;
	int i;
	MyString method;

	setbuf( stdout, NULL );

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

		// If our effective and real gids are different (b/c the
		// submit binary is setgid) set umask(002) so that stdout,
		// stderr and the user log files are created writable by group
		// condor.
	check_umask();

	DebugFlags |= D_EXPR;

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, (SIG_HANDLER)SIG_IGN );
#endif

#if defined(WIN32)
	bool query_credential = true;
#endif

	for( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if ( match_prefix( ptr[0], "-verbose" ) ) {
				Quiet = 0;
			} else if ( match_prefix( ptr[0], "-disable" ) ) {
				DisableFileChecks = 1;
			} else if ( match_prefix( ptr[0], "-debug" ) ) {
				// dprintf to console
				Termlog = 1;
				dprintf_config( "TOOL" );
			} else if ( match_prefix( ptr[0], "-spool" ) ) {
				Remote++;
				DisableFileChecks = 1;
			} else if ( match_prefix( ptr[0], "-remote" ) ) {
				Remote++;
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
#if defined(WIN32)
				query_credential = false;
#endif
			} else if ( match_prefix( ptr[0], "-name" ) ) {
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
#if defined(WIN32)
				query_credential = false;
#endif
			} else if ( match_prefix( ptr[0], "-append" ) ) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -append requires another argument\n",
							 MyName );
					exit( 1 );
				}
				extraLines.Append( *ptr );
			} else if ( match_prefix( ptr[0], "-password" ) ) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -password requires another argument\n",
							 MyName );
				}
				myproxy_password = strdup (*ptr);
			} else if ( match_prefix( ptr[0], "-pool" ) ) {
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
			} else if ( match_prefix( ptr[0], "-stm" ) ) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -stm requires another argument\n",
							 MyName );
					exit(1);
				}
				method = *ptr;
				string_to_stm(method, STMethod);
			} else if ( match_prefix( ptr[0], "-unused" ) ) {
				WarnOnUnusedMacros = WarnOnUnusedMacros == 1 ? 0 : 1;
				// TOGGLE? 
				// -- why not? if you set it to warn on unused macros in the 
				// config file, there should be a way to turn it off
			} else if ( match_prefix( ptr[0], "-dump" ) ) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -dump requires another argument\n",
						MyName );
					exit(1);
				}
				DumpFileName = *ptr;
				DumpClassAdToFile = true;
#if defined ( WIN32 )
				// we don't really want to do this because there is no real 
				// schedd to query the credentials from...
				query_credential = false;
#endif				
			} else if ( match_prefix( ptr[0], "-help" ) ) {
				usage();
				exit( 0 );
			} else {
				usage();
				exit( 1 );
			}
		} else {
			cmd_file = *ptr;
		}
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

	char *dis_check = param("SUBMIT_SKIP_FILECHECKS");
	if ( dis_check ) {
		if (dis_check[0]=='T' || dis_check[0]=='t') {
			DisableFileChecks = 1;
		}
		free(dis_check);
	}

	// if we are dumping to a file, we never want to check file permissions
	// as this would initiate some schedd communication
	if ( DumpClassAdToFile ) {
		DisableFileChecks = 1;
	}

	MaxProcsPerCluster = param_integer("SUBMIT_MAX_PROCS_IN_CLUSTER", 0, 0);

	// we don't want a schedd instance if we are dumping to a file
	if ( !DumpClassAdToFile ) {
		// Instantiate our DCSchedd so we can locate and communicate
		// with our schedd.  
		MySchedd = new DCSchedd( ScheddName, PoolName );
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

#ifdef WIN32

	// make sure our shadow will have access to our credential
	// (check is disabled for "-n" and "-r" submits)
	if (query_credential) {

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
	}
#endif
	
	// open submit file
	if ( !cmd_file ) {
		// no file specified, read from stdin
		fp = stdin;
	} else {
		if( (fp=safe_fopen_wrapper(cmd_file,"r")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open command file (%s)\n",
						strerror(errno));
			exit(1);
		}
	}

	// in case things go awry ...
	_EXCEPT_Cleanup = DoCleanup;

	IsFirstExecutable = true;
	init_job_ad();	

	if ( !DumpClassAdToFile ) {
		if (Quiet) {
			fprintf(stdout, "Submitting job(s)");
		}
	} else {
		fprintf(stdout, "Storing job ClassAd(s)");
	}

	// open the file we are to dump the ClassAds to...
	if ( DumpClassAdToFile ) {
		if( (DumpFile=safe_fopen_wrapper(DumpFileName.Value(),"w")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open file to dump ClassAds into (%s)\n",
				strerror(errno));
			exit(1);
		}
	}

	//  Parse the file and queue the jobs
	if( read_condor_file(fp) < 0 ) {
		if( ExtraLineNo == 0 ) {
			fprintf( stderr,
					 "\nERROR: Failed to parse command file (line %d).\n",
					 LineNo);
		}
		else {
			fprintf( stderr,
					 "\nERROR: Failed to parse -a argument line (#%d).\n",
					 ExtraLineNo );
		}
		exit(1);
	}

	if( !GotQueueCommand ) {
		fprintf(stderr, "\nERROR: \"%s\" doesn't contain any \"queue\"",
				cmd_file ? cmd_file : "(stdin)" );
		fprintf( stderr, " commands -- no jobs queued\n" );
		exit( 1 );
	}

	// we can't disconnect from something if we haven't connected to it: since
	// we are dumping to a file, we don't actually open a connection to the schedd
	if ( !DumpClassAdToFile ) {
		if ( !DisconnectQ(0) ) {
			fprintf(stderr, "\nERROR: Failed to commit job submission into the queue.\n");
			exit(1);
		}
	}

	if (Quiet) {
		fprintf(stdout, "\n");
	}

	if (!DumpClassAdToFile) {
		if (UserLogSpecified) {
			log_submit();
		}
	}

	if (Quiet) {
		int this_cluster = -1, job_count=0;
		for (i=0; i <= CurrentSubmitInfo; i++) {
			if (SubmitInfo[i].cluster != this_cluster) {
				if (this_cluster != -1) {
					fprintf(stdout, "%d job(s) submitted to cluster %d.\n",
							job_count, this_cluster);
					job_count = 0;
				}
				this_cluster = SubmitInfo[i].cluster;
			}
			job_count += SubmitInfo[i].lastjob - SubmitInfo[i].firstjob + 1;
		}
		if (this_cluster != -1) {
			fprintf(stdout, "%d job(s) submitted to cluster %d.\n",
					job_count, this_cluster);
		}
	}

	ActiveQueueConnection = FALSE; 

	if ( !DisableFileChecks ) {
		TestFilePermissions( MySchedd->addr() );
	}

	// we don't want to spool jobs if we are simply writing the ClassAds to 
	// a file, so we just skip this block entirely if we are doing this...
	if ( !DumpClassAdToFile ) {
		if ( Remote && JobAdsArrayLen > 0 ) {
			bool result;
			CondorError errstack;

			switch(STMethod) {
				case STM_USE_SCHEDD_ONLY:
					// perhaps check for proper schedd version here?
					result = MySchedd->spoolJobFiles( JobAdsArrayLen,
											  JobAdsArray.getarray(),
											  &errstack );
					if ( !result ) {
						fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
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
						fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
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
						fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
						fprintf( stderr, "ERROR: Failed to spool job files.\n" );
						exit(1);
					}

					} // end block

					break;

				default:
					EXCEPT("PROGRAMMER ERROR: Unknown sandbox transfer method\n");
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

	/*	print all of the parameters that were not actually expanded/used 
		in the submit file */
	if (WarnOnUnusedMacros) {
		if (!Quiet) { fprintf(stdout, "\n"); }
		HASHITER it = hash_iter_begin(ProcVars, PROCVARSIZE);
		for ( ; !hash_iter_done(it); hash_iter_next(it) ) {
			if(0 == hash_iter_used_value(it)) {
				char *key = hash_iter_key(it),
					 *val = hash_iter_value(it);
				fprintf(stderr, "WARNING: the line `%s = %s' was unused by condor_submit. Is it a typo?\n", key, val);
			}
		}
		
	}

	return 0;
}

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
		{ Grid_Type, 0, ATTR_JOB_GRID_TYPE },
		{ RemoteSchedd, 0, ATTR_REMOTE_SCHEDD },
		{ RemotePool, 0, ATTR_REMOTE_POOL },
		{ GlobusScheduler, "globus_scheduler", ATTR_GLOBUS_RESOURCE },
		{ GlobusRSL, "globus_rsl", ATTR_GLOBUS_RSL },
		{ GlobusXML, "globus_xml", ATTR_GLOBUS_XML },
		{ NordugridRSL, "nordugrid_rsl", ATTR_NORDUGRID_RSL },
		{ GridResource, 0, ATTR_GRID_RESOURCE },
	};
	const int tostringizesz = sizeof(tostringize) / sizeof(tostringize[0]);


	HASHITER it = hash_iter_begin(ProcVars, PROCVARSIZE);
	for( ; ! hash_iter_done(it); hash_iter_next(it)) {

		char * key = hash_iter_key(it);
		int remote_depth = 0;
		while(strincmp(key, REMOTE_PREFIX, REMOTE_PREFIX_LEN) == 0) {
			remote_depth++;
			key += REMOTE_PREFIX_LEN;
		}

		if(remote_depth == 0) {
			continue;
		}

		// remote_schedd and remote_pool have remote_ in front of them. :-/
		// Special case to detect them. 
		char * possible_key = key - REMOTE_PREFIX_LEN;
		if(stricmp(possible_key, RemoteSchedd) == 0 ||
			stricmp(possible_key, RemotePool) == 0) {
			remote_depth--;
			key = possible_key;
		}

		if(remote_depth == 0) {
			continue;
		}

		MyString preremote = "";
		for(int i = 0; i < remote_depth; ++i) {
			preremote += REMOTE_PREFIX;
		}

		if(stricmp(key, Universe) == 0 || stricmp(key, ATTR_JOB_UNIVERSE) == 0) {
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

				if(	stricmp(key, item.submit_expr) &&
					(item.special_expr == NULL || stricmp(key, item.special_expr)) &&
					stricmp(key, item.job_expr)) {
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
			
			if (stricmp(my_name, net_name) != 0 ) {
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

	// In vm universe and ec2 grid jobs, 'Executable' parameter is not
	// a real file but just the name of job.
	if ( JobUniverse == CONDOR_UNIVERSE_VM ||
		 ( JobUniverse == CONDOR_UNIVERSE_GRID &&
		   JobGridType != NULL &&
		   stricmp( JobGridType, "amazon" ) == MATCH ) ) {
		ignore_it = true;
	}

	ename = condor_param( Executable, ATTR_JOB_CMD );
	if( ename == NULL ) {
		fprintf( stderr, "No '%s' parameter was provided\n", Executable);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	macro_value = condor_param( TransferExecutable, ATTR_TRANSFER_EXECUTABLE );
	if ( macro_value ) {
		if ( macro_value[0] == 'F' || macro_value[0] == 'f' ) {
			buffer.sprintf( "%s = FALSE", ATTR_TRANSFER_EXECUTABLE );
			InsertJobExpr( buffer );
			transfer_it = false;
		}
		free( macro_value );
	}

	if ( ignore_it ) {
		if( transfer_it == true ) {
			buffer.sprintf( "%s = FALSE", ATTR_TRANSFER_EXECUTABLE );
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

	buffer.sprintf( "%s = \"%s\"", ATTR_JOB_CMD, full_ename.Value());
	InsertJobExpr (buffer);

		/* MPI REALLY doesn't like these! */
	if ( JobUniverse != CONDOR_UNIVERSE_MPI && JobUniverse != CONDOR_UNIVERSE_PVM ) {
		InsertJobExpr ("MinHosts = 1");
		InsertJobExpr ("MaxHosts = 1");
	} 

	if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
		InsertJobExpr ("WantIOProxy = TRUE");
	}

	InsertJobExpr ("CurrentHosts = 0");

	switch(JobUniverse) 
	{
	case CONDOR_UNIVERSE_STANDARD:
		buffer.sprintf( "%s = TRUE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		buffer.sprintf( "%s = TRUE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	case CONDOR_UNIVERSE_PVM:
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_MPI:  // for now
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_GRID:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_VM:
		buffer.sprintf( "%s = FALSE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		buffer.sprintf( "%s = FALSE", ATTR_WANT_CHECKPOINT);
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
	IckptName = gen_ckpt_name(0,ClusterId,ICKPT,0);

	// ensure the executables exist and spool them only if no 
	// $$(arch).$$(opsys) are specified  (note that if we are simply
	// dumping the class-ad to a file, we won't actually transfer
	// or do anything [nothing that follows will affect the ad])
	if ( !strstr(ename,"$$") && transfer_it && !DumpClassAdToFile ) {

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
						md5.sprintf_cat("%02x", static_cast<int>(md5_raw[i]));
					}
					free(md5_raw);
				}
			}
			int ret;
			if (!md5.IsEmpty()) {
				ClassAd tmp_ad;
				tmp_ad.Assign(ATTR_OWNER, owner);
				tmp_ad.Assign(ATTR_JOB_CMD_MD5, md5.Value());
				ret = SendSpoolFileIfNeeded(tmp_ad);
			}
			else {
				ret = SendSpoolFile(IckptName.Value());
			}

			if (ret < 0) {
				fprintf( stderr,
				         "\nERROR: Request to transfer executable %s failed\n",
				         IckptName.Value() );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}

			// ret will be 0 if the SchedD gave us the go-ahead to send
			// the file. if it's not, the SchedD is using ickpt sharing
			// and already has a copy, so no need
			//
			if ((ret == 0) && SendSpoolFileBytes(full_path(ename,false)) < 0) {
				fprintf( stderr,
						 "\nERROR: failed to transfer executable file %s\n", 
						 ename );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}
		}

	}

	free(ename);
	free(copySpool);
}

#ifdef WIN32
#define CLIPPED
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

	if( univ && stricmp(univ,"scheduler") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_SCHEDULER;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_SCHEDULER);
		InsertJobExpr (buffer);
		free(univ);
		return;
	};

	if( univ && stricmp(univ,"local") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_LOCAL;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE,
						CONDOR_UNIVERSE_LOCAL );
		InsertJobExpr( buffer );
		free( univ );
		return;
	};

#if !defined(WIN32)
	if( univ && stricmp(univ,"pvm") == MATCH ) 
	{
		char *pvmd = param("PVMD");

		if (!pvmd || access(pvmd, R_OK|X_OK) != 0) {
			fprintf(stderr, "\nERROR: Condor PVM support is not installed.\n"
					"You must install the Condor PVM Contrib Module before\n"
					"submitting PVM universe jobs\n");
			if (!pvmd) {
				fprintf(stderr, "PVMD parameter not defined in the Condor "
						"configuration file.\n");
			} else {
				fprintf(stderr, "Can't access %s: %s\n", pvmd,
						strerror(errno));
			}
			exit(1);
		}

		JobUniverse = CONDOR_UNIVERSE_PVM;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_PVM);
		InsertJobExpr (buffer);

		free(univ);
		return;
	};

#endif // !defined(WIN32)

	if( univ && 
		((stricmp(univ,"globus") == MATCH) || (stricmp(univ,"grid") == MATCH))) {
		JobUniverse = CONDOR_UNIVERSE_GRID;
		
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID);
		InsertJobExpr (buffer);
		free(univ);
		univ = 0;
	
		// Set Grid_Type
		// Check both grid_type and grid_resource. If the latter one starts
		// with '$$(', then we're matchmaking and don't know the grid-type.
		// If both are blank, check globus_scheduler and its variants. If
		// one of them exists, then this is an old gt2 submit file.
		// Otherwise, we matchmaking and don't know the grid-type.
		// If grid_resource exists, we ignore grid_type.
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
			JobGridType = condor_param( Grid_Type, ATTR_JOB_GRID_TYPE );
			if ( !JobGridType ) {
				char *tmp = condor_param( GlobusScheduler,
										  "globus_scheduler" );
				if ( tmp == NULL ) {
					tmp = condor_param( ATTR_GLOBUS_RESOURCE, NULL );
				}
				if ( tmp ) {
					JobGridType = strdup( "gt2" );
					free( tmp );
				}
			}
		}
		if ( JobGridType ) {
			// Validate
			// Valid values are (as of 6.7): nordugrid, globus,
			//    gt2, infn, condor

			// CRUFT: grid-type 'blah' is deprecated. Now, the specific batch
			//   system names should be used (pbs, lsf). Glite are the only
			//   people who care about the old value. This changed happend in
			//   Condor 6.7.12.
			if ((stricmp (JobGridType, "gt2") == MATCH) ||
				(stricmp (JobGridType, "gt5") == MATCH) ||
				(stricmp (JobGridType, "gt4") == MATCH) ||
				(stricmp (JobGridType, "infn") == MATCH) ||
				(stricmp (JobGridType, "blah") == MATCH) ||
				(stricmp (JobGridType, "pbs") == MATCH) ||
				(stricmp (JobGridType, "lsf") == MATCH) ||
				(stricmp (JobGridType, "nqs") == MATCH) ||
				(stricmp (JobGridType, "naregi") == MATCH) ||
				(stricmp (JobGridType, "condor") == MATCH) ||
				(stricmp (JobGridType, "nordugrid") == MATCH) ||
				(stricmp (JobGridType, "amazon") == MATCH) ||	// added for amazon job
				(stricmp (JobGridType, "unicore") == MATCH) ||
				(stricmp (JobGridType, "cream") == MATCH)){
				// We're ok	
				// Values are case-insensitive for gridmanager, so we don't need to change case			
			} else if ( stricmp( JobGridType, "globus" ) == MATCH ) {
				// Convert 'globus' to 'gt2'
				free( JobGridType );
				JobGridType = strdup( "gt2" );
			} else {

				fprintf( stderr, "\nERROR: Invalid value '%s' for grid_type\n", JobGridType );
				fprintf( stderr, "Must be one of: globus, gt2, gt4, condor, nordugrid, unicore, or cream\n" );
				exit( 1 );
			}
		}			
		
			// Setting ATTR_JOB_GRID_TYPE in the job ad has been moved to
			// SetGlobusParams().
		
		return;
	};

	if( univ && stricmp(univ,"parallel") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_PARALLEL;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_PARALLEL);
		InsertJobExpr (buffer);
		
		free(univ);
		return;
	}

	if( univ && stricmp(univ,"vanilla") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_VANILLA;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA);
		InsertJobExpr (buffer);
		free(univ);
		return;
	};

	if( univ && stricmp(univ,"mpi") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_MPI;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_MPI);
		InsertJobExpr (buffer);
		
		free(univ);
		return;
	}

	if( univ && stricmp(univ,"java") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_JAVA;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_JAVA);
		InsertJobExpr (buffer);
		free(univ);
		return;
	}

	if( univ && stricmp(univ,"vm") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_VM;
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VM);
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


	if( univ && stricmp(univ,"standard") == MATCH ) {
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
		buffer.sprintf( "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_STANDARD);
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
	char	*ptr;
	MyString buffer;
	int		request_cpus = 1;

	if (JobUniverse == CONDOR_UNIVERSE_PVM) {

		mach_count = condor_param( MachineCount, "MachineCount" );

		int tmp;
		if (mach_count != NULL) {
			for (ptr = mach_count; *ptr && *ptr != '.'; ptr++) ;
			if (*ptr != '\0') {
				*ptr = '\0';
				ptr++;
			}

			tmp = atoi(mach_count);
			buffer.sprintf( "%s = %d", ATTR_MIN_HOSTS, tmp);
			InsertJobExpr (buffer);
			
			for ( ; !isdigit(*ptr) && *ptr; ptr++) ;
			if (*ptr != '\0') {
				tmp = atoi(ptr);
			}

			buffer.sprintf( "%s = %d", ATTR_MAX_HOSTS, tmp);
			InsertJobExpr (buffer);
			free(mach_count);
		} else {
			InsertJobExpr ("MinHosts = 1");
			InsertJobExpr ("MaxHosts = 1");
		}

		request_cpus = 1;
	} else if (JobUniverse == CONDOR_UNIVERSE_MPI ||
			   JobUniverse == CONDOR_UNIVERSE_PARALLEL ) {

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

		buffer.sprintf( "%s = %d", ATTR_MIN_HOSTS, tmp);
		InsertJobExpr (buffer);
		buffer.sprintf( "%s = %d", ATTR_MAX_HOSTS, tmp);
		InsertJobExpr (buffer);

		request_cpus = 1;
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

			buffer.sprintf( "%s = %d", ATTR_MACHINE_COUNT, tmp);
			InsertJobExpr (buffer);

			request_cpus = tmp;
		}
	}

	if ((mach_count = condor_param(RequestCpus, ATTR_REQUEST_CPUS))) {
		buffer.sprintf("%s = %s", ATTR_REQUEST_CPUS, mach_count);
		free(mach_count);
	} else {
		buffer.sprintf("%s = %d", ATTR_REQUEST_CPUS, request_cpus);
	}
	InsertJobExpr(buffer);
}

struct SimpleExprInfo {
	char const *name1;
	char const *name2;
	char const *default_value;
};

/* This function is used to handle submit file commands that are inserted
 * into the job ClassAd verbatim, with no special treatment.
 */
void
SetSimpleJobExprs()
{
	SimpleExprInfo simple_exprs[] = {
		{next_job_start_delay, next_job_start_delay2, NULL},
		{NULL,NULL,NULL}
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
		buffer.sprintf( "%s = %s", ATTR_NEXT_JOB_START_DELAY, expr);
		InsertJobExpr (buffer);

		free( expr );
	}
}

// Note: you must call SetTransferFiles() *before* calling SetImageSize().
void
SetImageSize()
{
	int		size;
	unsigned int disk_usage = 0;
	static int executablesize;
	char	*tmp;
	char	*p;
	char    buff[2048];
	MyString buffer;

	tmp = condor_param( ImageSize, ATTR_IMAGE_SIZE );

	// we should only call calc_image_size on the first
	// proc in the cluster, since the executable cannot change.
	if ( ProcId < 1 ) {
		ASSERT (job->LookupString (ATTR_JOB_CMD, buff));
		if( JobUniverse == CONDOR_UNIVERSE_VM ) { 
			executablesize = 0;
		}else {
			executablesize = calc_image_size( buff );
		}
	}

	size = executablesize;

	if( tmp ) {
		size = atoi( tmp );
		for( p=tmp; *p && isdigit(*p); p++ )
			;
		while( isspace(*p) ) {
			p++;
		}
		if( *p == 'm' || *p == 'M' ) {
			size *=  1024;
		}
		free( tmp );
		if( size < 1 ) {
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

	buffer.sprintf( "%s = %d", ATTR_IMAGE_SIZE, size);
	InsertJobExpr (buffer);

	buffer.sprintf( "%s = %u", ATTR_EXECUTABLE_SIZE, 
					executablesize);
	InsertJobExpr (buffer);

	if( JobUniverse == CONDOR_UNIVERSE_VM) {
		// In vm universe, when a VM is suspended, 
		// memory being used by the VM will be saved into a file. 
		// So we need as much disk space as the memory.
		int vm_disk_space = executablesize + TransferInputSize + VMMemory*1024;

		// In vmware vm universe, vmware disk may be a sparse disk or 
		// snapshot disk. So we can't estimate the disk space in advanace 
		// because the sparse disk or snapshot disk will grow up 
		// as a VM runs. So we will add 100MB to disk space.
		if( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_VMWARE) == MATCH ) {
			vm_disk_space += 100*1024;
		}
		buffer.sprintf( "%s = %u", ATTR_DISK_USAGE, vm_disk_space);
	}else {
		tmp = condor_param( DiskUsage, ATTR_DISK_USAGE );

		if( tmp ) {
			disk_usage = atoi(tmp);

			if( disk_usage < 1 ) {
				fprintf( stderr, "\nERROR: disk_usage must be >= 1\n" );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}
			free( tmp );
		} else {
			disk_usage = executablesize + TransferInputSize;
		}

		buffer.sprintf( "%s = %u", ATTR_DISK_USAGE, disk_usage );
	}
	InsertJobExpr (buffer);


	if ((tmp = condor_param(RequestMemory, ATTR_REQUEST_MEMORY))) {
		buffer.sprintf("%s = %s", ATTR_REQUEST_MEMORY, tmp);
		free(tmp);
	} else {
		buffer.sprintf("%s = ceiling(ifThenElse(%s =!= UNDEFINED, %s, %s/1024.0))",
					   ATTR_REQUEST_MEMORY,
					   ATTR_JOB_VM_MEMORY,
					   ATTR_JOB_VM_MEMORY,
					   ATTR_IMAGE_SIZE);
	}
	InsertJobExpr(buffer);

	if ((tmp = condor_param(RequestDisk, ATTR_REQUEST_DISK))) {
		buffer.sprintf("%s = %s", ATTR_REQUEST_DISK, tmp);
		free(tmp);
	} else {
		buffer.sprintf("%s = %s", ATTR_REQUEST_DISK, ATTR_DISK_USAGE);
	}
	InsertJobExpr(buffer);
}

void SetFileOptions()
{
	char *tmp;
	MyString strbuffer;

	tmp = condor_param( FileRemaps, ATTR_FILE_REMAPS );
	if(tmp) {
		strbuffer.sprintf("%s = %s",ATTR_FILE_REMAPS,tmp);
		InsertJobExpr(strbuffer);
		free(tmp);
	}

	tmp = condor_param( BufferFiles, ATTR_BUFFER_FILES );
	if(tmp) {
		strbuffer.sprintf("%s = %s",ATTR_BUFFER_FILES,tmp);
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
	strbuffer.sprintf("%s = %s",ATTR_BUFFER_SIZE,tmp);
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
	strbuffer.sprintf("%s = %s",ATTR_BUFFER_BLOCK_SIZE,tmp);
	InsertJobExpr(strbuffer.Value());
	free(tmp);
}


/*
** Make a wild guess at the size of the image represented by this a.out.
** Should add up the text, data, and bss sizes, then allow something for
** the stack.  But how we gonna do that if the executable is for some
** other architecture??  Our answer is in kilobytes.
*/
int
calc_image_size( const char *name)
{
	struct stat	buf;

	if ( IsUrl( name ) ) {
		return 0;
	}

	if( stat(full_path(name),&buf) < 0 ) {
		// EXCEPT( "Cannot stat \"%s\"", name );
		return 0;
	}
	return (buf.st_size + 1023) / 1024;
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
			TransferInputSize += calc_image_size(tmp.Value());
		}
		if ( count ) {
			tmp_ptr = input_list->print_to_string();
			input_files->sprintf( "%s = \"%s\"",
				ATTR_TRANSFER_INPUT_FILES, tmp_ptr );
			free( tmp_ptr );
			*files_specified = true;
		}
	}
}

// Note: SetTransferFiles() sets a global variable TransferInputSize which
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

	should_transfer = STF_YES;

	macro_value = condor_param( TransferInputFiles, "TransferInputFiles" ) ;
	TransferInputSize = 0;
	if( macro_value ) {
		input_file_list.initializeFromString( macro_value );
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


	macro_value = condor_param( TransferOutputFiles,
								"TransferOutputFiles" ); 
	if( macro_value ) 
	{
		output_file_list.initializeFromString(macro_value);
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
			(void) output_files.sprintf ("%s = \"%s\"", 
				ATTR_TRANSFER_OUTPUT_FILES, tmp_ptr);
			free(tmp_ptr);
			tmp_ptr = NULL;
			out_files_specified = true;
		}
		free(macro_value);
	}

		// now that we've gathered up all the possible info on files
		// the user explicitly wants to transfer, see if they set the
		// right attributes controlling if and when to do the
		// transfers.  if they didn't tell us what to do, in some
		// cases, we can give reasonable defaults, but in others, it's
		// a fatal error.  first we check for the new attribute names
		// ("ShouldTransferFiles" and "WheToTransferOutput").  If
		// those aren't defined, we look for the old "TransferFiles". 
	if( ! SetNewTransferFiles() ) {
		SetOldTransferFiles( in_files_specified, out_files_specified );
	}

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
			TransferInputSize += calc_image_size(tdp_cmd);
			if(file_list[0]) {
				file_list_tdp += ",";
			}
			file_list_tdp += tdp_cmd;
			changed_it = true;
		}
		if( tdp_input && (!strstr(file_list, tdp_input)) ) {
			TransferInputSize += calc_image_size(tdp_input);
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
			StringList files(macro_value);
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
		b.sprintf("%s = FALSE", ATTR_TRANSFER_EXECUTABLE);
		InsertJobExpr( b.Value() );
	}

	// If either stdout or stderr contains path information, and the
	// file is being transfered back via the FileTransfer object,
	// substitute a safe name to use in the sandbox and stash the
	// original name in the output file "remaps".  The FileTransfer
	// object will take care of transferring the data back to the
	// correct path.

	if ( (should_transfer != STF_NO && JobUniverse != CONDOR_UNIVERSE_GRID &&
		  JobUniverse != CONDOR_UNIVERSE_STANDARD) ||
		 Remote ) {

		MyString output;
		MyString error;

		job->LookupString(ATTR_JOB_OUTPUT,output);
		job->LookupString(ATTR_JOB_ERROR,error);

		if(output.Length() && output != condor_basename(output.Value()) && 
		   strcmp(output.Value(),"/dev/null") != 0 && !stream_stdout_toggle)
		{
			char const *working_name = "_condor_stdout";
				//Force setting value, even if we have already set it
				//in the cluster ad, because whatever was in the
				//cluster ad may have been overwritten (e.g. by a
				//filename containing $(Process)).  At this time, the
				//check in InsertJobExpr() is not smart enough to
				//notice that.
			InsertJobExprString(ATTR_JOB_OUTPUT, working_name,false);

			if(!output_remaps.IsEmpty()) output_remaps += ";";
			output_remaps.sprintf_cat("%s=%s",working_name,output.EscapeChars(";=\\",'\\').Value());
		}

		if(error.Length() && error != condor_basename(error.Value()) && 
		   strcmp(error.Value(),"/dev/null") != 0 && !stream_stderr_toggle)
		{
			char const *working_name = "_condor_stderr";

			if(error == output) {
				//stderr will use same file as stdout
				working_name = "_condor_stdout";
			}
				//Force setting value, even if we have already set it
				//in the cluster ad, because whatever was in the
				//cluster ad may have been overwritten (e.g. by a
				//filename containing $(Process)).  At this time, the
				//check in InsertJobExpr() is not smart enough to
				//notice that.
			InsertJobExprString(ATTR_JOB_ERROR, working_name,false);

			if(!output_remaps.IsEmpty()) output_remaps += ";";
			output_remaps.sprintf_cat("%s=%s",working_name,error.EscapeChars(";=\\",'\\').Value());
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
			MyString err_msg;
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
		expr.sprintf("%s = \"%s\"",ATTR_TRANSFER_OUTPUT_REMAPS,output_remaps.Value());
		InsertJobExpr(expr);
	}

		// Check accessibility of output files.
	output_file_list.rewind();
	char const *output_file;
	while ( (output_file=output_file_list.next()) ) {
		// Apply filename remaps if there are any.
		MyString remap_fname;
		if(filename_remap_find(output_remaps.Value(),output_file,remap_fname)) {
			output_file = remap_fname.Value();
		}

		check_open(output_file, O_WRONLY|O_CREAT|O_TRUNC );
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

bool
SetNewTransferFiles( void )
{
	bool found_it = false;
	char *should, *when;
	FileTransferOutput_t when_output = FTO_NONE;
	MyString err_msg;
	
	should = condor_param( ATTR_SHOULD_TRANSFER_FILES, 
						   "should_transfer_files" );
	if( should ) {
		should_transfer = getShouldTransferFilesNum( should );
		if( should_transfer < 0 ) {
			err_msg = "\nERROR: invalid value (\"";
			err_msg += should;
			err_msg += "\") for ";
			err_msg += ATTR_SHOULD_TRANSFER_FILES;
			err_msg += ".  Please either specify \"YES\", \"NO\", or ";
			err_msg += "\"IF_NEEDED\" and try again.";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		found_it = true;
	}
	
	when = condor_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, 
						 "when_to_transfer_output" );
	if( when ) {
		when_output = getFileTransferOutputNum( when );
		if( when_output < 0 ) {
			err_msg = "\nERROR: invalid value (\"";
			err_msg += when;
			err_msg += "\") for ";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += ".  Please either specify \"ON_EXIT\", or ";
			err_msg += "\"ON_EXIT_OR_EVICT\" and try again.";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
			// if they gave us WhenToTransferOutput, but they didn't
			// specify ShouldTransferFiles yet, give them a default of
			// "YES", since that's the safest option.
		if( ! found_it ) {
			should_transfer = STF_YES;
		}
		if( should_transfer == STF_NO ) {
			err_msg = "\nERROR: you specified ";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += " yet you defined ";
			err_msg += ATTR_SHOULD_TRANSFER_FILES;
			err_msg += " to be \"";
			err_msg += should;
			err_msg += "\".  Please remove this contradiction from ";
			err_msg += "your submit file and try again.";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		found_it = true;
	} else {
		if( found_it && should_transfer != STF_NO ) {
			err_msg = "\nERROR: you specified ";
			err_msg += ATTR_SHOULD_TRANSFER_FILES;
			err_msg += " to be \"";
			err_msg += should;
			err_msg += "\" but you did not specify *when* you want Condor "
				"to transfer the output back.  Please put either \"";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += " = ON_EXIT\" or \"";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += " = ON_EXIT_OR_EVICT\" in your submit file and "
				"try again.";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
	}
	
	if( found_it && when_output == FTO_ON_EXIT_OR_EVICT && 
		            should_transfer == STF_IF_NEEDED ) {
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
		print_wrapped_text( err_msg.Value(), stderr );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

		// if we found the new syntax, we're done, and we should now
		// add the appropriate ClassAd attributes to the job.
	if( found_it ) {
		InsertFileTransAttrs( when_output );
	}
	return found_it;
}


void
SetOldTransferFiles( bool in_files_specified, bool out_files_specified )
{
	char *macro_value;
	FileTransferOutput_t when_output = FTO_NONE;
		// this variable is never used (even once we pass it to
		// InsertFileTransAttrs()) unless should_transfer is set
		// appropriately.  however, just to be safe, we initialize it
		// here to avoid passing around uninitialized variables.

	macro_value = condor_param( TransferFiles, ATTR_TRANSFER_FILES );
	if( macro_value ) {
		// User explicitly specified TransferFiles; do what user says
		switch ( macro_value[0] ) {
				// Handle "Never"
			case 'n':
			case 'N':
				// Handle "Never"
				if( in_files_specified || out_files_specified ) {
					MyString err_msg;
					err_msg += "\nERROR: you specified \"";
					err_msg += TransferFiles;
					err_msg += " = Never\" but listed files you want "
						"transfered via \"";
					if( in_files_specified ) {
						err_msg += "transfer_input_files";
						if( out_files_specified ) {
							err_msg += "\" and \"transfer_output_files\".";
						} else {
							err_msg += "\".";
						}
					} else {
						ASSERT( out_files_specified );
						err_msg += "transfer_output_files\".";
					}
					err_msg += "  Please remove this contradiction from "
						"your submit file and try again.";
					print_wrapped_text( err_msg.Value(), stderr );
					DoCleanup(0,0,NULL);
					exit( 1 );
				}
				should_transfer = STF_NO;
				break;
			case 'o':
			case 'O':
				// Handle "OnExit"
				should_transfer = STF_YES;
				when_output = FTO_ON_EXIT;
				break;
			case 'a':
			case 'A':
				// Handle "Always"
				should_transfer = STF_YES;
				when_output = FTO_ON_EXIT_OR_EVICT;
				break;
			default:
				// Unrecognized
				fprintf( stderr, "\nERROR: Unrecognized argument for "
						 "parameter \"%s\"\n", TransferFiles );
				DoCleanup(0,0,NULL);
				exit( 1 );
				break;
		}	// end of switch

		free(macro_value);		// condor_param() calls malloc; free it!
	} else {
		// User did not explicitly specify TransferFiles; choose a default
#ifdef WIN32
		should_transfer = STF_YES;
		when_output = FTO_ON_EXIT;
#else
		if( in_files_specified || out_files_specified ) {
			MyString err_msg;
			err_msg += "\nERROR: you specified files you want Condor to "
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
			err_msg += " but you did not specify *when* you want Condor to "
				"transfer the files.  Please put either \"";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += " = ON_EXIT\" or \"";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += " = ON_EXIT_OR_EVICT\" in your submit file and "
				"try again.";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		should_transfer = STF_NO;
#endif
	}
		// now that we know what we want, call a shared method to
		// actually insert the right classad attributes for it (since
		// this stuff is shared, regardless of the old or new syntax).
	InsertFileTransAttrs( when_output );
}


void
InsertFileTransAttrs( FileTransferOutput_t when_output )
{
	MyString should = ATTR_SHOULD_TRANSFER_FILES;
	should += " = \"";
	MyString when = ATTR_WHEN_TO_TRANSFER_OUTPUT;
	when += " = \"";
	MyString ft = ATTR_TRANSFER_FILES;
	ft += " = \"";
	
	should += getShouldTransferFilesString( should_transfer );
	should += '"';
	if( should_transfer != STF_NO ) {
		if( ! when_output ) {
			EXCEPT( "InsertFileTransAttrs() called we might transfer "
					"files but when_output hasn't been set" );
		}
		when += getFileTransferOutputString( when_output );
		when += '"';
		if( when_output == FTO_ON_EXIT ) {
			ft += "ONEXIT\"";
		} else {
			ft += "ALWAYS\"";
		}
	} else {
		ft += "NEVER\"";
	}
	InsertJobExpr( should.Value() );
	if( should_transfer != STF_NO ) {
		InsertJobExpr( when.Value() );
	} else if (!Remote) {
		MyString never_create_sandbox = ATTR_NEVER_CREATE_JOB_SANDBOX;
		never_create_sandbox += " = true";
		InsertJobExpr( never_create_sandbox.Value() );
	}
	InsertJobExpr( ft.Value() );
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
			strbuffer.sprintf("%s = \"%s\"",ATTR_JOB_JAVA_VM_ARGS1,
						   value.EscapeChars("\"",'\\').Value());
			InsertJobExpr (strbuffer);
		}
	}
	else {
		args_success = args.GetArgsStringV2Raw(&value,&error_msg);
		if(!value.IsEmpty()) {
			strbuffer.sprintf("%s = \"%s\"",ATTR_JOB_JAVA_VM_ARGS2,
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
		strbuffer.sprintf( "%s = \"%s\"", ATTR_JOB_INPUT, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( macro_value, O_RDONLY );
			strbuffer.sprintf( "%s = %s", ATTR_STREAM_INPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
		} else {
			strbuffer.sprintf( "%s = FALSE", ATTR_TRANSFER_INPUT );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	case 1:
		strbuffer.sprintf( "%s = \"%s\"", ATTR_JOB_OUTPUT, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			strbuffer.sprintf( "%s = %s", ATTR_STREAM_OUTPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
			stream_stdout_toggle = stream_it;
		} else {
			strbuffer.sprintf( "%s = FALSE", ATTR_TRANSFER_OUTPUT );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	case 2:
		strbuffer.sprintf( "%s = \"%s\"", ATTR_JOB_ERROR, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			strbuffer.sprintf( "%s = %s", ATTR_STREAM_ERROR, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
			stream_stderr_toggle = stream_it;
		} else {
			strbuffer.sprintf( "%s = FALSE", ATTR_TRANSFER_ERROR );
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

	if( hold && (hold[0] == 'T' || hold[0] == 't') ) {
		buffer.sprintf( "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer, false);

		buffer.sprintf( "%s=\"submitted on hold at user's request\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );

		buffer.sprintf( "%s = %d", ATTR_HOLD_REASON_CODE,
				 CONDOR_HOLD_CODE_SubmittedOnHold );
		InsertJobExpr( buffer );
	} else 
	if ( Remote ) {
		buffer.sprintf( "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer, false);

		buffer.sprintf( "%s=\"Spooling input data files\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );

		buffer.sprintf( "%s = %d", ATTR_HOLD_REASON_CODE,
				 CONDOR_HOLD_CODE_SpoolingInput );
		InsertJobExpr( buffer );
	} else {
		buffer.sprintf( "%s = %d", ATTR_JOB_STATUS, IDLE);
		InsertJobExpr (buffer, false);
	}

	buffer.sprintf( "%s = %d", ATTR_ENTERED_CURRENT_STATUS,
					(int)time(0) );
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
	buffer.sprintf( "%s = %d", ATTR_JOB_PRIO, prioval);
	InsertJobExpr (buffer);

	// also check if the job is "dirt user" priority (i.e., nice_user==True)
	char *nice_user = condor_param( NiceUser, ATTR_NICE_USER );
	if( nice_user && (*nice_user == 'T' || *nice_user == 't') )
	{
		buffer.sprintf( "%s = TRUE", ATTR_NICE_USER );
		InsertJobExpr( buffer );
		free(nice_user);
		nice_user_setting = true;
	}
	else
	{
		buffer.sprintf( "%s = FALSE", ATTR_NICE_USER );
		InsertJobExpr( buffer );
		nice_user_setting = false;
	}
}

void
SetMaxJobRetirementTime()
{
	// Assume that SetPriority() has been called before getting here.
	char *value = NULL;

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
		expr.sprintf("%s = %s",ATTR_MAX_JOB_RETIREMENT_TIME, value);
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
		buffer.sprintf( "%s = FALSE", ATTR_PERIODIC_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.sprintf( "%s = %s", ATTR_PERIODIC_HOLD_CHECK, phc );
		free(phc);
	}

	InsertJobExpr( buffer );

	phc = condor_param(PeriodicReleaseCheck, ATTR_PERIODIC_RELEASE_CHECK);

	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.sprintf( "%s = FALSE", ATTR_PERIODIC_RELEASE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.sprintf( "%s = %s", ATTR_PERIODIC_RELEASE_CHECK, phc );
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
		buffer.sprintf( "%s = FALSE", ATTR_PERIODIC_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.sprintf( "%s = %s", ATTR_PERIODIC_REMOVE_CHECK, prc );
		free(prc);
	}

	InsertJobExpr( buffer );
}

void
SetExitHoldCheck(void)
{
	char *ehc = condor_param(OnExitHoldCheck, ATTR_ON_EXIT_HOLD_CHECK);
	MyString buffer;

	if (ehc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.sprintf( "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.sprintf( "%s = %s", ATTR_ON_EXIT_HOLD_CHECK, ehc );
		free(ehc);
	}

	InsertJobExpr( buffer );
}

void
SetLeaveInQueue()
{
	char *erc = condor_param(LeaveInQueue, ATTR_JOB_LEAVE_IN_QUEUE);
	MyString buffer;

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		if ( !Remote ) {
			buffer.sprintf( "%s = FALSE", ATTR_JOB_LEAVE_IN_QUEUE );
		} else {
				/* if remote spooling, leave in the queue after the job completes
				   for up to 10 days, so user can grab the output.
				 */
			buffer.sprintf( 
				"%s = %s == %d && (%s =?= UNDEFINED || %s == 0 || ((CurrentTime - %s) < %d))",
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
		buffer.sprintf( "%s = %s", ATTR_JOB_LEAVE_IN_QUEUE, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
}


void
SetExitRemoveCheck(void)
{
	char *erc = condor_param(OnExitRemoveCheck, ATTR_ON_EXIT_REMOVE_CHECK);
	MyString buffer;

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.sprintf( "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.sprintf( "%s = %s", ATTR_ON_EXIT_REMOVE_CHECK, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
}

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
		buffer.sprintf( "%s = %s", ATTR_JOB_NOOP, noop );
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
		buffer.sprintf( "%s = %s", ATTR_JOB_NOOP_EXIT_SIGNAL, noop );
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
		buffer.sprintf( "%s = %s", ATTR_JOB_NOOP_EXIT_CODE, noop );
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
		buffer.sprintf( "%s = TRUE", 
						ATTR_WANT_REMOTE_IO);
	} else {
		buffer.sprintf( "%s = %s", ATTR_WANT_REMOTE_IO, 
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

	if( (how == NULL) || (stricmp(how, "COMPLETE") == 0) ) {
		notification = NOTIFY_COMPLETE;
	} 
	else if( stricmp(how, "NEVER") == 0 ) {
		notification = NOTIFY_NEVER;
	} 
	else if( stricmp(how, "ALWAYS") == 0 ) {
		notification = NOTIFY_ALWAYS;
	} 
	else if( stricmp(how, "ERROR") == 0 ) {
		notification = NOTIFY_ERROR;
	} 
	else {
		fprintf( stderr, "\nERROR: Notification must be 'Never', "
				 "'Always', 'Complete', or 'Error'\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	buffer.sprintf( "%s = %d", ATTR_JOB_NOTIFICATION, notification);
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
			if( !stricmp(who, "false") ) {
				needs_warning = true;
			}
			if( !stricmp(who, "never") ) {
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
		buffer.sprintf( "%s = \"%s\"", ATTR_NOTIFY_USER, who);
		InsertJobExpr (buffer);
		free(who);
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
			buffer.sprintf( "%s = \"%s\"", ATTR_EMAIL_ATTRIBUTES, tmp );
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
	int ctr;
	char *param = NULL;
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
			buffer.sprintf( "%s = \"%s\"", CronTab::attributes[ctr], param );
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
		buffer.sprintf( "%s = %d", ATTR_LAST_MATCH_LIST_LENGTH, len );
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
		buffer.sprintf( "%s = \"%s\"", ATTR_DAG_NODE_NAME, name );
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
		(void) buffer.sprintf( "%s = \"%s\"", ATTR_DAGMAN_JOB_ID, id );
		InsertJobExpr( buffer );
		free( id );
	}
}

void
SetLogNotes()
{
	LogNotesVal = condor_param( LogNotesCommand );
	// just in case the user forgets the underscores
	if( !LogNotesVal ) {
		LogNotesVal = condor_param( "SubmitEventNotes" );
	}
}

void
SetUserNotes()
{
	UserNotesVal = condor_param( UserNotesCommand, "SubmitEventUserNotes" );
}

void
SetRemoteInitialDir()
{
	char *who = condor_param( RemoteInitialDir, ATTR_JOB_REMOTE_IWD );
	MyString buffer;
	if (who) {
		buffer.sprintf( "%s = \"%s\"", ATTR_JOB_REMOTE_IWD, who);
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
		strbuffer.sprintf("%s = \"%s\"",ATTR_JOB_ARGUMENTS1,
					   value.EscapeChars("\"",'\\').Value());
	}
	else {
		args_success = arglist.GetArgsStringV2Raw(&value,&error_msg);
		strbuffer.sprintf("%s = \"%s\"",ATTR_JOB_ARGUMENTS2,
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
		buffer.sprintf( "%s = %s", ATTR_DEFERRAL_TIME, temp );
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
			buffer.sprintf(  "%s = %s", ATTR_DEFERRAL_WINDOW, temp );	
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			buffer.sprintf( "%s = %d",
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
			buffer.sprintf(  "%s = %s", ATTR_DEFERRAL_PREP_TIME, temp );	
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			buffer.sprintf( "%s = %d",
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
			buffer.sprintf( "%s = %s", ATTR_SCHEDD_INTERVAL, temp );
			free( temp );
		} else {
			buffer.sprintf( "%s = %d", ATTR_SCHEDD_INTERVAL,
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

	//There may already be environment info in the ClassAd from SUBMIT_EXPRS.
	//Check for that now.

	bool ad_contains_env1 = job->Lookup(ATTR_JOB_ENVIRONMENT1);
	bool ad_contains_env2 = job->Lookup(ATTR_JOB_ENVIRONMENT2);

	bool MyCondorVersionRequiresV1 = false;
	if ( !DumpClassAdToFile ) {
		MyCondorVersionRequiresV1 = envobject.InputWasV1() 
			|| envobject.CondorVersionRequiresV1(MySchedd->version());
	}
	bool insert_env1 = MyCondorVersionRequiresV1;
	bool insert_env2 = !insert_env1;

	if(!env1 && !env2 && envobject.Count() == 0 && \
	   (ad_contains_env1 || ad_contains_env2)) {
			// User did not specify any environment, but SUBMIT_EXPRS did.
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
		newenv.sprintf("%s = \"%s\"",
					   ATTR_JOB_ENVIRONMENT1,
					   newenv_raw.EscapeChars("\"",'\\').Value());
		InsertJobExpr(newenv);

		// Record in the JobAd the V1 delimiter that is being used.
		// This way remote submits across platforms have a prayer.
		MyString delim_assign;
		delim_assign.sprintf("%s = \"%c\"",
							 ATTR_JOB_ENVIRONMENT1_DELIM,
							 envobject.GetEnvV1Delimiter());
		InsertJobExpr(delim_assign);
	}

	if(insert_env2 && env_success) {
		MyString newenv;
		MyString newenv_raw;

		env_success = envobject.getDelimitedStringV2Raw(&newenv_raw,&error_msg);
		newenv.sprintf("%s = \"%s\"",
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
	buffer.sprintf( "%s = \"%s\"", ATTR_JOB_ROOT_DIR, JobRootdir.Value());
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
	buffer.sprintf( "%s = %s", ATTR_REQUIREMENTS, tmp.Value());
	JobRequirements = tmp;

	InsertJobExpr (buffer);
	
	char* fs_domain = NULL;
	if( (should_transfer == STF_NO || should_transfer == STF_IF_NEEDED) 
		&& ! job->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &fs_domain) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, 
				 My_fs_domain ); 
		InsertJobExpr( buffer );
	}
	if( fs_domain ) {
		free( fs_domain );
	}
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
		buf.sprintf( "%s = \"%s\"", ATTR_TOOL_DAEMON_CMD, path.Value() );
		InsertJobExpr( buf.Value() );
	}
	if( tdp_input ) {
		path = tdp_input;
		check_and_universalize_path( path );
		buf.sprintf( "%s = \"%s\"", ATTR_TOOL_DAEMON_INPUT, path.Value() );
		InsertJobExpr( buf.Value() );
	}
	if( tdp_output ) {
		path = tdp_output;
		check_and_universalize_path( path );
		buf.sprintf( "%s = \"%s\"", ATTR_TOOL_DAEMON_OUTPUT, path.Value() );
		InsertJobExpr( buf.Value() );
		free( tdp_output );
		tdp_output = NULL;
	}
	if( tdp_error ) {
		path = tdp_error;
		check_and_universalize_path( path );
		buf.sprintf( "%s = \"%s\"", ATTR_TOOL_DAEMON_ERROR, path.Value() );
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
			buf.sprintf("%s = \"%s\"",ATTR_TOOL_DAEMON_ARGS1,
						args_value.EscapeChars("\"",'\\').Value());
			InsertJobExpr( buf );
		}
	}
	else if(args.Count()) {
		args_success = args.GetArgsStringV2Raw(&args_value,&error_msg);
		if(!args_value.IsEmpty()) {
			buf.sprintf("%s = \"%s\"",ATTR_TOOL_DAEMON_ARGS2,
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
		buf.sprintf( "%s = %s", ATTR_SUSPEND_JOB_AT_EXEC,
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
	if (run_as_owner == NULL) {
		return;
	}

	MyString buffer;
	buffer.sprintf(  "%s = %s", ATTR_JOB_RUNAS_OWNER, isTrue(run_as_owner) ? "True" : "False" );
	InsertJobExpr (buffer);
	free(run_as_owner);

#if defined(WIN32)
	// make sure we have a CredD
	// (RunAsOwner is global for use in SetRequirements(),
	//  the memory is freed() there)
	RunAsOwnerCredD = param("CREDD_HOST");
	if(RunAsOwnerCredD == NULL) {
		fprintf(stderr,
				"\nERROR: run_as_owner requires a valid CREDD_HOST configuration macro\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}
#endif
}

#if defined(WIN32)
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
    buffer.sprintf ( "%s = True", ATTR_JOB_LOAD_PROFILE );
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
#endif

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
		buffer.sprintf( "%s = 0.0", ATTR_RANK );
		InsertJobExpr( buffer );
	} else {
		buffer.sprintf( "%s = %s", ATTR_RANK, rank.Value() );
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
				iwd.sprintf( "%s%c%s", cwd.Value(), DIR_DELIM_CHAR, shortname );
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
	buffer.sprintf( "%s = \"%s\"", ATTR_JOB_IWD, JobIwd.Value());
	InsertJobExpr (buffer);
}

void
check_iwd( char const *iwd )
{
	MyString pathname;

#if defined(WIN32)
	pathname = iwd;
#else
	pathname.sprintf( "%s/%s", JobRootdir.Value(), iwd );
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
	char *ulog_entry = condor_param( UserLogFile, ATTR_ULOG_FILE );
	MyString buffer;

	if (ulog_entry) {
		
		MyString ulog = full_path(ulog_entry);

		if ( !DumpClassAdToFile ) {
			
			free(ulog_entry);

			// check that the log is a valid path
			if ( !DisableFileChecks ) {
				FILE* test = safe_fopen_wrapper(ulog.Value(), "a+", 0664);
				if (!test) {
					fprintf(stderr,
						"\nERROR: Invalid log file: \"%s\" (%s)\n", ulog.Value(),
						strerror(errno));
					exit( 1 );
				} else {
					fclose(test);
				}
			}

			// Check that the log file isn't on NFS
			BOOLEAN nfs_is_error = param_boolean("LOG_ON_NFS_IS_ERROR", false);
			BOOLEAN	nfs = FALSE;

			if ( fs_detect_nfs( ulog.Value(), &nfs ) != 0 ) {
				fprintf(stderr,
					"\nWARNING: Can't determine whether log file %s is on NFS\n",
					ulog.Value() );
			} else if ( nfs ) {
				if ( nfs_is_error ) {

					fprintf(stderr,
						"\nERROR: Log file %s is on NFS.\nThis could cause"
						" log file corruption. Condor has been configured to"
						" prohibit log files on NFS.\n",
						ulog.Value() );

					DoCleanup(0,0,NULL);
					exit( 1 );

				} 
			}
		}

		check_and_universalize_path(ulog);
		buffer.sprintf( "%s = \"%s\"", ATTR_ULOG_FILE, ulog.Value());
		InsertJobExpr(buffer);
		UserLogSpecified = true;
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
			buffer.sprintf( "%s = TRUE", ATTR_ULOG_USE_XML);
			InsertJobExpr( buffer );
		} else {
			buffer.sprintf( "%s = FALSE", ATTR_ULOG_USE_XML);
		}
		free(use_xml);
	}
	return;
}

#if defined(ALPHA)
	char	*CoreSizeFmt = "CONDOR_CORESIZE=%ld";
#else
	char	*CoreSizeFmt = "CONDOR_CORESIZE=%d";
#endif


void
SetCoreSize()
{
	char *size = condor_param( CoreSize, "core_size" );
	long coresize;
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

	buffer.sprintf( "%s = %ld", ATTR_CORE_SIZE, coresize);
	InsertJobExpr(buffer);
}


void
SetJobLease( void )
{
	static bool warned_too_small = false;
	long lease_duration = 0;
	char *tmp = condor_param( "job_lease_duration", ATTR_JOB_LEASE_DURATION );
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
			lease_duration = 20 * 60;
		} else {
				// not defined and can't reconnect, we're done.
			return;
		}
	} else {
		char *endptr = NULL;
		lease_duration = strtol(tmp, &endptr, 10);
		if (endptr != tmp) {
			while (isspace(*endptr)) {
				endptr++;
			}
		}
		bool valid = (endptr != tmp && *endptr == '\0');
		if (!valid) {
			fprintf(stderr, "\nERROR: invalid %s given: %s\n",
					ATTR_JOB_LEASE_DURATION, tmp);
			DoCleanup(0, 0, NULL);
			exit(1);
		}
		if (lease_duration == 0) {
				// User explicitly didn't want a lease, so we're done.
			free(tmp);
			return;
		}
		else if (lease_duration < 20) {
			if (! warned_too_small) { 
				fprintf(stderr, "\nWARNING: %s less than 20 seconds is not "
						"allowed, using 20 instead\n",
						ATTR_JOB_LEASE_DURATION);
				warned_too_small = true;
			}
			lease_duration = 20;
		}
		free(tmp);
	}
	MyString val = ATTR_JOB_LEASE_DURATION;
	val += "=";
	val += lease_duration;
	InsertJobExpr( val.Value() );
}


void
SetForcedAttributes()
{
	AttrKey		name;
	MyString	value;
	char		*exValue;
	MyString buffer;

	forcedAttributes.startIterations();
	while( ( forcedAttributes.iterate( name, value ) ) )
	{
		// expand the value; and insert it into the job ad
		exValue = expand_macro( value.Value(), ProcVars, PROCVARSIZE );
		if( !exValue )
		{
			fprintf( stderr, "\nWarning: Unable to expand macros in \"%s\"."
					 "  Ignoring.\n", value.Value() );
			continue;
		}
		buffer.sprintf( "%s = %s", name.value(), exValue );
			// Call InserJobExpr with checkcluster set to false.
			// This will result in forced attributes always going
			// into the proc ad, not the cluster ad.  This allows
			// us to easily remove attributes with the "-" command.
		InsertJobExpr( buffer, false );

		// free memory allocated by macro expansion module
		free( exValue );
	}	
}

void
SetGlobusParams()
{
	char *tmp;
	bool unified_syntax;
	MyString buffer;
	FILE* fp;

	if ( JobUniverse != CONDOR_UNIVERSE_GRID )
		return;

		// If we are dumping to a file we can't call
		// MySchedd->version(), because MySchedd is NULL. Instead we
		// assume we'd be talking to a Schedd just as current as we
		// are.
	if ( DumpClassAdToFile ) {
		unified_syntax = true;
	} else {
			// Does the schedd support the new unified syntax for grid universe
			// jobs (i.e. GridResource and GridJobId used for all types)?
		CondorVersionInfo vi( MySchedd->version() );
		unified_syntax = vi.built_since_version(6,7,11);
	}

	tmp = condor_param( GridResource, ATTR_GRID_RESOURCE );
	if ( tmp ) {
			// If we find grid_resource, then just toss it into the job ad

		if ( !unified_syntax ) {
				fprintf( stderr, "ERROR: Attribute %s cannot be used with "
						 "schedds older than 6.7.11\n", GridResource );
				DoCleanup( 0, 0, NULL );
				exit( 1 );
		}

			// TODO validate number of fields in grid_resource?

		buffer.sprintf( "%s = \"%s\"", ATTR_GRID_RESOURCE, tmp );
		InsertJobExpr( buffer );

		if ( strstr(tmp,"$$") ) {
				// We need to perform matchmaking on the job in order
				// to fill GridResource.
			buffer.sprintf("%s = FALSE", ATTR_JOB_MATCHED);
			InsertJobExpr (buffer);
			buffer.sprintf("%s = 0", ATTR_CURRENT_HOSTS);
			InsertJobExpr (buffer);
			buffer.sprintf("%s = 1", ATTR_MAX_HOSTS);
			InsertJobExpr (buffer);
		}

		free( tmp );

	} else if ( JobGridType ) {
			// If we don't find grid_resource but we know the grid-type,
			// then the user is using the old syntax (with grid_type).
			// Deal with all the attributes that go into GridResource.

		buffer.sprintf( "%s = \"%s\"", ATTR_JOB_GRID_TYPE, JobGridType );
		InsertJobExpr( buffer, false );

		if ( stricmp (JobGridType, "gt2") == MATCH ||
			 stricmp (JobGridType, "gt4") == MATCH ||
			 stricmp (JobGridType, "gt5") == MATCH ) {

			char * jobmanager_type;
			jobmanager_type = condor_param ( GlobusJobmanagerType );
			if (jobmanager_type) {
				if (stricmp (JobGridType, "gt4") != MATCH ) {
					fprintf(stderr, "\nWARNING: Param %s is not supported for grid types other than gt4\n", GlobusJobmanagerType );
				}
				if ( !unified_syntax ) {
					buffer.sprintf( "%s = \"%s\"", ATTR_GLOBUS_JOBMANAGER_TYPE,
							 jobmanager_type );
					InsertJobExpr (buffer, false );
				}
			} else if (stricmp (JobGridType, "gt4") == MATCH ) {
				jobmanager_type = strdup ("Fork");
			}

			char *globushost;
			globushost = condor_param( GlobusScheduler, "globus_scheduler" );
			if( !globushost ) {
					// this is stupid, the "GlobusScheduler" global
					// variable doesn't follow our usual conventions, so
					// its value is "globusscheduler". *sigh* so, our
					// first condor_param() uses the "old-style" format as
					// the alternate.  if we don't have a value, we want
					// to try again with the actual job classad value:
				globushost = condor_param( ATTR_GLOBUS_RESOURCE, NULL );
				if( ! globushost ) { 
					fprintf( stderr, "Globus/gt4 universe jobs require a "
							 "\"GlobusScheduler\" parameter\n" );
					DoCleanup( 0, 0, NULL );
					exit( 1 );
				}
			}

			if ( unified_syntax ) {
					// GT4 jobs need the extra jobmanager_type field.
				buffer.sprintf( "%s = \"%s %s%s%s\"", ATTR_GRID_RESOURCE,
						 JobGridType, globushost,
						 stricmp( JobGridType, "gt4" ) == MATCH ? " " : "",
						 stricmp(JobGridType, "gt4") == MATCH ?
						 jobmanager_type : "" );
				InsertJobExpr( buffer );
			} else {
				buffer.sprintf( "%s = \"%s\"", ATTR_GLOBUS_RESOURCE, globushost );
				InsertJobExpr (buffer);
			}

			if ( strstr(globushost,"$$") ) {
					// We need to perform matchmaking on the job in order
					// to find the GlobusScheduler.
				buffer.sprintf("%s = FALSE", ATTR_JOB_MATCHED);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 0", ATTR_CURRENT_HOSTS);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 1", ATTR_MAX_HOSTS);
				InsertJobExpr (buffer);
			}

			free( globushost );
			if ( jobmanager_type ) {
				free( jobmanager_type );
			}
		}

		if ( stricmp ( JobGridType, "condor" ) == MATCH ) {

			char *remote_schedd;
			char *remote_pool;

			if ( !(remote_schedd = condor_param( RemoteSchedd,
												 ATTR_REMOTE_SCHEDD ) ) ) {
				fprintf(stderr, "\nERROR: Condor grid jobs require a \"%s\" "
						"parameter\n", RemoteSchedd );
				DoCleanup( 0, 0, NULL );
				exit( 1 );
			}

			remote_pool = condor_param( RemotePool, ATTR_REMOTE_POOL );
			if ( remote_pool == NULL ) {
				DCCollector collector;
				remote_pool = collector.name();
				if ( remote_pool ) {
					remote_pool = strdup( remote_pool );
				}
			}
			if ( remote_pool == NULL && unified_syntax ) {

				fprintf(stderr, "\nERROR: Condor grid jobs require a \"%s\" "
						"parameter\n", RemotePool );
				DoCleanup( 0, 0, NULL );
				exit( 1 );
			}

			if ( unified_syntax ) {
				buffer.sprintf( "%s = \"condor %s %s\"", ATTR_GRID_RESOURCE,
						 remote_schedd, remote_pool );
				InsertJobExpr( buffer );
			} else {
				buffer.sprintf( "%s = \"%s\"", ATTR_REMOTE_SCHEDD,
						 remote_schedd );
				InsertJobExpr (buffer);

				if ( remote_pool ) {
					buffer.sprintf( "%s = \"%s\"", ATTR_REMOTE_POOL,
							 remote_pool );
					InsertJobExpr ( buffer );
				}
			}

			if ( strstr(remote_schedd,"$$") ) {

				// We need to perform matchmaking on the job in order to find
				// the RemoteSchedd.
				buffer.sprintf("%s = FALSE", ATTR_JOB_MATCHED);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 0", ATTR_CURRENT_HOSTS);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 1", ATTR_MAX_HOSTS);
				InsertJobExpr (buffer);
			}

			free( remote_schedd );
			free( remote_pool );
		}

		if ( stricmp (JobGridType, "nordugrid") == MATCH ) {

			char *host;
			host = condor_param( "nordugrid_resource", "nordugridresource" );
			if( !host ) {
				fprintf( stderr, "Nordugrid jobs require a "
						 "\"nordugrid_resource\" parameter\n" );
				DoCleanup( 0, 0, NULL );
				exit( 1 );
			}

				// nordugrid is only supported in versions that use the
				// unified grid resource syntax
			buffer.sprintf( "%s = \"nordugrid %s\"", ATTR_GRID_RESOURCE, 
					 host );
			InsertJobExpr( buffer );

			if ( strstr(host,"$$") ) {
					// We need to perform matchmaking on the job in order
					// to find the nordugrid_resource.
				buffer.sprintf("%s = FALSE", ATTR_JOB_MATCHED);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 0", ATTR_CURRENT_HOSTS);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 1", ATTR_MAX_HOSTS);
				InsertJobExpr (buffer);
			}

			free( host );
		}

		if ( stricmp ( JobGridType, "unicore" ) == MATCH ) {

			char *u_site = NULL;
			char *v_site = NULL;

			if ( !(u_site = condor_param( UnicoreUSite, "UnicoreUSite" )) ) {
				fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
						"parameter\n", UnicoreUSite );
				DoCleanup( 0, 0, NULL );
				exit( 1 );
			}

			if ( !(v_site = condor_param( UnicoreVSite, "UnicoreVSite" )) ) {
				fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
						"parameter\n", UnicoreVSite );
				DoCleanup( 0, 0, NULL );
				exit( 1 );
			}

			if ( unified_syntax ) {
				buffer.sprintf( "%s = \"unicore %s %s\"", ATTR_GRID_RESOURCE,
						 u_site, v_site );
				InsertJobExpr( buffer );
			} else {
				buffer.sprintf( "%s = \"%s\"", "UnicoreUSite", u_site );
				InsertJobExpr (buffer);

				buffer.sprintf( "%s = \"%s\"", "UnicoreVSite", v_site );
				InsertJobExpr ( buffer );
			}

			if ( strstr(u_site,"$$") || strstr(v_site,"$$") ) {

				// We need to perform matchmaking on the job in order to find
				// the remote resource.
				buffer.sprintf("%s = FALSE", ATTR_JOB_MATCHED);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 0", ATTR_CURRENT_HOSTS);
				InsertJobExpr (buffer);
				buffer.sprintf("%s = 1", ATTR_MAX_HOSTS);
				InsertJobExpr (buffer);
			}

			free( u_site );
			free( v_site );

		}

	} else {
			// TODO Make this allowable, triggering matchmaking for
			//   GridResource
		fprintf(stderr, "\nERROR: No resource identifier was found.\n" );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( !unified_syntax && JobGridType &&
		 ( stricmp (JobGridType, "gt2") == MATCH ||
		   stricmp (JobGridType, "gt4") == MATCH ||
		   stricmp (JobGridType, "gt5") == MATCH ||
		   stricmp (JobGridType, "nordugrid") == MATCH ) ) {

		buffer.sprintf( "%s = \"%s\"", ATTR_GLOBUS_CONTACT_STRING,
				 NULL_JOB_CONTACT );
		InsertJobExpr (buffer);
	}

	if ( JobGridType == NULL ||
		 stricmp (JobGridType, "gt2") == MATCH ||
		 stricmp (JobGridType, "gt4") == MATCH ||
		 stricmp (JobGridType, "gt5") == MATCH ||
		 stricmp (JobGridType, "nordugrid") == MATCH ) {

		if( (tmp = condor_param(GlobusResubmit,ATTR_GLOBUS_RESUBMIT_CHECK)) ) {
			buffer.sprintf( "%s = %s", ATTR_GLOBUS_RESUBMIT_CHECK, tmp );
			free(tmp);
			InsertJobExpr (buffer, false );
		} else {
			buffer.sprintf( "%s = FALSE", ATTR_GLOBUS_RESUBMIT_CHECK);
			InsertJobExpr (buffer, false );
		}
	}

	if ( (tmp = condor_param(GridShell, ATTR_USE_GRID_SHELL)) ) {

		if( tmp[0] == 't' || tmp[0] == 'T' ) {
			buffer.sprintf( "%s = TRUE", ATTR_USE_GRID_SHELL );
			InsertJobExpr( buffer );
		}
		free(tmp);
	}

	if ( JobGridType == NULL ||
		 stricmp (JobGridType, "gt2") == MATCH ||
		 stricmp (JobGridType, "gt5") == MATCH ||
		 stricmp (JobGridType, "gt4") == MATCH ) {

		buffer.sprintf( "%s = %d", ATTR_GLOBUS_STATUS,
				 GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED );
		InsertJobExpr (buffer);

		buffer.sprintf( "%s = 0", ATTR_NUM_GLOBUS_SUBMITS );
		InsertJobExpr (buffer, false );
	}

	buffer.sprintf( "%s = False", ATTR_WANT_CLAIMING );
	InsertJobExpr(buffer);

	if( (tmp = condor_param(GlobusRematch,ATTR_REMATCH_CHECK)) ) {
		buffer.sprintf( "%s = %s", ATTR_REMATCH_CHECK, tmp );
		free(tmp);
		InsertJobExpr (buffer, false );
	}

	if( (tmp = condor_param(GlobusRSL, ATTR_GLOBUS_RSL)) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_GLOBUS_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if( (tmp = condor_param(GlobusXML, ATTR_GLOBUS_XML)) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_GLOBUS_XML, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if( (tmp = condor_param(NordugridRSL, ATTR_NORDUGRID_RSL)) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_NORDUGRID_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ( (tmp = condor_param( KeystoreFile, ATTR_KEYSTORE_FILE )) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_KEYSTORE_FILE, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( JobGridType && stricmp( JobGridType, "unicore" ) == 0 ) {
		fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
				"parameter\n", KeystoreFile );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( (tmp = condor_param( KeystoreAlias, ATTR_KEYSTORE_ALIAS )) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_KEYSTORE_ALIAS, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( JobGridType && stricmp( JobGridType, "unicore" ) == 0 ) {
		fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
				"parameter\n", KeystoreAlias );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	if ( (tmp = condor_param( KeystorePassphraseFile,
							  ATTR_KEYSTORE_PASSPHRASE_FILE )) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_KEYSTORE_PASSPHRASE_FILE, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( JobGridType && stricmp( JobGridType, "unicore" ) == 0 ) {
		fprintf(stderr, "\nERROR: Unicore grid jobs require a \"%s\" "
				"parameter\n", KeystorePassphraseFile );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}
	
	//
	// Amazon grid-type submit attributes
	//
	if ( (tmp = condor_param( AmazonPublicKey, ATTR_AMAZON_PUBLIC_KEY )) ) {
		// check public key file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open public key file %s (%s)\n", 
							 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);
		}
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_PUBLIC_KEY, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && stricmp( JobGridType, "amazon" ) == 0 ) {
		fprintf(stderr, "\nERROR: Amazon jobs require a \"%s\" parameter\n", AmazonPublicKey );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}
	
	if ( (tmp = condor_param( AmazonPrivateKey, ATTR_AMAZON_PRIVATE_KEY )) ) {
		// check private key file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open private key file %s (%s)\n", 
							 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);
		}
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_PRIVATE_KEY, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && stricmp( JobGridType, "amazon" ) == 0 ) {
		fprintf(stderr, "\nERROR: Amazon jobs require a \"%s\" parameter\n", AmazonPrivateKey );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}
	
	// AmazonKeyPairFile is not a necessary parameter
	if( (tmp = condor_param( AmazonKeyPairFile, ATTR_AMAZON_KEY_PAIR_FILE )) ) {
		// for the relative path, the keypair output file will be written to the IWD
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_KEY_PAIR_FILE, full_path(tmp) );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
	
	// AmazonGroupName is not a necessary parameter
	if( (tmp = condor_param( AmazonSecurityGroups, ATTR_AMAZON_SECURITY_GROUPS )) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_SECURITY_GROUPS, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
	
	if ( (tmp = condor_param( AmazonAmiID, ATTR_AMAZON_AMI_ID )) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_AMI_ID, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( JobGridType && stricmp( JobGridType, "amazon" ) == 0 ) {
		fprintf(stderr, "\nERROR: Amazon jobs require a \"%s\" parameter\n", AmazonAmiID );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}
	
	// AmazonInstanceType is not a necessary parameter
	if( (tmp = condor_param( AmazonInstanceType, ATTR_AMAZON_INSTANCE_TYPE )) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_INSTANCE_TYPE, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
	
	// AmazonUserData and AmazonUserDataFile cannot exist in the same submit file
	bool has_userdata = false;
	bool has_userdatafile = false;
	
	// AmazonUserData is not a necessary parameter
	if( (tmp = condor_param( AmazonUserData, ATTR_AMAZON_USER_DATA )) ) {
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_USER_DATA, tmp);
		free( tmp );
		InsertJobExpr( buffer.Value() );
		has_userdata = true;
	}	

	// AmazonUserDataFile is not a necessary parameter
	if( (tmp = condor_param( AmazonUserDataFile, ATTR_AMAZON_USER_DATA_FILE )) ) {
		// check user data file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open user data file %s (%s)\n", 
								 full_path(tmp), strerror(errno));
				exit(1);
			}
			fclose(fp);
		}
		buffer.sprintf( "%s = \"%s\"", ATTR_AMAZON_USER_DATA_FILE, 
				full_path(tmp) );
		free( tmp );
		InsertJobExpr( buffer.Value() );
		has_userdatafile = true;
	}
	
	if (has_userdata && has_userdatafile) {
		// two attributes appear in the same submit file
		fprintf(stderr, "\nERROR: Parameters \"%s\" and \"%s\" exist in same Amazon job\n", 
						AmazonUserData, AmazonUserDataFile);
		DoCleanup( 0, 0, NULL );
		exit(1);
	}

	// CREAM clients support an alternate representation for resources:
	//   host.edu:8443/cream-batchname-queuename
	// Transform this representation into our regular form:
	//   host.edu:8443 batchname queuename
	if ( JobGridType != NULL && stricmp (JobGridType, "cream") == MATCH ) {
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
				resource.replaceString( "-", " ", pos );
				resource.replaceString( "/cream ", "/ce-cream/services/CREAM2 ", pos );

				buffer.sprintf( "%s = \"%s\"", ATTR_GRID_RESOURCE,
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

		// Find the X509 user proxy
		// First param for it in the submit file. If it's not there
		// and the job type requires an x509 proxy (globus, nordugrid),
		// then check the usual locations (as defined by GSI) and
		// bomb out if we can't find it.

	char *proxy_file = condor_param( X509UserProxy );

	if ( proxy_file == NULL && JobUniverse == CONDOR_UNIVERSE_GRID &&
		 JobGridType != NULL &&
		 (stricmp (JobGridType, "gt2") == MATCH ||
		  stricmp (JobGridType, "gt4") == MATCH ||
		  stricmp (JobGridType, "gt5") == MATCH ||
		  stricmp (JobGridType, "cream") == MATCH ||
		  stricmp (JobGridType, "nordugrid") == MATCH)) {

		proxy_file = get_x509_proxy_filename();
		if ( proxy_file == NULL ) {

			fprintf( stderr, "\nERROR: can't determine proxy filename\n" );
			fprintf( stderr, "x509 user proxy is required for gt2, gt4, nordugrid or cream jobs\n");
			exit (1);
		}
	}

	if (proxy_file != NULL) {
		if ( proxy_file[0] == '#' ) {
			buffer.sprintf( "%s=\"%s\"", ATTR_X509_USER_PROXY_SUBJECT, 
						   &proxy_file[1]);
			InsertJobExpr(buffer);	

//			(void) buffer.sprintf( "%s=\"%s\"", ATTR_X509_USER_PROXY, 
//						   proxy_file);
//			InsertJobExpr(buffer);	
			free( proxy_file );
		} else {
#ifndef WIN32
				// Versions of Condor prior to 6.7.3 set a different
				// value for X509UserProxySubject. Specifically, the
				// proxy's subject is used rather than the identity
				// (the subject this is a proxy for), and spaces are
				// converted to underscores. The elimination of spaces
				// is important, as the proxy subject gets passed on
				// the command line to the gridmanager in these older
				// versions and daemon core's CreateProcess() can't
				// handle spaces in command line arguments. So if
				// we're talking to an older schedd, use the old format.
			CondorVersionInfo *vi = NULL;
			if ( !DumpClassAdToFile ) {
				vi = new CondorVersionInfo( MySchedd->version() );
			}

			if ( check_x509_proxy(proxy_file) != 0 ) {
				fprintf( stderr, "\nERROR: %s\n", x509_error_string() );
				if ( vi ) delete vi;
				exit( 1 );
			}

			/* Insert the proxy subject name into the ad */
			char *proxy_subject;
			if ( vi && !vi->built_since_version(6,7,3) ) {
				// subject name with no VOMS attrs
				proxy_subject = x509_proxy_subject_name(proxy_file, 0);
			} else {
				// identity with VOMS attrs
				proxy_subject = x509_proxy_identity_name(proxy_file, 1);
			}
			if ( !proxy_subject ) {
				fprintf( stderr, "\nERROR: %s\n", x509_error_string() );
				if ( vi ) delete vi;
				exit( 1 );
			}
			/* Dreadful hack: replace all the spaces in the cert subject
			* with underscores.... why?  because we need to pass this
			* as a command line argument to the gridmanager, and until
			* daemoncore handles command-line args w/ an argv array, spaces
			* will cause trouble.  
			*/
			if ( vi && !vi->built_since_version(6,7,3) ) {
				char *space_tmp;
				do {
					if ( (space_tmp = strchr(proxy_subject,' ')) ) {
						*space_tmp = '_';
					}
				} while (space_tmp);
			}
			if ( vi ) delete vi;

			(void) buffer.sprintf( "%s=\"%s\"", ATTR_X509_USER_PROXY_SUBJECT, 
						   proxy_subject);
			InsertJobExpr(buffer);	
			free( proxy_subject );
#endif

			(void) buffer.sprintf( "%s=\"%s\"", ATTR_X509_USER_PROXY, 
						   full_path(proxy_file));
			InsertJobExpr(buffer);	
			free( proxy_file );
		}
	}


	//ckireyev: MyProxy-related crap
	if ((tmp = condor_param (ATTR_MYPROXY_HOST_NAME))) {
		buffer.sprintf(  "%s = \"%s\"", ATTR_MYPROXY_HOST_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_SERVER_DN))) {
		buffer.sprintf(  "%s = \"%s\"", ATTR_MYPROXY_SERVER_DN, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_PASSWORD))) {
		if (myproxy_password == NULL) {
			myproxy_password = tmp;
		}
	}

	if ((tmp = condor_param (ATTR_MYPROXY_CRED_NAME))) {
		buffer.sprintf(  "%s = \"%s\"", ATTR_MYPROXY_CRED_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if (myproxy_password) {
		buffer.sprintf(  "%s = %s", ATTR_MYPROXY_PASSWORD, myproxy_password);
		InsertJobExpr (buffer);
	}

	if ((tmp = condor_param (ATTR_MYPROXY_REFRESH_THRESHOLD))) {
		buffer.sprintf(  "%s = %s", ATTR_MYPROXY_REFRESH_THRESHOLD, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_NEW_PROXY_LIFETIME))) { 
		buffer.sprintf(  "%s = %s", ATTR_MYPROXY_NEW_PROXY_LIFETIME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	// END MyProxy-related crap
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
	MyString buffer;

	sig_name = findKillSigName( KillSig, ATTR_KILL_SIG );
	if( ! sig_name ) {
		switch(JobUniverse) {
		case CONDOR_UNIVERSE_STANDARD:
			sig_name = strdup( "SIGTSTP" );
			break;
		default:
			sig_name = strdup( "SIGTERM" );
			break;
		}
	}
	buffer.sprintf( "%s=\"%s\"", ATTR_KILL_SIG, sig_name );
	InsertJobExpr( buffer );
	free( sig_name );

	sig_name = findKillSigName( RmKillSig, ATTR_REMOVE_KILL_SIG );
	if( sig_name ) {
		buffer.sprintf( "%s=\"%s\"", ATTR_REMOVE_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
		sig_name = NULL;
	}

	sig_name = findKillSigName( HoldKillSig, ATTR_HOLD_KILL_SIG );
	if( sig_name ) {
		buffer.sprintf( "%s=\"%s\"", ATTR_HOLD_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
		sig_name = NULL;
	}
}
#endif  // of ifndef WIN32


int
read_condor_file( FILE *fp )
{
	char	*name, *value;
	char	*ptr;
	int		force = 0, queue_modifier = 0;

	char* justSeenQueue = NULL;

	JobIwd = "";

	LineNo = 0;
	ExtraLineNo = 0;

	extraLines.Rewind();

	for(;;) {
		force = 0;

		// check if we've just seen a "queue" command and need to
		// parse any extra lines passed in via -a first
		if( justSeenQueue ) {
			if( extraLines.Next( name ) ) {
				name = strdup( name );
				ExtraLineNo++;
			}
			else {
				// there are no more -a lines to parse, so rewind
				// extraLines in case we encounter another queue
				// command later, and restore the "queue" line itself
				// (stashed in justSeenQueue) so we can now parse it
				extraLines.Rewind();
				ExtraLineNo = 0;
				name = justSeenQueue;
			}
		}
		else {
			name = getline( fp );
			LineNo++;
		}
		if( name == NULL ) {
			break;
		}

			/* Skip over comments */
		if( *name == '#' || blankline(name) ) {
			continue;
		}
		/* check if the user wants to force a parameter into/outof the job ad */
		if (*name == '+') {
			force = 1;
			name++;
		} 
		else if (*name == '-') {
			name++;
			strlwr( name );
			forcedAttributes.remove( AttrKey( name ) );
			job->Delete( name );
			continue;
		}

		if( strincmp(name, "queue", strlen("queue")) == 0 ) {
			// if this is the first time we've seen this "queue"
			// command, then set justSeenQueue to TRUE and go back to
			// the top of the loop to process extraLines before
			// proceeding; if justSeenQueue is already TRUE, however,
			// then we've just finished processing extraLines, and
			// we're ready to go ahead parsing the "queue" command
			// itself
			if( !justSeenQueue ) {
				justSeenQueue = name;
				continue;
			}
			else {
				justSeenQueue = NULL;
				// we don't have to worry about freeing justSeenQueue
				// since the string is still pointed to by name and
				// will be freed below like any other line...
			}
			name = expand_macro( name, ProcVars, PROCVARSIZE );
			if( name == NULL ) {
				(void)fclose( fp );
				fprintf( stderr, "\nERROR: Failed to expand macros "
						 "in: %s\n", name );
				return( -1 );
			}
			int rval = sscanf(name+strlen("queue"), "%d", &queue_modifier); 
			if( rval == EOF || rval == 0 ) {
				queue_modifier = 1;
			}
			queue(queue_modifier);
			continue;
		}	

#define isop(c)		((c) == '=')
		
		/* Separate out the parameter name */
/*
		for( ptr=name ; *ptr && !isspace(*ptr) && !isop(*ptr); ptr++ );
*/

		ptr = name;
		while( *ptr ) {
			if( isspace(*ptr) || isop(*ptr) ) {
				break;
			} else {
				ptr++;
			}
		}

		if( !*ptr ) {
			(void)fclose( fp );
			return( -1 );
		}

		if( isop(*ptr) ) {
			*ptr++ = '\0';
		} else {
			*ptr++ = '\0';
			while( *ptr && !isop(*ptr) ) {
				ptr++;
			}

			if( !*ptr ) {
				(void)fclose( fp );
				return( -1 );
			}
			ptr += 1;

		}

			/* Skip to next non-space character */
		while( *ptr && isspace(*ptr) ) {
			ptr++;
		}

		value = ptr;

		if (name != NULL) {

			/* Expand references to other parameters */
			name = expand_macro( name, ProcVars, PROCVARSIZE );
			if( name == NULL ) {
				(void)fclose( fp );
				fprintf( stderr, "\nERROR: Failed to expand macros in: %s\n",
						 name );
				return( -1 );
			}
		}

		/* if the user wanted to force the parameter into the classad, do it 
		   We want to remove it first, so we get the new value if it's
		   already in there. -Derek Wright 7/13/98 */
		if (force == 1) {
			forcedAttributes.remove( AttrKey( name ) );
			forcedAttributes.insert( AttrKey( name ), MyString( value ) );
		} 

		strlwr( name );

		if( strcmp(name, Executable) == 0 ) {
			NewExecutable = true;
		}
			// Also, see if we're hitting "cmd", instead (we've
			// already lower-cased the name we're looking at)
		if( strcmp(name, "cmd") == 0 ) {
			NewExecutable = true;
		}

		/* Put the value in the Configuration Table, but only if it
		 *  wasn't forced into the ad with a '+'
		 */
		if ( force == 0 ) {
			insert( name, value, ProcVars, PROCVARSIZE );
		}

		free( name );
	}

	(void)fclose( fp );
	return 0;
}

char *
condor_param( const char* name)
{
	return condor_param(name, NULL);
}

char *
condor_param( const char* name, const char* alt_name )
{
	bool used_alt = false;
	char *pval = lookup_macro( name, ProcVars, PROCVARSIZE );

	static StringList* submit_exprs = NULL;
	static bool submit_exprs_initialized = false;
	if( ! submit_exprs_initialized ) {
		char* tmp = param( "SUBMIT_EXPRS" );
		if( tmp ) {
			submit_exprs = new StringList( tmp );
			free( tmp ); 
		}
		submit_exprs_initialized = true;
	}

	if( ! pval && alt_name ) {
		pval = lookup_macro( alt_name, ProcVars, PROCVARSIZE );
		used_alt = true;
	}

	if( ! pval ) {
			// if the value isn't in the submit file, check in the
			// submit_exprs list and use that as a default.  
		if( submit_exprs ) {
			if( submit_exprs->contains_anycase(name) ) {
				return( param(name) );
			}
			if( alt_name && submit_exprs->contains_anycase(alt_name) ) {
				return( param(alt_name) );
			}
		}			
		return( NULL );
	}

	pval = expand_macro( pval, ProcVars, PROCVARSIZE );

	if( pval == NULL ) {
		fprintf( stderr, "\nERROR: Failed to expand macros in: %s\n",
				 used_alt ? alt_name : name );
		exit(1);
	}

	return( pval );
}


void
set_condor_param( const char *name, const char *value )
{
	char *tval = strdup( value );

	insert( name, tval, ProcVars, PROCVARSIZE );
	free(tval);
}

void
set_condor_param_used( const char *name ) 
{
	set_macro_used(name, 1, ProcVars, PROCVARSIZE);
}

int
strcmpnull(const char *str1, const char *str2)
{
	if( str1 && str2 ) {
		return strcmp(str1, str2);
	}
	return (str1 || str2);
}

void
connect_to_the_schedd()
{
	if ( ActiveQueueConnection == TRUE ) {
		// we are already connected; do not connect again
		return;
	}

	setupAuthentication();

	CondorError errstack;
	if( ConnectQ(MySchedd->addr(), 0 /* default */, false /* default */, &errstack) == 0 ) {
		if( ScheddName ) {
			fprintf( stderr, 
					"\nERROR: Failed to connect to queue manager %s\n%s\n",
					 ScheddName, errstack.getFullText(true) );
		} else {
			fprintf( stderr, 
				"\nERROR: Failed to connect to local queue manager\n%s\n",
				errstack.getFullText(true) );
		}
		exit(1);
	}

	ActiveQueueConnection = TRUE;
}

void
queue(int num)
{
	char tmp[ BUFSIZ ];
	char const *logfile;
	int		rval;

	// First, connect to the schedd if we have not already done so

	if (!DumpClassAdToFile && ActiveQueueConnection != TRUE) 
	{
		connect_to_the_schedd();
	}

	/* queue num jobs */
	for (int i=0; i < num; i++) {

	
		if (NewExecutable) {
 			if ( !DumpClassAdToFile ) {
				if ((ClusterId = NewCluster()) < 0) {
					fprintf(stderr, "\nERROR: Failed to create cluster\n");
					if ( ClusterId == -2 ) {
						fprintf(stderr,
						"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
					}
					exit(1);
				}
			}
				// We only need to call init_job_ad the second time we see
				// a new Executable, because we call init_job_ad() in main()
			if ( !IsFirstExecutable ) {
				init_job_ad();
			}
			IsFirstExecutable = false;
			if ( !IsFirstExecutable && DumpClassAdToFile ) {
					ProcId = 1;
			} else {
				ProcId = -1;
			}
			ClusterAdAttrs.clear();
		}

		if ( !DumpClassAdToFile ) {
			if ( ClusterId == -1 ) {
				fprintf( stderr, "\nERROR: Used queue command without "
						 "specifying an executable\n" );
				exit(1);
			}
		
			ProcId = NewProc (ClusterId);

			if ( ProcId < 0 ) {
				fprintf(stderr, "\nERROR: Failed to create proc\n");
				if ( ProcId == -2 ) {
					fprintf(stderr,
					"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
				}
				DoCleanup(0,0,NULL);
				exit(1);
			}

		}
		
		if (MaxProcsPerCluster && ProcId >= MaxProcsPerCluster) {
			fprintf(stderr, "\nERROR: number of procs exceeds SUBMIT_MAX_PROCS_IN_CLUSTER\n");
			DoCleanup(0,0,NULL);
			exit(-1);
		} 

		if ( !DumpClassAdToFile ) {
			/*
			**	Insert the current idea of the cluster and
			**	process number into the hash table.
			*/	
			// GotQueueCommand = 1; // (see bellow)
			(void)sprintf(tmp, "%d", ClusterId);
			set_condor_param(Cluster, tmp);
			set_condor_param_used(Cluster); // we don't show system macros as "unused"
			(void)sprintf(tmp, "%d", ProcId);
			set_condor_param(Process, tmp);
			set_condor_param_used(Process);

		}

		// we move this outside the above, otherwise it appears that we have 
		// received no queue command (or several, if there were multiple ones)
		GotQueueCommand = 1;

#if !defined(WIN32)
		SetRootDir();	// must be called very early
#endif
		SetIWD();		// must be called very early

		if (NewExecutable) {
			NewExecutable = false;
			SetUniverse();
			SetExecutable();
		}
		SetMachineCount();

		/* For MPI only... we have to define $(NODE) to some string
		here so that we don't break the param parser.  In the
		MPI shadow, we'll convert the string into an integer
		corresponding to the mpi node's number. */
		if ( JobUniverse == CONDOR_UNIVERSE_MPI ) {
			set_condor_param ( "NODE", "#MpInOdE#" );
			set_condor_param_used ( "NODE" );
		} else if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL ) {
			set_condor_param ( "NODE", "#pArAlLeLnOdE#" );
			set_condor_param_used ( "NODE" );
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
		SetTDP();			// before SetTransferFile() and SetRequirements()
		SetTransferFiles();	 // must be called _before_ SetImageSize() 
		SetRunAsOwner();
#if defined(WIN32)
        SetLoadProfile();
#endif
		SetPerFileEncryption();  // must be called _before_ SetRequirements()
		SetImageSize();		// must be called _after_ SetTransferFiles()

		SetSimpleJobExprs();

			//
			// SetCronTab() must be called before SetJobDeferral()
			// Both of these need to be called before SetRequirements()
			//
		SetCronTab();
		SetJobDeferral();
		
			//
			// Must be called _after_ SetTransferFiles(), SetJobDeferral(),
			// SetCronTab(), and SetPerFileEncryption()
			//
		SetRequirements();	
		
		SetJobLease();		// must be called _after_ SetStdFile(0,1,2)

		SetRemoteAttrs();

		SetPeriodicHoldCheck();
		SetPeriodicRemoveCheck();
		SetExitHoldCheck();
		SetExitRemoveCheck();
		SetNoopJob();
		SetNoopJobExitSignal();
		SetNoopJobExitCode();
		SetLeaveInQueue();
		SetArguments();
		SetGlobusParams();
		SetGSICredentials();
		SetMatchListLen();
		SetDAGNodeName();
		SetDAGManJobId();
		SetJarFiles();
		SetJavaVMArgs();
		SetParallelStartupScripts(); //JDB
		SetConcurrencyLimits();
		SetVMParams();

			// SetForcedAttributes should be last so that it trumps values
			// set by normal submit attributes
		SetForcedAttributes();
		rval = 0; // assume success
		if ( !DumpClassAdToFile ) {
			rval = SaveClassAd();
		}
		SetLogNotes();
		SetUserNotes();

		switch( rval ) {
		case 0:			/* Success */
		case 1:
			break;
		default:		/* Failed for some other reason... */
			fprintf( stderr, "\nERROR: Failed to queue job.\n" );
			exit(1);
		}

		ClusterCreated = TRUE;
	
		if( !Quiet ) 
			{
				fprintf(stdout, "\n** Proc %d.%d:\n", ClusterId, ProcId);
				job->fPrint (stdout);
			}

		logfile = condor_param( UserLogFile, ATTR_ULOG_FILE );
		// Convert to a pathname using IWD if needed
		if ( logfile ) {
			logfile = full_path(logfile);
		}

		if (CurrentSubmitInfo == -1 ||
			SubmitInfo[CurrentSubmitInfo].cluster != ClusterId ||
			strcmpnull( SubmitInfo[CurrentSubmitInfo].logfile,
						logfile ) != 0 ||
			strcmpnull( SubmitInfo[CurrentSubmitInfo].lognotes,
						LogNotesVal ) != 0 ||
			strcmpnull( SubmitInfo[CurrentSubmitInfo].usernotes,
						UserNotesVal ) != 0 ) {
			CurrentSubmitInfo++;
			SubmitInfo[CurrentSubmitInfo].cluster = ClusterId;
			SubmitInfo[CurrentSubmitInfo].firstjob = ProcId;
			SubmitInfo[CurrentSubmitInfo].lognotes = NULL;
			SubmitInfo[CurrentSubmitInfo].usernotes = NULL;

			if (logfile) {
				// Store the full pathname to the log file
				SubmitInfo[CurrentSubmitInfo].logfile = strdup(logfile);
			} else {
				SubmitInfo[CurrentSubmitInfo].logfile = NULL;
			}
			if( LogNotesVal ) {
				SubmitInfo[CurrentSubmitInfo].lognotes = strdup( LogNotesVal );
			}
			if( UserNotesVal ) {
				SubmitInfo[CurrentSubmitInfo].usernotes = strdup( UserNotesVal );
			}
		}
		SubmitInfo[CurrentSubmitInfo].lastjob = ProcId;

		// SubmitInfo[x].lastjob controls how many submit events we
		// see in the user log.  For parallel jobs, we only want
		// one "job" submit event per cluster, no matter how many 
		// Procs are in that it.  Setting lastjob to zero makes this so.

		if (JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
				SubmitInfo[CurrentSubmitInfo].lastjob = 0;
		}

		// now that we've loaded all the information into the job object
		// we can just dump it to a file.  (In my opinion this is needlessly 
		// ugly, here we should be using a derivation of Stream called File 
		// which we would in turn serialize the job object to...)
		if ( DumpClassAdToFile ) {
			job->fPrint ( DumpFile );
			fprintf ( DumpFile, "\n" );
		}

		if ( job_ad_saved == false ) {
			delete job;
		}
		job = new ClassAd();
		job_ad_saved = false;

		if (Quiet) {
			fprintf(stdout, ".");
		}


	}
}

bool
findClause( const char* buffer, const char* attr_name )
{
	const char* ptr;
	int len = strlen( attr_name );
	for( ptr = buffer; *ptr; ptr++ ) {
		if( strincmp(attr_name,ptr,len) == MATCH ) {
			return true;
		}
	}
	return false;
}

bool
findClause( MyString const &buffer, char const *attr_name )
{
	return findClause( buffer.Value(), attr_name );
}


bool
check_requirements( char const *orig, MyString &answer )
{
	bool	checks_opsys = false;
	bool	checks_arch = false;
	bool	checks_disk = false;
	bool	checks_mem = false;
	bool	checks_fsdomain = false;
	bool	checks_ckpt_arch = false;
	bool	checks_file_transfer = false;
	bool	checks_per_file_encryption = false;
	bool	checks_pvm = false;
	bool	checks_mpi = false;
	bool	checks_tdp = false;
#if defined(WIN32)
	bool	checks_credd = false;
#endif
	char	*ptr;
	MyString ft_clause;

	if( strlen(orig) ) {
		answer.sprintf( "(%s)", orig );
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

	checks_arch = findClause( answer, ATTR_ARCH );
	checks_opsys = findClause( answer, ATTR_OPSYS );
	checks_disk =  findClause( answer, ATTR_DISK );
	checks_tdp =  findClause( answer, ATTR_HAS_TDP );
#if defined(WIN32)
	checks_credd = findClause( answer, ATTR_LOCAL_CREDD );
#endif

	if( JobUniverse == CONDOR_UNIVERSE_STANDARD ) {
		checks_ckpt_arch = findClause( answer, ATTR_CKPT_ARCH );
	}
	if( JobUniverse == CONDOR_UNIVERSE_PVM ) {
		checks_pvm = findClause( answer, ATTR_HAS_PVM );
	}
	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		checks_mpi = findClause( answer, ATTR_HAS_MPI );
	}
	if( mightTransfer(JobUniverse) ) { 
		switch( should_transfer ) {
		case STF_IF_NEEDED:
		case STF_NO:
			checks_fsdomain = findClause( answer,
										  ATTR_FILE_SYSTEM_DOMAIN ); 
			break;
		case STF_YES:
			checks_file_transfer = findClause( answer,
											   ATTR_HAS_FILE_TRANSFER );
			checks_per_file_encryption = findClause( answer,
										   ATTR_HAS_PER_FILE_ENCRYPTION );
			break;
		}
	}

		// because of the special-case nature of "Memory" and
		// "VirtualMemory", we have to do this one manually...
	char const *aptr;
	for( aptr = answer.Value(); *aptr; aptr++ ) {
		if( strincmp(ATTR_MEMORY,aptr,5) == MATCH ) {
				// We found "Memory", but we need to make sure that's
				// not part of "VirtualMemory"...
			if( aptr == answer.Value() ) {
					// We're at the beginning, must be Memory, since
					// there's nothing before it.
				checks_mem = true;
				break;
			}
				// Otherwise, it's safe to go back one position:
			if( *(aptr-1) == 'l' || *(aptr-1) == 'L' ) {
					// Must be VirtualMemory, keep searching...
				continue;
			}
				// If it wasn't an 'l', we must have found it...
			checks_mem = true;
			break;
		}
	}
 
	if( JobUniverse == CONDOR_UNIVERSE_JAVA ) {
		if( answer[0] ) {
			answer += " && ";
		}
		answer += "(";
		answer += ATTR_HAS_JAVA;
		answer += ")";
	} else if ( JobUniverse == CONDOR_UNIVERSE_VM ) {
		// For vm universe, we require the same archicture.
		if( !checks_arch ) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "(Arch == \"";
			answer += Architecture;
			answer += "\")";
		}
		// add HasVM to requirements
		bool checks_vm = false;
		checks_vm = findClause( answer, ATTR_HAS_VM );
		if( !checks_vm ) {
			answer += "&& (";
			answer += ATTR_HAS_VM;
			answer += ")";
		}
		// add vm_type to requirements
		bool checks_vmtype = false;
		checks_vmtype = findClause( answer, ATTR_VM_TYPE);
		if( !checks_vmtype ) {
			answer += " && (";
			answer += ATTR_VM_TYPE;
			answer += " == \"";
			answer += VMType.Value();
			answer += "\")";
		}
		// check if the number of executable VM is more than 0
		bool checks_avail = false;
		checks_avail = findClause(answer, ATTR_VM_AVAIL_NUM);
		if( !checks_avail ) {
			answer += " && (";
			answer += ATTR_VM_AVAIL_NUM;
			answer += " > 0)";
		}
	} else {
		if( !checks_arch ) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "(Arch == \"";
			answer += Architecture;
			answer += "\")";
		}

		if( !checks_opsys ) {
			answer += " && (OpSys == \"";
			answer += OperatingSystem;
			answer += "\")";
		}
	}

	if ( JobUniverse == CONDOR_UNIVERSE_STANDARD && !checks_ckpt_arch ) {
		answer += " && ((CkptArch == Arch) ||";
		answer += " (CkptArch =?= UNDEFINED))";
		answer += " && ((CkptOpSys == OpSys) ||";
		answer += "(CkptOpSys =?= UNDEFINED))";
	}

	if( !checks_disk ) {
		if ( JobUniverse == CONDOR_UNIVERSE_VM ) {
			// VM universe uses Total Disk 
			// instead of Disk for Condor slot
			answer += " && (TotalDisk >= DiskUsage)";
		}else {
			answer += " && (Disk >= DiskUsage)";
		}
	}

	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		// The memory requirement for VM universe will be 
		// added in SetVMRequirements 
		if ( !checks_mem ) {
			answer += " && ( (Memory * 1024) >= ImageSize )";
		}
	}

	if( HasTDP && !checks_tdp ) {
		answer += "&& (";
		answer += ATTR_HAS_TDP;
		answer += ")";
	}

	if ( JobUniverse == CONDOR_UNIVERSE_PVM ) {
		if( ! checks_pvm ) {
			answer += "&& (";
			answer += ATTR_HAS_PVM;
			answer += ")";
		}
	} 

	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		if( ! checks_mpi ) {
			answer += "&& (";
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
				answer += "&& (TARGET.";
				answer += ATTR_FILE_SYSTEM_DOMAIN;
				answer += " == MY.";
				answer += ATTR_FILE_SYSTEM_DOMAIN;
				answer += ")";
			} 
			break;
			
		case STF_YES:
				// we're definitely going to use file transfer.  
			if( ! checks_file_transfer ) {
				answer += "&& (";
				answer += ATTR_HAS_FILE_TRANSFER;
				if (!checks_per_file_encryption && NeedsPerFileEncryption) {
					answer += " && ";
					answer += ATTR_HAS_PER_FILE_ENCRYPTION;
				}
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
				ft_clause = "&& ((";
				ft_clause += ATTR_HAS_FILE_TRANSFER;
				if (NeedsPerFileEncryption) {
					ft_clause += " && ";
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
			answer += " && (";
			answer += ATTR_HAS_JOB_DEFERRAL;
			answer += ")";
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
			//	( ( ATTR_CURRENT_TIME + ATTR_SCHEDD_INTERVAL ) >= 
			//	  ( ATTR_DEFERRAL_TIME - ATTR_DEFERRAL_PREP_TIME ) )
			//  &&
			//    ( ATTR_CURRENT_TIME < 
			//    ( ATTR_DEFFERAL_TIME + ATTR_DEFERRAL_WINDOW ) )
			//  )
			//
		MyString attrib;
		attrib.sprintf( "( ( %s + %s ) >= ( %s - %s ) ) && "\
						"( %s < ( %s + %s ) )",
						ATTR_CURRENT_TIME,
						ATTR_SCHEDD_INTERVAL,
						ATTR_DEFERRAL_TIME,
						ATTR_DEFERRAL_PREP_TIME,
						ATTR_CURRENT_TIME,
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
		MyString tmp_rao = " && (";
		tmp_rao += ATTR_HAS_WIN_RUN_AS_OWNER;
		if (!checks_credd) {
			tmp_rao += " && (";
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
		pathname.sprintf( "%s\\%s", p_iwd, name );
	}
#else

	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		pathname.sprintf( "%s%s", JobRootdir.Value(), name );
	} else {	/* relative to iwd which is relative to the root */
		pathname.sprintf( "%s/%s/%s", JobRootdir.Value(), p_iwd, name );
	}
#endif

	compress( pathname );

	return pathname.Value();
}

void
check_open( const char *name, int flags )
{
	int		fd;
	MyString strPathname;
	char *temp;
	StringList *list;

	/* No need to check for existence of the Null file. */
	if( strcmp(name, NULL_FILE) == MATCH ) {
		return;
	}

	if ( IsUrl( name ) ) {
		return;
	}

	strPathname = full_path(name);

		/* This is only for MPI.  We test for our string that
		   we replaced "$(NODE)" with, and replace it with "0".  Thus, 
		   we will really only try and access the 0th file only */
	if ( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		strPathname.replaceString("#MpInOdE#", "0");
	} else if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL ) {
		strPathname.replaceString("#pArAlLeLnOdE#", "0");
	}


	/* If this file as marked as append-only, do not truncate it here */

	temp = condor_param( AppendFiles, ATTR_APPEND_FILES );
	if(temp) {
		list = new StringList(temp);
		if(list->contains_withwildcard(name)) {
			flags = flags & ~O_TRUNC;
		}
		delete list;
	}

	if ( !DisableFileChecks ) {
			if( (fd=safe_open_wrapper(strPathname.Value(),flags | O_LARGEFILE,0664)) < 0 ) {
			fprintf( stderr, "\nERROR: Can't open \"%s\"  with flags 0%o (%s)\n",
					 strPathname.Value(), flags, strerror( errno ) );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		(void)close( fd );
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


void
usage()
{
	fprintf( stderr, "Usage: %s [options] [cmdfile]\n", MyName );
	fprintf( stderr, "	Valid options:\n" );
	fprintf( stderr, "	-verbose\t\tverbose output\n" );
	fprintf( stderr, 
			 "	-unused\t\t\ttoggles unused or unexpanded macro warnings\n"
			 "         \t\t\t(overrides config file; multiple -u flags ok)\n" );
	fprintf( stderr, "	-name <name>\t\tsubmit to the specified schedd\n" );
	fprintf( stderr,
			 "	-remote <name>\t\tsubmit to the specified remote schedd\n"
			 "                \t\t(implies -spool)\n" );
	fprintf( stderr,
			 "	-append <line>\t\tadd line to submit file before processing\n"
			 "                \t\t(overrides submit file; multiple -a lines ok)\n" );
	fprintf( stderr, "	-disable\t\tdisable file permission checks\n" );
	fprintf( stderr, "	-spool\t\t\tspool all files to the schedd\n" );
	fprintf( stderr, "	-password <password>\tspecify password to MyProxy server\n" );
	fprintf( stderr, "	-pool <host>\t\tUse host as the central manager to query\n" );
	fprintf( stderr, "	-dump <filename>\t\tWrite schedd bound ClassAds to this\n"
					 "                  \t\tfile rather than to the schedd itself\n" );
	fprintf( stderr, "	-stm <method>\t\thow to move a sandbox into Condor\n" );
	fprintf( stderr, "               \t\tAvailable methods:\n\n" );
	fprintf( stderr, "               \t\t\tstm_use_schedd_only\n" );
	fprintf( stderr, "               \t\t\tstm_use_transferd\n\n" );
	fprintf( stderr, "	If [cmdfile] is omitted, input is read from stdin\n" );
}


extern "C" {
int
DoCleanup(int,int,char*)
{
	if( ClusterCreated ) 
	{
		// TerminateCluster() may call EXCEPT() which in turn calls 
		// DoCleanup().  This lead to infinite recursion which is bad.
		ClusterCreated = 0;
		if (!ActiveQueueConnection) {
			ActiveQueueConnection = (ConnectQ(MySchedd->addr()) != 0);
		}
		if (ActiveQueueConnection) {
			// Call DestroyCluster() now in an attempt to get the schedd
			// to unlink the initial checkpoint file (i.e. remove the 
			// executable we sent earlier from the SPOOL directory).
			DestroyCluster( ClusterId );

			// We purposefully do _not_ call DisconnectQ() here, since 
			// we do not want the current transaction to be committed.
		}
	}

	if (owner) {
		free(owner);
	}
	if (ntdomain) {
		free(ntdomain);
	}
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

	Architecture = param( "ARCH" );

	if( Architecture == NULL ) {
		fprintf(stderr, "ARCH not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	OperatingSystem = param( "OPSYS" );
	if( OperatingSystem == NULL ) {
		fprintf(stderr,"OPSYS not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		fprintf(stderr,"SPOOL not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

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

	tmp = param( "WARN_ON_UNUSED_SUBMIT_FILE_MACROS" );
	if ( NULL != tmp ) {
		if( (*tmp == 'f' || *tmp == 'F') ) {
			WarnOnUnusedMacros = 0;
		}
		free( tmp );
	}

}

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
	if (str) {
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


void
delete_commas( char *ptr )
{
	for( ; *ptr; ptr++ ) {
		if( *ptr == ',' ) {
			*ptr = ' ';
		}
	}
}


extern "C" {
int SetSyscalls( int foo ) { return foo; }
}

void
log_submit()
{
	 char	 *simple_name;

		// don't write to the EVENT_LOG in condor_submit; that is done by 
		// the condor_schedd (since submit likely does not have permission).
	 WriteUserLog usr_log(true);
	 SubmitEvent jobSubmit;

	 usr_log.setUseXML(UseXMLInLog);

	if( Quiet ) {
		fprintf(stdout, "Logging submit event(s)");
	}

	if ( DumpClassAdToFile ) {
		// we just put some arbitrary string here: it doesn't actually mean 
		// anything since we will never communicate the resulting ad to 
		// to anyone (we make the name obviously unresolvable so we know
		// this was a generated file).
		strcpy (jobSubmit.submitHost, "localhost-used-to-dump");
	} else {
		strcpy (jobSubmit.submitHost, MySchedd->addr());
	}

	if( LogNotesVal ) {
		jobSubmit.submitEventLogNotes = LogNotesVal;
		LogNotesVal = NULL;
	}

	if( UserNotesVal ) {
		jobSubmit.submitEventUserNotes = UserNotesVal;
		UserNotesVal = NULL;
	}

	for (int i=0; i <= CurrentSubmitInfo; i++) {

		if ((simple_name = SubmitInfo[i].logfile) != NULL) {
			if( jobSubmit.submitEventLogNotes ) {
				delete[] jobSubmit.submitEventLogNotes;
			}
			jobSubmit.submitEventLogNotes = strnewp( SubmitInfo[i].lognotes );

			if( jobSubmit.submitEventUserNotes ) {
				delete[] jobSubmit.submitEventUserNotes;
			}
			jobSubmit.submitEventUserNotes = strnewp( SubmitInfo[i].usernotes );
			
			// we don't know the gjid here, so pass in NULL as the last 
			// parameter - epaulson 2/09/2007
			if ( ! usr_log.initialize(owner, ntdomain, simple_name,
									  0, 0, 0, NULL) ) {
				fprintf(stderr, "\nERROR: Failed to log submit event.\n");
			} else {
				// Output the information
				for (int j=SubmitInfo[i].firstjob; j<=SubmitInfo[i].lastjob;
							j++) {
					if ( ! usr_log.initialize(SubmitInfo[i].cluster,
								j, 0, NULL) ) {
						fprintf(stderr, "\nERROR: Failed to log submit event.\n");
					} else {
						if( ! usr_log.writeEvent(&jobSubmit,job) ) {
							fprintf(stderr, "\nERROR: Failed to log submit event.\n");
						}
						if( Quiet ) {
							fprintf(stdout, ".");
						}
					}
				}
			}
		}
	}
	if( Quiet ) {
		fprintf( stdout, "\n" );
	}
}


int
SaveClassAd ()
{
	ExprTree *tree = NULL, *lhs = NULL, *rhs = NULL;
	char *lhstr, *rhstr;
	int  retval = 0;
	int myprocid = ProcId;
	static ClassAd* current_cluster_ad = NULL;

	if ( ProcId > 0 ) {
		SetAttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId);
	} else {
		myprocid = -1;		// means this is a cluster ad
		if( SetAttributeInt (ClusterId, myprocid, ATTR_CLUSTER_ID, ClusterId) == -1 ) {
			fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n", 
					ATTR_CLUSTER_ID, ClusterId, ClusterId, ProcId, errno);
			return -1;
		}
	}

	

	job->ResetExpr();
	while( (tree = job->NextExpr()) ) {
		if( tree->invisible ) {
			continue;
		}
		lhstr = NULL;
		rhstr = NULL;
		if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
		if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
		if( !lhs || !rhs || !lhstr || !rhstr) { 
			fprintf( stderr, "\nERROR: Null attribute name or value for job %d.%d\n",
					 ClusterId, ProcId );
			retval = -1;
		} else {
			// To facilitate processing of job status from the
			// job_queue.log, the ATTR_JOB_STATUS attribute should not
			// be stored within the cluster ad. Instead, it should be
			// directly part of each job ad. This change represents an
			// increase in size for the job_queue.log initially, but
			// the ATTR_JOB_STATUS is guaranteed to be specialized for
			// each job so the two approaches converge. Further
			// optimization should focus on sending only the
			// attributes required for the job to run. -matt 1 June 09
			// Mostly the same rational for ATTR_JOB_SUBMISSION.
			// -matt // 24 June 09
			int tmpProcId = myprocid;
			if( strcasecmp(lhstr, ATTR_JOB_STATUS) == 0 ||
				strcasecmp(lhstr, ATTR_JOB_SUBMISSION) == 0 ) myprocid = ProcId;
			if( SetAttribute(ClusterId, myprocid, lhstr, rhstr) == -1 ) {
				fprintf( stderr, "\nERROR: Failed to set %s=%s for job %d.%d (%d)\n", 
						 lhstr, rhstr, ClusterId, ProcId, errno );
				retval = -1;
			}
			myprocid = tmpProcId;
		}
		free(lhstr);
		free(rhstr);
		if(retval == -1) {
			return -1;
		}
	}

	if ( ProcId == 0 ) {
		if( SetAttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId) == -1 ) {
			fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n", 
					 ATTR_PROC_ID, ProcId, ClusterId, ProcId, errno );
			return -1;
		}
	}

	// If spooling entire job "sandbox" to the schedd, then we need to keep
	// the classads in an array to later feed into the filetransfer object.
	if ( Remote ) {
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
InsertJobExpr (MyString const &expr, bool clustercheck)
{
	InsertJobExpr(expr.Value(),clustercheck);
}

void 
InsertJobExpr (const char *expr, bool clustercheck)
{
	ExprTree *tree = NULL, *lhs = NULL;
	int unused = 0;

	MyString hashkey(expr);

	if ( clustercheck && ProcId > 0 ) {
		// We are inserting proc 1 or above.  So before we actually stick
		// this into the job ad, make certain we did not already place it
		// into the cluster ad.  We do this via a hashtable lookup.

		if ( ClusterAdAttrs.lookup(hashkey,unused) == 0 ) {
			// found it.  so it is already in the cluster ad; we're done.
			return;
		}
	}

	int pos = 0;
	int retval = Parse (expr, tree, &pos);

	if (retval)
	{
		fprintf (stderr, "\nERROR: Parse error in expression: \n\t%s\n\t", expr);
		while (pos--) {
			fputc( ' ', stderr );
		}
		fprintf (stderr, "^^^\n");
		fprintf(stderr,"Error in submit file\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if( !(lhs = tree->LArg()) ) {
		fprintf (stderr, "\nERROR: Expression not assignment: %s\n", expr);
		fprintf(stderr,"Error in submit file\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	
	if (!job->InsertOrUpdate (expr))
	{	
		fprintf(stderr,"\nERROR: Unable to insert expression: %s\n", expr);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if ( clustercheck && ProcId < 1 ) {
		// We are working on building the ad which will serve as our
		// cluster ad.  Thus insert this expr into our hashtable.
		if ( ClusterAdAttrs.insert(hashkey,unused) < 0 ) {
			fprintf( stderr,"\nERROR: Unable to insert expression into "
					 "hashtable: %s\n", expr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
	}

	delete tree;
}

void 
InsertJobExprInt(const char * name, int val, bool clustercheck /*= true*/)
{
	ASSERT(name);
	MyString buf;
	buf.sprintf("%s = %d", name, val);
	InsertJobExpr(buf.Value(), clustercheck);
}

void 
InsertJobExprString(const char * name, const char * val, bool clustercheck /*= true*/)
{
	ASSERT(name);
	ASSERT(val);
	MyString buf;
	buf.sprintf("%s = \"%s\"", name, val);
	InsertJobExpr(buf.Value(), clustercheck);
}

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
	Rendezvous = condor_param( RendezvousDir, "rendezvous_dir" );
	if( ! Rendezvous ) {
			// If those didn't work, try a few other variations, just
			// to be safe:
		Rendezvous = condor_param( "rendezvousdirectory",
								   "rendezvous_directory" );
	}
	if( Rendezvous ) {
		dprintf( D_FULLDEBUG,"setting RENDEZVOUS_DIRECTORY=%s\n", Rendezvous );
			//SetEnv because Authentication::authenticate() expects them there.
		SetEnv( "RENDEZVOUS_DIRECTORY", Rendezvous );
		free( Rendezvous );
	}
}


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
	TransferInputSize += calc_image_size(fixedname.Value());

	transfer_file_list.append(fixedname.Value());
	tmp_ptr = transfer_file_list.print_to_string();

	buffer.sprintf( "%s = \"%s\"", ATTR_TRANSFER_INPUT_FILES, tmp_ptr);
	InsertJobExpr(buffer, false);
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

// If a file is in transfer_input_files, the file will have just basename.
// Otherwise, it will have full path. To get full path, iwd is used.
bool 
make_vm_file_path(const char *filename, MyString& fixedname)
{
	if( filename == NULL ) {
		return false;
	}

	fixedname = delete_quotation_marks(filename);

	MyString transfer_input_files;
	// check whether the file will be transferred
	if( job->LookupString(ATTR_TRANSFER_INPUT_FILES,transfer_input_files) 
				== 1 ) {
		StringList transfer_file_list(NULL, ",");
		transfer_file_list.initializeFromString(transfer_input_files.Value() );

		if( filelist_contains_file(fixedname.Value(), 
					&transfer_file_list, true) ) {
			// this file is already in transfer_input_files
			// filename should have only basename
			fixedname = condor_basename(fixedname.Value());
			return true;
		}
	}

	// This file will not be transferred
	// filename should have absolute path
	fixedname = full_path(fixedname.Value());
	check_and_universalize_path(fixedname);
	//check_open(fixedname.Value(), O_RDONLY);
	
	// we need the same file system for this file
	vm_need_fsdomain = true;
	return true;
}

// this function parses and checks xen disk parameters
bool
validate_xen_disk_parm(const char *xen_disk, MyString &fixed_disk)
{
	if( !xen_disk ) {
		return false;
	}

	const char *ptr = xen_disk;
	// skip leading white spaces
	while( *ptr && ( *ptr == ' ' )) {
		ptr++;
	}

	// parse each disk
	// e.g.) xen_disk = filename1:hda1:w, filename2:hda2:w
	StringList disk_files(ptr, ",");
	if( disk_files.isEmpty() ) {
		return false;
	}

	fixed_disk = "";

	disk_files.rewind();
	const char *one_disk = NULL;
	while( (one_disk = disk_files.next() ) != NULL ) {

		// found disk file
		StringList single_disk_file(one_disk, ":");
		if( single_disk_file.number() != 3 ) {
			return false;
		}

		// check filename
		single_disk_file.rewind();
		MyString filename = single_disk_file.next();
		filename.trim();

		MyString fixedname;
		if( make_vm_file_path(filename.Value(), fixedname) 
				== false ) {
			return false;
		}else {
			if( fullpath(fixedname.Value()) == false ) {
				// This file will be transferred
				xen_has_file_to_be_transferred = true;
			}

			if( fixed_disk.Length() > 0 ) {
				fixed_disk += ",";
			}
			// file name
			fixed_disk += fixedname;
			fixed_disk += ":";
			// device
			fixed_disk += single_disk_file.next();
			fixed_disk += ":";
			// permission
			fixed_disk += single_disk_file.next();
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

	// check OS
	/*	
	if( (stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH ) || 
			vm_need_fsdomain ) {
		bool checks_opsys = false;
		checks_opsys = findClause( vmanswer, ATTR_OPSYS );

		if( !checks_opsys ) {
			vmanswer += " && (OpSys == \"";
			vmanswer += OperatingSystem;
			vmanswer += "\")";
		}
	}
	*/

	// check file system domain
	if( vm_need_fsdomain ) {
		// some files don't use file transfer.
		// so we need the same file system domain
		bool checks_fsdomain = false;
		checks_fsdomain = findClause( vmanswer, ATTR_FILE_SYSTEM_DOMAIN ); 

		if( !checks_fsdomain ) {
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_FILE_SYSTEM_DOMAIN;
			vmanswer += " == MY.";
			vmanswer += ATTR_FILE_SYSTEM_DOMAIN;
			vmanswer += ")";
		}
		MyString my_fsdomain;
		if(job->LookupString(ATTR_FILE_SYSTEM_DOMAIN, my_fsdomain) != 1) {
			buffer.sprintf( "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, 
					My_fs_domain ); 
			InsertJobExpr( buffer );
		}
	}

	if( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) != MATCH ) {
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
		vmanswer += " && (";
		vmanswer += ATTR_TOTAL_MEMORY;
		vmanswer += " >= ";

		MyString mem_tmp;
		mem_tmp.sprintf("%d", VMMemory);
		vmanswer += mem_tmp.Value();
		vmanswer += ")";
	}

	// add vm_memory to requirements
	bool checks_vmmemory = false;
	checks_vmmemory = findClause(vmanswer, ATTR_VM_MEMORY);
	if( !checks_vmmemory ) {
		vmanswer += " && (";
		vmanswer += ATTR_VM_MEMORY;
		vmanswer += " >= ";

		MyString mem_tmp;
		mem_tmp.sprintf("%d", VMMemory);
		vmanswer += mem_tmp.Value();
		vmanswer += ")";
	}

	if( VMHardwareVT ) {
		bool checks_hardware_vt = false;
		checks_hardware_vt = findClause( vmanswer, ATTR_VM_HARDWARE_VT);

		if( !checks_hardware_vt ) {
			// add hardware vt to requirements
			vmanswer += " && (";
			vmanswer += ATTR_VM_HARDWARE_VT;
			vmanswer += ")";
		}
	}

	if( VMNetworking ) {
		bool checks_vmnetworking = false;
		checks_vmnetworking = findClause( vmanswer, ATTR_VM_NETWORKING);

		if( !checks_vmnetworking ) {
			// add vm_networking to requirements
			vmanswer += " && (";
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
		checks_ckpt_arch = findClause( vmanswer, ATTR_CKPT_ARCH );
		checks_vm_ckpt_mac = findClause( vmanswer, ATTR_VM_CKPT_MAC );

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

	buffer.sprintf( "%s = %s", ATTR_REQUIREMENTS, vmanswer.Value());
	JobRequirements = vmanswer;
	InsertJobExpr (buffer);
}

void
SetConcurrencyLimits()
{
	MyString tmp = condor_param_mystring(ConcurrencyLimits, NULL);

	if (!tmp.IsEmpty()) {
		char *str;

		tmp.lower_case();

		StringList list(tmp.Value());

		list.qsort();

		str = list.print_to_string();
		tmp.sprintf("%s = \"%s\"", ATTR_CONCURRENCY_LIMITS, str);
		InsertJobExpr(tmp.Value());

		free(str);
	}
}

// this function must be called after SetUniverse
void
SetVMParams()
{
	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		return;
	}

	char* tmp_ptr = NULL;
	bool has_vm_cdrom_files = false;
	bool has_vm_iso_file = false;
	bool vm_should_transfer_cdrom_files = false;
	MyString final_cdrom_files;
	MyString buffer;

	// VM type is already set in SetUniverse
	buffer.sprintf( "%s = \"%s\"", ATTR_JOB_VM_TYPE, VMType.Value());
	InsertJobExpr(buffer);

	// VM checkpoint is already set in SetUniverse
	buffer.sprintf( "%s = %s", ATTR_JOB_VM_CHECKPOINT, 
			VMCheckpoint? "TRUE":"FALSE");
	InsertJobExpr(buffer);

	// VM networking is already set in SetUniverse
	buffer.sprintf( "%s = %s", ATTR_JOB_VM_NETWORKING, 
			VMNetworking? "TRUE":"FALSE");
	InsertJobExpr(buffer);

	// Here we need to set networking type
	if( VMNetworking ) {
		tmp_ptr = condor_param(VM_Networking_Type, ATTR_JOB_VM_NETWORKING_TYPE);
		if( tmp_ptr ) {
			VMNetworkType = tmp_ptr;
			free(tmp_ptr);

			buffer.sprintf( "%s = \"%s\"", ATTR_JOB_VM_NETWORKING_TYPE, 
					VMNetworkType.Value());
			InsertJobExpr(buffer, false );
		}else {
			VMNetworkType = "";
		}
	}

	// Set memory for virtual machine
	tmp_ptr = condor_param(VM_Memory, ATTR_JOB_VM_MEMORY);
	if( !tmp_ptr ) {
		fprintf( stderr, "\nERROR: '%s' cannot be found.\n"
				"Please specify '%s' for vm universe "
				"in your submit description file.\n", VM_Memory, VM_Memory);
		DoCleanup(0,0,NULL);
		exit(1);
	}else {
		VMMemory = (int)strtol(tmp_ptr, (char **)NULL, 10);
		free(tmp_ptr);
	}
	if( VMMemory <= 0 ) {
		fprintf( stderr, "\nERROR: '%s' is incorrectly specified\n"
				"For example, for vm memroy of 128 Megabytes,\n"
				"you need to use 128 in your submit description file.\n", 
				VM_Memory);
		DoCleanup(0,0,NULL);
		exit(1);
	}
	buffer.sprintf( "%s = %d", ATTR_JOB_VM_MEMORY, VMMemory);
	InsertJobExpr( buffer, false );

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
	buffer.sprintf("%s = %d", ATTR_JOB_VM_VCPUS, VMVCPUS);
	InsertJobExpr(buffer, false);

	/*
	 * Set the MAC address for this VM.
	 */
	tmp_ptr = condor_param(VM_MACAddr, ATTR_JOB_VM_MACADDR);
	if(tmp_ptr)
	  {
	    buffer.sprintf("%s = \"%s\"", ATTR_JOB_VM_MACADDR, tmp_ptr);
	    InsertJobExpr(buffer, false);
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
			buffer.sprintf( "%s = TRUE", VMPARAM_NO_OUTPUT_VM);
			InsertJobExpr( buffer, false );
		}
	}

	// 'vm_cdrom_files' defines files which will be shown in CDROM inside a VM.
	// That is, vm-gahp on the execute machine will create a ISO file with 
	// the defined files. The ISO file will be viewed as a CDROM in a VM.
	char *vm_cdrom_files = NULL;
	vm_cdrom_files = condor_param("vm_cdrom_files");
	if( vm_cdrom_files ) {
		vm_should_transfer_cdrom_files = false;
		tmp_ptr = condor_param("vm_should_transfer_cdrom_files");
		if( parse_vm_option(tmp_ptr, vm_should_transfer_cdrom_files) 
				== false ) {
			MyString err_msg;
			err_msg = "\nERROR: You must explicitly specify "
				"\"vm_should_transfer_cdrom_files\" "
				"in your submit description file. "
				"You need to define either "
				"\"vm_should_transfer_cdrom_files = YES\" or "
				" \"vm_should_transfer_cdrom_files = NO\". "
				"If you define \"vm_should_transfer_cdrom_files = YES\", " 
				"all files in \"vm_cdrom_files\" will be "
				"transfered to an execute machine. "
				"If you define \"vm_should_transfer_cdrom_files = NO\", "
				"all files in \"vm_cdrom_files\" should be "
				"accessible with a shared file system\n";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit(1);
		}
		free(tmp_ptr);

		buffer.sprintf( "%s = %s", VMPARAM_TRANSFER_CDROM_FILES,
				vm_should_transfer_cdrom_files ? "TRUE" : "FALSE");
		InsertJobExpr( buffer, false);

		if( vm_should_transfer_cdrom_files == false ) {
			vm_need_fsdomain = true;
		}

		StringList cdrom_file_list(NULL, ",");
		cdrom_file_list.initializeFromString(vm_cdrom_files);
		final_cdrom_files = "";

		cdrom_file_list.rewind();

		has_vm_iso_file = false;
		MyString cdrom_file;
		const char *tmp_file = NULL;
		while( (tmp_file = cdrom_file_list.next() ) != NULL ) {
			cdrom_file = delete_quotation_marks(tmp_file);
			if( cdrom_file.Length() == 0 ) {
				continue;
			}
			cdrom_file.trim();
			if( has_suffix(cdrom_file.Value(), ".iso") ) {
				has_vm_iso_file = true;
			}

			// convert file name to full path that uses iwd
			cdrom_file = full_path(cdrom_file.Value());
			check_and_universalize_path(cdrom_file);

			if( vm_should_transfer_cdrom_files ) {
				// add this cdrom file to transfer_input_files
				transfer_vm_file(cdrom_file.Value());
			}

			if( final_cdrom_files.Length() > 0 ) {
				final_cdrom_files += ",";
			}
			if( vm_should_transfer_cdrom_files ) {
				// A file will be transferred. 
				// So we use basename.
				final_cdrom_files += condor_basename(cdrom_file.Value());
			}else {
				final_cdrom_files += cdrom_file.Value();
			}
		}

		if( has_vm_iso_file && (cdrom_file_list.number() > 1)) {
			fprintf( stderr, "\nERROR: You cannot define an iso file "
					"with other files. You should define either "
					"only one iso file or multiple non-iso files in %s\n", 
					"vm_cdrom_files");
			DoCleanup(0,0,NULL);
			exit(1);
		}

		buffer.sprintf( "%s = \"%s\"", VMPARAM_CDROM_FILES, 
				final_cdrom_files.Value());
		InsertJobExpr( buffer, false );
		has_vm_cdrom_files = true;
		free(vm_cdrom_files);
	}

	if( (stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH) || (stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_KVM) == MATCH) ) {
		bool real_xen_kernel_file = false;
		bool need_xen_root_device = false;

		// Read the parameter of xen_transfer_files 
		char *transfer_files = NULL;
		char *transf_attr_name;
		if ( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			transfer_files = condor_param("xen_transfer_files");
			transf_attr_name = VMPARAM_XEN_TRANSFER_FILES;
		}
		else
		{
			transfer_files = condor_param("kvm_transfer_files");
			transf_attr_name = VMPARAM_KVM_TRANSFER_FILES;
		}

		if( transfer_files ) {
			MyString final_output;
			StringList xen_file_list(NULL, ",");
			xen_file_list.initializeFromString(transfer_files);

			xen_file_list.rewind();

			MyString one_file;
			const char *tmp_file = NULL;
			while( (tmp_file = xen_file_list.next() ) != NULL ) {
				one_file = delete_quotation_marks(tmp_file);
				if( one_file.Length() == 0 ) {
					continue;
				}
				one_file.trim();
				// convert file name to full path that uses iwd
				one_file = full_path(one_file.Value());
				check_and_universalize_path(one_file);

				// add this file to transfer_input_files
				transfer_vm_file(one_file.Value());

				if( final_output.Length() > 0 ) {
					final_output += ",";
				}
				// VMPARAM_XEN_TRANSFER_FILES will include 
				// basenames for files to be transferred.
				final_output += condor_basename(one_file.Value());
			}
			buffer.sprintf( "%s = \"%s\"", transf_attr_name, 
					final_output.Value());
			InsertJobExpr( buffer, false );
			free(transfer_files);
		}
		
		if ( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
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
				MyString fixedname = delete_quotation_marks(xen_kernel);

				if ( stricmp(fixedname.Value(), XEN_KERNEL_INCLUDED) == 0) {
					// kernel image is included in a disk image file
					// so we will use bootloader(pygrub etc.) defined 
					// in a vmgahp config file on an excute machine 
					real_xen_kernel_file = false;
					need_xen_root_device = false;
				}else if ( stricmp(fixedname.Value(), XEN_KERNEL_HW_VT) == 0) {
					// A job user want to use an unmodified OS in Xen.
					// so we require hardware virtualization.
					real_xen_kernel_file = false;
					need_xen_root_device = false;
					VMHardwareVT = true;
					buffer.sprintf( "%s = TRUE", ATTR_JOB_VM_HARDWARE_VT);
					InsertJobExpr( buffer, false );
				}else {
					// real kernel file
					if( make_vm_file_path(xen_kernel, fixedname) 
							== false ) {
						DoCleanup(0,0,NULL);
						exit(1);
					}
					real_xen_kernel_file = true;
					need_xen_root_device = true;
				}
				buffer.sprintf( "%s = \"%s\"", VMPARAM_XEN_KERNEL, 
						fixedname.Value());
				InsertJobExpr(buffer, false);
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
				MyString fixedname;
				if( make_vm_file_path(xen_initrd, fixedname) 
						== false ) {
					DoCleanup(0,0,NULL);
					exit(1);
				}
				buffer.sprintf( "%s = \"%s\"", VMPARAM_XEN_INITRD, 
						fixedname.Value());
				InsertJobExpr(buffer, false);
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
					MyString fixedvalue = delete_quotation_marks(xen_root);
					buffer.sprintf( "%s = \"%s\"", VMPARAM_XEN_ROOT, 
							fixedvalue.Value());
					InsertJobExpr(buffer, false);
					free(xen_root);
				}
			}
		}// xen only params

		// <x>_disk is a required parameter
		char *disk = NULL;
		char *disk_attr_name;
		if ( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			disk = condor_param("xen_disk");
			disk_attr_name = VMPARAM_XEN_DISK;
		}
		else
		{
			disk = condor_param("kvm_disk");
			disk_attr_name = VMPARAM_KVM_DISK;
		}

		if( !disk ) {
			fprintf( stderr, "\nERROR: '%s' cannot be found.\n"
					"Please specify '%s' for the virtual machine "
					"in your submit description file.\n", 
					"<vm>_disk", "<vm>_disk");
			DoCleanup(0,0,NULL);
			exit(1);
		}else {
			MyString fixedvalue = delete_quotation_marks(disk);
			if( validate_xen_disk_parm(fixedvalue.Value(), fixedvalue) 
					== false ) {
				fprintf(stderr, "\nERROR: '<vm>_disk' has incorrect format.\n"
						"The format shoud be like "
						"\"<filename>:<devicename>:<permission>\"\n"
						"e.g.> For single disk: <vm>_disk = filename1:hda1:w\n"
						"      For multiple disks: <vm>_disk = "
						"filename1:hda1:w,filename2:hda2:w\n");
				DoCleanup(0,0,NULL);
				exit(1);
			}

			buffer.sprintf( "%s = \"%s\"", disk_attr_name, fixedvalue.Value());
			InsertJobExpr( buffer, false);
			free(disk);
		}

		if ( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			// xen_kernel_params is a optional parameter
			char *xen_kernel_params = NULL;
			xen_kernel_params = condor_param("xen_kernel_params");
			if( xen_kernel_params ) {
				MyString fixedvalue = delete_quotation_marks(xen_kernel_params);
				buffer.sprintf( "%s = \"%s\"", VMPARAM_XEN_KERNEL_PARAMS, 
						fixedvalue.Value());
				InsertJobExpr( buffer, false);
				free(xen_kernel_params);
			}
		}

		if( has_vm_cdrom_files )
		{
			MyString xen_cdrom_string;
			char *cdrom_device = NULL;
			char *cdrom_attr_name;

			if ( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
			{
				cdrom_device = condor_param("xen_cdrom_device");
				cdrom_attr_name = VMPARAM_XEN_CDROM_DEVICE;
			}
			else
			{
				cdrom_device = condor_param("kvm_cdrom_device");
				cdrom_attr_name = VMPARAM_KVM_CDROM_DEVICE;
			}

			if( !cdrom_device ) {
				fprintf(stderr, "\nERROR: To use 'vm_cdrom_files', "
						"you must also define '<vm>_cdrom_device'.\n");
				DoCleanup(0,0,NULL);
				exit(1);
			}
			xen_cdrom_string = cdrom_device;
			free(cdrom_device);

			if( xen_cdrom_string.find(":", 0 ) >= 0 ) {
				fprintf(stderr, "\nERROR: '<vm>_cdrom_device' should include "
						"just device name.\n"
						"e.g.) 'xen_cdrom_device = hdc'\n");
				DoCleanup(0,0,NULL);
				exit(1);
			}

			buffer.sprintf( "%s = \"%s\"", cdrom_attr_name,
					xen_cdrom_string.Value());
			InsertJobExpr( buffer, false );
		}

	}else if( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_VMWARE) == MATCH ) {
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

		buffer.sprintf( "%s = %s", VMPARAM_VMWARE_TRANSFER, 
				vmware_should_transfer_files ? "TRUE" : "FALSE");
		InsertJobExpr( buffer, false);

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

		buffer.sprintf( "%s = %s", VMPARAM_VMWARE_SNAPSHOTDISK,
				vmware_snapshot_disk? "TRUE" : "FALSE");
		InsertJobExpr( buffer, false );

		// vmware_dir is a directory that includes vmx file and vmdk files.
		char *vmware_dir = NULL;
		vmware_dir = condor_param("vmware_dir");
		if( !vmware_dir ) {
			fprintf( stderr, "\nERROR: '%s' cannot be found.\n"
					"Please specify the directory including "
					"vmx and vmdk files for vmware virtual machine "
					"in your submit description file.\n",
					"vmware_dir");
			DoCleanup(0,0,NULL);
			exit(1);
		}

		MyString f_dirname = delete_quotation_marks(vmware_dir);
		free(vmware_dir);

		f_dirname = full_path(f_dirname.Value(), false);
		check_and_universalize_path(f_dirname);

		buffer.sprintf( "%s = \"%s\"", VMPARAM_VMWARE_DIR, f_dirname.Value());
		InsertJobExpr( buffer, false );

		// find vmx file in the given directory
		StringList vmxfiles;
		if( suffix_matched_files_in_dir(f_dirname.Value(), vmxfiles, 
					".vmx", true) == false ) {
			fprintf( stderr, "\nERROR: no vmx file for vmware can be found "
					"in %s.\n", f_dirname.Value());
			DoCleanup(0,0,NULL);
			exit(1);
		}else {
			if( vmxfiles.number() > 1 ) {
				fprintf( stderr, "\nERROR: multiple vmx files exist. "
						"Only one vmx file must be in %s.\n", 
						f_dirname.Value());
				DoCleanup(0,0,NULL);
				exit(1);
			}
			vmxfiles.rewind();
			MyString vmxfile = vmxfiles.next();

			// add vmx file to transfer_input_files
			// vmx file will be always transfered to an execute machine
			transfer_vm_file(vmxfile.Value());
			buffer.sprintf( "%s = \"%s\"", VMPARAM_VMWARE_VMX_FILE,
					condor_basename(vmxfile.Value()));
			InsertJobExpr( buffer, false );
		}

		// find vmdk files in the given directory
		StringList vmdk_files;
		if( suffix_matched_files_in_dir(f_dirname.Value(), vmdk_files, 
					".vmdk", true) == true ) {
			MyString vmdks;
			vmdk_files.rewind();
			const char *tmp_file = NULL;
			while( (tmp_file = vmdk_files.next() ) != NULL ) {
				if( vmware_should_transfer_files ) {
					// add vmdk files to transfer_input_files
					transfer_vm_file(tmp_file);
				}
				if( vmdks.Length() > 0 ) {
					vmdks += ",";
				}
				// VMPARAM_VMWARE_VMDK_FILES will include 
				// basenames for vmdks files
				vmdks += condor_basename(tmp_file);
			}
			buffer.sprintf( "%s = \"%s\"", VMPARAM_VMWARE_VMDK_FILES, 
					vmdks.Value());
			InsertJobExpr( buffer, false );
		}

		// look for an nvram file in given directory; if present,
		// transfer it
		Directory d(f_dirname.Value());
		if (d.Find_Named_Entry("nvram")) {
			transfer_vm_file(d.GetFullPath());
		}
	}

	// Check if a job user defines 'Argument'.
	// If 'Argument' parameter is defined, 
	// we will create a file called "condor.arg" on an execute machine 
	// and add the file to input CDROM.
	ArgList arglist;
	MyString arg_err_msg;
	MyString arg_str;
	if(!arglist.GetArgsStringV1or2Raw(job, &arg_str, &arg_err_msg)) {
		arg_str = "";
	}

	// Here we check if this job submit description file is 
	// correct for vm checkpoint
	if( VMCheckpoint ) {
		if( stricmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH ) {
			// For vm checkpoint in Xen
			// 1. all disk files should be in a shared file system
			// 2. If a job uses CDROM files, it should be 
			// 	  single ISO file and be in a shared file system
			if( xen_has_file_to_be_transferred || 
				(has_vm_cdrom_files && 
				 (!has_vm_iso_file || vm_should_transfer_cdrom_files)) ||
				!arg_str.IsEmpty() ) {
				MyString err_msg;
				err_msg = "\nERROR: To use checkpoint in Xen, "
					"You need to make sure the followings.\n"
					"1. All xen disk files should be in a shared file system\n"
					"2. If you use 'vm_cdrom_files', "
					"only single ISO file is allowed and the ISO file "
					"should also be in a shared file system\n"
					"3. You cannot use 'arguments' in a job description file\n\n";

				if( xen_has_file_to_be_transferred ) {
					err_msg += "ERROR: You requested to transfer at least one Xen "
						"disk file\n";
				}
				if( has_vm_cdrom_files ) {
					if( !has_vm_iso_file ) {
						err_msg += "ERROR: You defined non-iso CDROM files\n";
					}else if( vm_should_transfer_cdrom_files ) {
						err_msg += "ERROR: You requested to transfer a ISO file\n";
					}
				}
				if( !arg_str.IsEmpty() ) {
					err_msg += "ERROR: You defined 'arguments'.\n";
				}

				fprintf( stderr, "%s", err_msg.Value());
				DoCleanup(0,0,NULL);
				exit( 1 );
			}
		}
		// For vmware, there is no limitation.
	}

	if( !arg_str.IsEmpty() && has_vm_cdrom_files ) {
		if( has_vm_iso_file ) {
			// A job user cannot use a iso file for input CDROM and 
			// 'Argument' parameter together, 
			MyString err_msg;
			err_msg = "\nERROR: You cannot use single iso file for "
				"'vm_cdrom_files' and 'arguments' in a job description "
				"file together. To use 'arguments' you need to use "
				"non-iso files in 'vm_cdrom_files'\n";
			print_wrapped_text( err_msg.Value(), stderr );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}else {
			// Because a file called "condor.arg" will be created and 
			// be added to input CDROM on an execute machine,
			// 'vm_cdrom_files' should not contain "condor.arg".
			if( final_cdrom_files.find(VM_UNIV_ARGUMENT_FILE, 0) >= 0 ) {
				MyString err_msg;
				err_msg.sprintf("\nERROR: The file name '%s' is reserved for "
					"'arguments' in vm universe. String in 'arguments' will "
					"be saved into a file '%s'. And '%s' will be added to "
					"input CDROM. So 'vm_cdrom_files' should not "
					"contain '%s'\n", VM_UNIV_ARGUMENT_FILE,
					VM_UNIV_ARGUMENT_FILE, VM_UNIV_ARGUMENT_FILE,
					VM_UNIV_ARGUMENT_FILE);
				print_wrapped_text( err_msg.Value(), stderr );
				DoCleanup(0,0,NULL);
				exit( 1 );
			}
		}
	}
			
	// Now all VM parameters are set
	// So we need to add necessary VM attributes to Requirements
	SetVMRequirements();
}


#include "daemon_core_stubs.h"
