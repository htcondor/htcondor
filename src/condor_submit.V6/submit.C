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
 
#define MAXVARNAME 256  // formerly in condor_scanner.h

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_string.h"
#include "condor_ckpt_name.h"
#include "env.h"
#include "basename.h"
#include <time.h>
#include "user_log.c++.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
//#include "condor_parser.h"
//#include "condor_scanner.h"
#include "condor_distribution.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#else
// WINDOWS only
#include "store_cred.h"
#endif
#include "my_hostname.h"
#include "get_daemon_name.h"
#include "condor_qmgr.h"
#include "sig_install.h"
#include "access.h"
#include "daemon.h"

#include "extArray.h"
#include "HashTable.h"
#include "MyString.h"
#include "string_list.h"
#include "which.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
#include "dc_schedd.h"
#include "my_username.h"
#include "globus_utils.h"
#include "enum_utils.h"

#include "list.h"

static int hashFunction( const MyString&, int );
HashTable<MyString,MyString> forcedAttributes( 64, hashFunction ); 
HashTable<MyString,int> CheckFilesRead( 577, hashFunction ); 
HashTable<MyString,int> CheckFilesWrite( 577, hashFunction ); 
HashTable<MyString,int> ClusterAdAttrs( 31, hashFunction );

char* mySubSystem = "SUBMIT";	/* Used for SUBMIT_EXPRS */

ClassAd  *job = NULL;
char	 buffer[_POSIX_ARG_MAX + 64];

char	*OperatingSystem;
char	*Architecture;
// char	*Spool;
char	*Flavor;
char	*ScheddName = NULL;
DCSchedd* MySchedd = NULL;
char	*My_fs_domain;
char    JobRequirements[2048];
#if !defined(WIN32)
char    JobRootdir[_POSIX_PATH_MAX];
#endif
char    JobIwd[_POSIX_PATH_MAX];

int		LineNo;
int		ExtraLineNo;
int		GotQueueCommand;

char	IckptName[_POSIX_PATH_MAX];	/* Pathname of spooled initial ckpt file */

unsigned int TransferInputSize;	/* total size of files transfered to exec machine */
char	*MyName;
int		Quiet = 1;
int		DisableFileChecks = 0;
int	  ClusterId = -1;
int	  ProcId = -1;
int	  JobUniverse;
int		Remote=0;
int		ClusterCreated = FALSE;
int		ActiveQueueConnection = FALSE;
bool	NewExecutable = false;
bool	IsFirstExecutable;
bool	UserLogSpecified = false;
bool    UseXMLInLog = false;
ShouldTransferFiles_t should_transfer = STF_NO;
bool job_ad_saved = false;	// should we deallocate the job ad after storing it?

char* LogNotesVal = NULL;
char* UserNotesVal = NULL;

List<char> extraLines;  // lines passed in via -a argument

#define PROCVARSIZE	32
BUCKET *ProcVars[ PROCVARSIZE ];

#define MEG	(1<<20)

/*
**	Names of possible CONDOR variables.
*/
char	*Cluster 		= "cluster";
char	*Process			= "process";
char	*Hold			= "hold";
char	*Priority		= "priority";
char	*Notification	= "notification";
char	*Executable		= "executable";
char	*Arguments		= "arguments";
char	*GetEnv			= "getenv";
char	*AllowStartupScript = "allow_startup_script";
char	*Environment	= "environment";
char	*Input			= "input";
char	*Output			= "output";
char	*Error			= "error";
#if !defined(WIN32)
char	*RootDir		= "rootdir";
#endif
char	*InitialDir		= "initialdir";
char	*RemoteInitialDir		= "remote_initialdir";
char	*Requirements	= "requirements";
char	*Preferences	= "preferences";
char	*Rank				= "rank";
char	*ImageSize		= "image_size";
char	*Universe		= "universe";
char	*Grid_Type		= "grid_type";
char	*MachineCount	= "machine_count";
char	*NotifyUser		= "notify_user";
char	*ExitRequirements = "exit_requirements";
char	*UserLogFile	= "log";
char    *UseLogUseXML   = "log_xml";
char	*CoreSize		= "coresize";
char	*NiceUser		= "nice_user";

char	*X509UserProxy	= "x509userproxy";
char	*GlobusScheduler = "globusscheduler";
char    *GridShell = "gridshell";
char	*GlobusRSL = "globusrsl";
char	*RendezvousDir	= "rendezvousdir";

char	*FileRemaps = "file_remaps";
char	*BufferFiles = "buffer_files";
char	*BufferSize = "buffer_size";
char	*BufferBlockSize = "buffer_block_size";

char	*FetchFiles = "fetch_files";
char	*CompressFiles = "compress_files";
char	*AppendFiles = "append_files";
char	*LocalFiles = "local_files";

char	*TransferInputFiles = "transfer_input_files";
char	*TransferOutputFiles = "transfer_output_files";
char	*TransferFiles = "transfer_files";
char	*TransferExecutable = "transfer_executable";
char	*TransferInput = "transfer_input";
char	*TransferOutput = "transfer_output";
char	*TransferError = "transfer_error";

char	*StreamInput = "stream_input";
char	*StreamOutput = "stream_output";
char	*StreamError = "stream_error";

char	*CopyToSpool = "copy_to_spool";
char	*LeaveInQueue = "leave_in_queue";
char	*MirrorSchedd = "mirror_schedd";

char	*PeriodicHoldCheck = "periodic_hold";
char	*PeriodicReleaseCheck = "periodic_release";
char	*PeriodicRemoveCheck = "periodic_remove";
char	*OnExitHoldCheck = "on_exit_hold";
char	*OnExitRemoveCheck = "on_exit_remove";

char	*GlobusResubmit = "globus_resubmit";
char	*GlobusRematch = "globus_rematch";

char	*LastMatchListLength = "match_list_length";

char	*DAGNodeName = "dag_node_name";
char	*DAGManJobId = "dagman_job_id";
char	*LogNotesCommand = "submit_event_notes";
char	*UserNotesCommand = "submit_event_user_notes";
char	*JarFiles = "jar_files";

char    *ParallelScriptShadow  = "parallel_script_shadow";  
char    *ParallelScriptStarter = "parallel_script_starter"; 

#if !defined(WIN32)
char	*KillSig			= "kill_sig";
char	*RmKillSig			= "remove_kill_sig";
char	*HoldKillSig		= "hold_kill_sig";
#endif

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
void 	SetNotifyUser ();
void	SetRemoteInitialDir();
void	SetExitRequirements();
void 	SetArguments();
void 	SetEnvironment();
#if !defined(WIN32)
void 	ComputeRootDir();
void 	SetRootDir();
#endif
void 	SetRequirements();
void 	SetTransferFiles();
bool 	SetNewTransferFiles( void );
void 	SetOldTransferFiles( bool, bool );
void	InsertFileTransAttrs( FileTransferOutput_t when_output );
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
void 	check_iwd( char *iwd );
int	read_condor_file( FILE *fp );
char * 	condor_param( const char* name, const char* alt_name );
char * 	condor_param( const char* name ); // call param with NULL as the alt
void 	set_condor_param( const char* name, char* value );
void 	queue(int num);
char * 	check_requirements( char *orig );
void 	check_open( const char *name, int flags );
void 	usage();
void 	init_params();
int 	whitespace( const char *str);
void 	delete_commas( char *ptr );
void 	compress( char *str );
char	*full_path(const char *name, bool use_iwd=true);
void 	log_submit();
void 	get_time_conv( int &hours, int &minutes );
int	  SaveClassAd ();
void	InsertJobExpr (const char *expr, bool clustercheck = true);
void	check_umask();
void setupAuthentication();
void	SetPeriodicHoldCheck(void);
void	SetPeriodicRemoveCheck(void);
void	SetExitHoldCheck(void);
void	SetExitRemoveCheck(void);
void SetDAGNodeName();
void SetMatchListLen();
void SetDAGManJobId();
void SetLogNotes();
void SetUserNotes();
void SetJarFiles();
void SetParallelStartupScripts(); //JDB
bool mightTransfer( int universe );

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
template class ExtArray<SubmitRec>;
template class ExtArray<ClassAd*>;

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
		result = attempt_access((char *)name.Value(), ACCESS_READ, uid, gid, scheddAddr);
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not readable by condor.\n", 
					name.Value());
		}
	}

	CheckFilesWrite.startIterations();
	while( ( CheckFilesWrite.iterate( name, junk ) ) )
	{
		result = attempt_access((char *)name.Value(), ACCESS_WRITE, uid, gid, scheddAddr );
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not writeable by condor.\n",
					name.Value());
		}
	}
#endif
}


void
init_job_ad()
{
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

	(void) sprintf (buffer, "%s = %d", ATTR_Q_DATE, (int)time ((time_t *) 0));
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_COMPLETION_DATE);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = \"%s\"", ATTR_OWNER, owner);
	InsertJobExpr (buffer);

#ifdef WIN32
	// put the NT domain into the ad as well
	ntdomain = my_domainname();
	(void) sprintf (buffer, "%s = \"%s\"", ATTR_NT_DOMAIN, ntdomain);
	InsertJobExpr (buffer);
#endif

	(void) sprintf (buffer, "%s = 0.0", ATTR_JOB_REMOTE_WALL_CLOCK);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0.0", ATTR_JOB_LOCAL_USER_CPU);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0.0", ATTR_JOB_LOCAL_SYS_CPU);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0.0", ATTR_JOB_REMOTE_USER_CPU);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0.0", ATTR_JOB_REMOTE_SYS_CPU);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_JOB_EXIT_STATUS);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_NUM_CKPTS);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_NUM_RESTARTS);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_NUM_SYSTEM_HOLDS);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_JOB_COMMITTED_TIME);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_TOTAL_SUSPENSIONS);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_LAST_SUSPENSION_TIME);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_CUMULATIVE_SUSPENSION_TIME);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = FALSE", ATTR_ON_EXIT_BY_SIGNAL);
	InsertJobExpr (buffer);

	config_fill_ad( job );
}

int
main( int argc, char *argv[] )
{
	FILE	*fp;
	char	**ptr;
	char	*cmd_file = NULL;
	int dag_pause = 0;
	int i;

	setbuf( stdout, NULL );

#if !defined(WIN32)
		// Make sure root isn't trying to submit.
	if( getuid() == 0 || getgid() == 0 ) {
		fprintf( stderr, "\nERROR: Submitting jobs as user/group 0 (root) is not "
				 "allowed for security reasons.\n" );
		exit( 1 );
	}
#endif /* not WIN32 */

	//TODO:this should go away, and the owner name be placed in ad by schedd!
	owner = my_username();
	if( !owner ) {
		owner = strdup("unknown");
	}

	MyName = basename(argv[0]);
	myDistro->Init( argc, argv );
	config();

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

	for( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
		if( ptr[0][0] == '-' ) {
			switch( ptr[0][1] ) {
			case 'v':
				Quiet = 0;
				break;
			case 'd':
				if (ptr[0][2] == 'e') {
					// dprintf to console
					Termlog = 1;
					dprintf_config ("TOOL", 2 );
				} else {
					DisableFileChecks = 1;
				}
				break;
			case 's':
				Remote++;
				DisableFileChecks = 1;
				break;
			case 'r':
				Remote++;
				DisableFileChecks = 1;
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -r requires another argument\n",
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
				break;
			case 'n':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -n requires another argument\n",
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
				break;
			case 'a':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -a requires another argument\n",
							 MyName );
					exit( 1 );
				}
				extraLines.Append( *ptr );
				break;
			case 'p':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -p requires another argument\n",
							 MyName );
				}
				myproxy_password = strdup (*ptr);
				break;
			default:
				usage();
			}
		} else {
			cmd_file = *ptr;
		}
	}

	char *dis_check = param("SUBMIT_SKIP_FILECHECKS");
	if ( dis_check ) {
		if (dis_check[0]=='T' || dis_check[0]=='t') {
			DisableFileChecks = 1;
		}
		free(dis_check);
	}

		// Instantiate our DCSchedd so we can locate and communicate
		// with our schedd.  
	MySchedd = new DCSchedd( ScheddName, NULL );
	if( ! MySchedd->locate() ) {
		if( ScheddName ) {
			fprintf( stderr, "\nERROR: Can't find address of schedd %s\n",
					 ScheddName );
		} else {
			fprintf( stderr, "\nERROR: Can't find address of local schedd\n" );
		}
		exit(1);
	}

#ifdef WIN32
	char userdom[256];
	sprintf(userdom, "%s@%s", my_username(), my_domainname());
	if ( queryCredential(userdom) != SUCCESS ) {
		fprintf( stderr, "\nERROR: No credential stored for %s\n"
				"\n\tCorrect this by running:\n"
				"\tcondor_store_cred add\n", userdom );
		exit(1);
	}
#endif

	
	// open submit file
	if ( !cmd_file ) {
		// no file specified, read from stdin
		fp = stdin;
	} else {
		if( (fp=fopen(cmd_file,"r")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open command file\n");
			exit(1);
		}
	}

	// in case things go awry ...
	_EXCEPT_Cleanup = DoCleanup;

	IsFirstExecutable = true;
	init_job_ad();

	if (Quiet) {
		fprintf(stdout, "Submitting job(s)");
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

	if ( !DisconnectQ(0) ) {
		fprintf(stderr, "\nERROR: Failed to commit job submission into the queue.\n");
		exit(1);
	}

	if (Quiet) {
		fprintf(stdout, "\n");
	}

	if (UserLogSpecified) {
		log_submit();
	}

	if (Quiet) {
		int this_cluster = -1, job_count=0;
		for (int i=0; i <= CurrentSubmitInfo; i++) {
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

	if ( Remote && JobAdsArrayLen > 0 ) {
		bool result;
			// perhaps check for proper schedd version here?
		fprintf(stdout,"Spooling data files for %d jobs...\n",JobAdsArrayLen);
		CondorError errstack;
		result = MySchedd->spoolJobFiles( JobAdsArrayLen,
										  JobAdsArray.getarray(),
										  &errstack );
		if ( !result ) {
			fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
			fprintf( stderr, "ERROR: Failed to spool job files.\n" );
			exit(1);
		}
	}

	if(ProcId != -1 ) {
		reschedule();
	}

	if( dag_pause ) {
		sleep(4);
	}

	// Deallocate some memory just to keep Purify happy
	for (i=0;i<JobAdsArrayLen;i++) {
		delete JobAdsArray[i];
	}

	return 0;
}


/*
** Send the reschedule command to the local schedd to get the jobs running
** as soon as possible.
*/
void
reschedule()
{
	if ( param_boolean("SUBMIT_SEND_RESCHEDULE",true) ) {
		if ( ! MySchedd->sendCommand(RESCHEDULE, Stream::safe_sock, 0) ) {
			fprintf( stderr,
					 "Can't send RESCHEDULE command to condor scheduler\n" );
			DoCleanup(0,0,NULL);
			exit( 1 );
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
check_and_universalize_path(MyString &path, char *lhs)
{

	int retval = 0;
#ifdef WIN32
	/*
	 * On Windows, we need to convert all drive letters (mappings) 
	 * to their UNC equivalents, and make sure these network devices
	 * are accessable by the submitting user (not mapped as someone
	 * else).
	 */

	char volume[8];
	char netname[80];
	char *my_name, *my_domain;
	unsigned char name_info_buf[MAX_PATH+1];
	DWORD name_info_buf_size = MAX_PATH+1;
	DWORD netname_size = 80;
	DWORD result;
	UNIVERSAL_NAME_INFO *uni;

	result = 0;
	volume[0] = '\0';
	if ( path[0] && (path[1]==':') ) {
		sprintf(volume,"%c:",path[0]);
	}

	if (volume[0] && (GetDriveType(volume)==DRIVE_REMOTE))
	{
		char my_fullname[255];

			// check if the user is the submitting user.
		
		result = WNetGetUser(volume, netname, &netname_size);
		my_name = my_username();
		my_domain = my_domainname();
		sprintf(my_fullname, "%s\\%s", my_domain, my_name);
		free(my_name);
		free(my_domain);
		my_name = my_domain = NULL;

		if ( result == NOERROR ) {
			if (stricmp(my_fullname, netname) != 0 ) {
				fprintf(stderr, "\nERROR: The path '%s' is associated with\n"
					"\tuser %s, so Condor can not access it.\n"
					"\tCurrently Condor only supports network drives that are \n"
					"\tassociated with the submitting user.\n", 
					path.Value(), netname);
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

	if (path.Length() > _POSIX_PATH_MAX) {
		fprintf(stderr, "\nERROR: Value for \"%s\" is too long:\n"
				"\tPosix limits path names to %d bytes\n",
				lhs, _POSIX_PATH_MAX);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	return retval;
}


void
SetExecutable()
{
	bool	transfer_it = true;
	char	*ename = NULL;
	char	*copySpool = NULL;
	char	*macro_value = NULL;
	MyString	full_ename;

	ename = condor_param( Executable, ATTR_JOB_CMD );
	if( ename == NULL ) {
		fprintf( stderr, "No '%s' parameter was provided\n", Executable);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	macro_value = condor_param( TransferExecutable ) ;
	if ( macro_value ) {
		if ( macro_value[0] == 'F' || macro_value[0] == 'f' ) {
			sprintf( buffer, "%s = FALSE", ATTR_TRANSFER_EXECUTABLE );
			InsertJobExpr( buffer );
			transfer_it = false;
		}
		free( macro_value );
	}

	// If we're not transfering the executable, leave a relative pathname
	// unresolved. This is mainly important for the Globus universe.
	if ( transfer_it ) {
		full_ename = full_path( ename, false );
	} else {
		full_ename = ename;
	}
	check_and_universalize_path(full_ename, Executable);

	(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_CMD, full_ename.Value());
	InsertJobExpr (buffer);

		/* MPI REALLY doesn't like these! */
	if ( JobUniverse != CONDOR_UNIVERSE_MPI && JobUniverse != CONDOR_UNIVERSE_PVM ) {
		InsertJobExpr ("MinHosts = 1");
		InsertJobExpr ("MaxHosts = 1");
	}

	InsertJobExpr ("CurrentHosts = 0");

	switch(JobUniverse) 
	{
	case CONDOR_UNIVERSE_STANDARD:
		(void) sprintf (buffer, "%s = TRUE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		(void) sprintf (buffer, "%s = TRUE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	case CONDOR_UNIVERSE_PVM:
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_MPI:  // for now
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_GLOBUS:
	case CONDOR_UNIVERSE_JAVA:
		(void) sprintf (buffer, "%s = FALSE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		(void) sprintf (buffer, "%s = FALSE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	default:
		fprintf(stderr, "\nERROR: Unknown universe %d (%s)\n", JobUniverse, CondorUniverseName(JobUniverse) );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	copySpool = condor_param( CopyToSpool, "CopyToSpool" );
	if( copySpool == NULL)
	{
		copySpool = (char *)malloc(16);
		if ( JobUniverse == CONDOR_UNIVERSE_GLOBUS && !Remote ) {
			strcpy(copySpool, "FALSE");
		} else {
			strcpy(copySpool, "TRUE");
		}
	}

	// generate initial checkpoint file
	strcpy( IckptName, gen_ckpt_name(0,ClusterId,ICKPT,0) );

	// spool executable only if no $$(arch).$$(opsys) specified

	if ( !strstr(ename,"$$") && *copySpool != 'F' && *copySpool != 'f' &&
		 transfer_it ) {

		if (SendSpoolFile(IckptName) < 0) {
			fprintf( stderr, "\nERROR: Permission to transfer executable "
					 "%s denied\n", IckptName );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		if (SendSpoolFileBytes(full_path(ename,false)) < 0) {
			fprintf( stderr,
					 "\nERROR: failed to transfer executable file %s\n", 
					 ename );
			DoCleanup(0,0,NULL);
			exit( 1 );
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

	univ = condor_param( Universe, ATTR_JOB_UNIVERSE );

	if (!univ) {
		// get a default universe from the config file
		univ = param("DEFAULT_UNIVERSE");
		if( !univ ) {
			// if nothing else, it must be a standard universe
			univ = strdup("standard");
		}
	}

	if( univ && stricmp(univ,"scheduler") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_SCHEDULER;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_SCHEDULER);
		InsertJobExpr (buffer);
		free(univ);
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
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_PVM);
		InsertJobExpr (buffer);

		free(univ);
		return;
	};


	if( univ && 
		((stricmp(univ,"globus") == MATCH) || (stricmp(univ,"grid") == MATCH))) {
		if ( (!Remote) && (have_condor_g() == 0) ) {
			fprintf( stderr, "This version of Condor doesn't support Globus Universe jobs.\n" );
			exit( 1 );
		}
		JobUniverse = CONDOR_UNIVERSE_GLOBUS;
		
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS);
		InsertJobExpr (buffer);
		free(univ);
	
		// Set Grid_Type (for Globus jobs)
		univ = condor_param( Grid_Type, ATTR_JOB_GRID_TYPE );
		if( !univ ) {
			univ = strdup("globus");
		} else {
			// Validate
			// Valid values are (as of 6.7): nordugrid, oracle, gt3, globus,
			//    gt2
			if ((stricmp (univ, "globus") == MATCH) ||
				(stricmp (univ, "gt2") == MATCH) ||
				(stricmp (univ, "gt3") == MATCH) ||
				(stricmp (univ, "nordugrid") == MATCH) ||
				(stricmp (univ, "oracle") == MATCH)) {
				// We're ok	
				// Values are case-insensitive for gridmanager, so we don't need to change case			
			} else {
				fprintf( stderr, "\nERROR: Invalid value '%s' for grid_type\n", univ );
				fprintf( stderr, "Must be one of: globus, gt2, gt3, nordugrid, or oracle\n" );
				exit( 1 );
			}
		}			
		
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_GRID_TYPE, univ);
		InsertJobExpr (buffer);
		free (univ);
		
		return;
	};

	if( univ && stricmp(univ,"parallel") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_PARALLEL;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_PARALLEL);
		InsertJobExpr (buffer);
		
		free(univ);
		return;
	}

#endif  // of !defined(WIN32)

	if( univ && stricmp(univ,"vanilla") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_VANILLA;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_VANILLA);
		InsertJobExpr (buffer);
		free(univ);
		return;
	};

	if( univ && stricmp(univ,"mpi") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_MPI;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_MPI);
		InsertJobExpr (buffer);
		
		free(univ);
		return;
	}

	if( univ && stricmp(univ,"java") == MATCH ) {
		JobUniverse = CONDOR_UNIVERSE_JAVA;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_JAVA);
		InsertJobExpr (buffer);
		free(univ);
		return;
	}


#if defined( CLIPPED )
		// If we got this far, we must be a standard job.  Since we're
		// clipped, that can't possibly work, so warn the user at
		// submit time.  -Derek Wright 6/11/99
	fprintf( stderr, 
			 "\nERROR: You are trying to submit a \"%s\" job to Condor.\n",
			 univ );
	fprintf( stderr, 
			 "This version of Condor only supports \"vanilla\" jobs, which\n"
			 "perform no checkpointing or remote system calls.  See the\n"
			 "Condor manual for details "
			 "(http://www.cs.wisc.edu/condor/manual).\n\n" );
	DoCleanup(0,0,NULL);
	exit( 1 );
#endif

	if(stricmp(univ,"standard")) {
		fprintf( stderr, "\nERROR: I don't know about the '%s' universe.\n",univ);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	JobUniverse = CONDOR_UNIVERSE_STANDARD;
	(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_STANDARD);
	InsertJobExpr (buffer);
	if ( univ ) {
		free(univ);
	}
	
	

	return;
}

void
SetMachineCount()
{
	char	*mach_count;
	char	*ptr;

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
			(void) sprintf (buffer, "%s = %d", ATTR_MIN_HOSTS, tmp);
			InsertJobExpr (buffer);
			
			for ( ; !isdigit(*ptr) && *ptr; ptr++) ;
			if (*ptr != '\0') {
				tmp = atoi(ptr);
			}

			(void) sprintf (buffer, "%s = %d", ATTR_MAX_HOSTS, tmp);
			InsertJobExpr (buffer);
			free(mach_count);
		} else {
			InsertJobExpr ("MinHosts = 1");
			InsertJobExpr ("MaxHosts = 1");
		}

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

		(void) sprintf (buffer, "%s = %d", ATTR_MIN_HOSTS, tmp);
		InsertJobExpr (buffer);
		(void) sprintf (buffer, "%s = %d", ATTR_MAX_HOSTS, tmp);
		InsertJobExpr (buffer);

	}
}

// Note: you must call SetTransferFiles() *before* calling SetImageSize().
void
SetImageSize()
{
	int		size;
	static int executablesize;
	char	*tmp;
	char	*p;
	char    buff[2048];

	tmp = condor_param( ImageSize, ATTR_IMAGE_SIZE );

	// we should only call calc_image_size on the first
	// proc in the cluster, since the executable cannot change.
	if ( ProcId < 1 ) {
		ASSERT (job->LookupString (ATTR_JOB_CMD, buff));
		executablesize = calc_image_size( buff );
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

	(void)sprintf (buffer, "%s = %d", ATTR_IMAGE_SIZE, size);
	InsertJobExpr (buffer);

	(void)sprintf (buffer, "%s = %u", ATTR_EXECUTABLE_SIZE, 
					executablesize);
	InsertJobExpr (buffer);

	(void)sprintf (buffer, "%s = %u", ATTR_DISK_USAGE, 
					executablesize + TransferInputSize);
	InsertJobExpr (buffer);
}

void SetFileOptions()
{
	char *tmp;
	char buffer[ATTRLIST_MAX_EXPRESSION];

	tmp = condor_param( FileRemaps, ATTR_FILE_REMAPS );
	if(tmp) {
		sprintf(buffer,"%s = %s",ATTR_FILE_REMAPS,tmp);
		InsertJobExpr(buffer);
		free(tmp);
	}

	tmp = condor_param(BufferFiles);
	if(tmp) {
		sprintf(buffer,"%s = %s",ATTR_BUFFER_FILES,tmp);
		InsertJobExpr(buffer);
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
	sprintf(buffer,"%s = %s",ATTR_BUFFER_SIZE,tmp);
	InsertJobExpr(buffer);
	free(tmp);

	/* If not buffer block size is given, use 32 KB */

	tmp = condor_param( BufferBlockSize, ATTR_BUFFER_BLOCK_SIZE );
	if(!tmp) {
		tmp = param("DEFAULT_IO_BUFFER_BLOCK_SIZE");
		if (!tmp) {
			tmp = strdup("32768");
		}
	}
	sprintf(buffer,"%s = %s",ATTR_BUFFER_BLOCK_SIZE,tmp);
	InsertJobExpr(buffer);
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

	if( stat(full_path(name),&buf) < 0 ) {
		// EXCEPT( "Cannot stat \"%s\"", name );
		return 0;
	}
	return (buf.st_size + 1023) / 1024;
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
	char	 buffer[ATTRLIST_MAX_EXPRESSION];
	char	 input_files[ATTRLIST_MAX_EXPRESSION];
	char	 output_files[ATTRLIST_MAX_EXPRESSION];
	StringList input_file_list(NULL, ",");

	should_transfer = STF_YES;

	buffer[0] = input_files[0] = output_files[0] = '\0';

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
		input_file_list.rewind();
		count = 0;
		while ( (tmp_ptr=input_file_list.next()) ) {
			count++;
			tmp = tmp_ptr;
			if ( check_and_universalize_path(tmp,TransferInputFiles) != 0) {
				// path was universalized, so update the string list
				input_file_list.deleteCurrent();
				input_file_list.insert(tmp.Value());
			}
			check_open(tmp.Value(), O_RDONLY);
			TransferInputSize += calc_image_size(tmp.Value());
		}
		if ( count ) {
			tmp_ptr = input_file_list.print_to_string();
			(void) sprintf( input_files, "%s = \"%s\"", 
							ATTR_TRANSFER_INPUT_FILES, tmp_ptr );
			free( tmp_ptr );
			in_files_specified = true;
		}
	}


	macro_value = condor_param( TransferOutputFiles,
								"TransferOutputFiles" ); 
	if( macro_value ) 
	{
		StringList files(macro_value,",");
		files.rewind();
		count = 0;
		while ( (tmp_ptr=files.next()) ) {
			count++;
			tmp = tmp_ptr;
			if ( check_and_universalize_path(tmp,TransferOutputFiles) != 0) 
			{
				// we universalized the path, so update the string list
				files.deleteCurrent();
				files.insert(tmp.Value());
			}
			check_open(tmp.Value(), O_WRONLY|O_CREAT|O_TRUNC );
		}
		tmp_ptr = files.print_to_string();
		if ( count ) {
			(void) sprintf (output_files, "%s = \"%s\"", 
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
	  In the Java universe, if we might be transfering files, we want
	  to automatically transfer the "executable" (i.e. the entry
	  class) and any requested .jar files.  However, the executable is
	  put directly into TransferInputFiles with TransferExecutable set
	  to false, because the FileTransfer object happily renames
	  executables left and right, which we don't want.
	*/

	if( should_transfer!=STF_NO && JobUniverse==CONDOR_UNIVERSE_JAVA ) {

		char file_list[ATTRLIST_MAX_EXPRESSION];

		if(job->LookupString(ATTR_TRANSFER_INPUT_FILES,file_list)!=1) {
			file_list[0] = 0;
		}

		macro_value = condor_param(Executable);
		if(macro_value) {
			MyString executable_str = macro_value;
			check_and_universalize_path(executable_str,TransferInputFiles);
			check_open(executable_str.Value(),O_RDONLY);
			TransferInputSize += calc_image_size(executable_str.Value());
			if(file_list[0]) {
				strcat(file_list,",");
				strcat(file_list,executable_str.Value());
			} else {
				strcpy(file_list,executable_str.Value());
			}
			free(macro_value);
		}

		macro_value = condor_param(JarFiles);
		if(macro_value) {
			StringList files(macro_value);
			files.rewind();
			while ( (tmp_ptr=files.next()) ) {
				tmp = tmp_ptr;
				check_and_universalize_path(tmp,TransferInputFiles);
				check_open(tmp.Value(), O_RDONLY);
				TransferInputSize += calc_image_size(tmp.Value());
				if(file_list[0]) {
					strcat(file_list,",");
					strcat(file_list,tmp.Value());
				} else {
					strcpy(file_list,tmp.Value());
				}
			}
			free(macro_value);
		}

		sprintf(buffer,"%s = \"%s\"",ATTR_TRANSFER_INPUT_FILES,file_list);
		InsertJobExpr(buffer);

		sprintf(buffer,"%s = \"java\"",ATTR_JOB_CMD);
		InsertJobExpr( buffer );

		sprintf(buffer,"%s = FALSE",ATTR_TRANSFER_EXECUTABLE);
		InsertJobExpr( buffer );
	}

	// If either stdout or stderr contains path information, and
	// the file is being transfered back, use an intermediate
	// file in the working directory.  The shadow will move the
	// output data to the user-specified path when the job finishes.
	//
	// NOTE: this behavior is likely to surprise new universes
	// which have non-standard shadow behavior.  For example, the
	// globus universe doesn't have a shadow at all!  While the 
	// globus universe has been special cased out below, perhaps
	// this is a flawed strategy.

	if ( should_transfer != STF_NO && job 
					&& JobUniverse != CONDOR_UNIVERSE_GLOBUS ) {
		char output[_POSIX_PATH_MAX + 32];
		char error[_POSIX_PATH_MAX + 32];

		output[0] = error[0] = '\0';
		job->LookupString(ATTR_JOB_OUTPUT,output,sizeof(output));
		job->LookupString(ATTR_JOB_ERROR,error,sizeof(error));

		if(*output && basename(output) != output && 
		   strcmp(output,"/dev/null") != 0)
		{
			sprintf(buffer,"%s = \"%s\"",ATTR_JOB_OUTPUT_ORIG,output);
			InsertJobExpr(buffer);
			sprintf(
					buffer,
					"%s = \"_condor_stdout_%d.%d\"",
					ATTR_JOB_OUTPUT,ClusterId,ProcId);
			InsertJobExpr(buffer);
		}

		if(*error && basename(error) != error && 
		   strcmp(error,"/dev/null") != 0)
		{
			sprintf(buffer,"%s = \"%s\"",ATTR_JOB_ERROR_ORIG,error);
			InsertJobExpr(buffer);
			
			if(strcmp(error,output) == 0) {
				//stderr will use same file as stdout
				sprintf(
						buffer,
						"%s = \"_condor_stdout_%d.%d\"",
						ATTR_JOB_ERROR,ClusterId,ProcId);
			}
			else {
				sprintf(
						buffer,
						"%s = \"_condor_stderr_%d.%d\"",
						ATTR_JOB_ERROR,ClusterId,ProcId);
			}
			InsertJobExpr(buffer);
		}
	}

	// if we might be using file transfer, insert in input/output
	// exprs  
	if( should_transfer != STF_NO ) {
		if ( input_files[0] ) {
			InsertJobExpr (input_files);
		}
		if ( output_files[0] ) {
			InsertJobExpr (output_files);
		}
	}

	if( should_transfer == STF_NO && 
		JobUniverse != CONDOR_UNIVERSE_GLOBUS &&
		JobUniverse != CONDOR_UNIVERSE_JAVA )
	{
		char *transfer_exe;
		transfer_exe = condor_param(TransferExecutable);
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
}


bool
SetNewTransferFiles( void )
{
	bool found_it = false;
	char *should, *when;
	FileTransferOutput_t when_output;
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
	FileTransferOutput_t when_output;

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
	}
	InsertJobExpr( ft.Value() );
}


void
SetFetchFiles()
{
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char *value;

	value = condor_param( FetchFiles );
	if(value) {
		sprintf(buffer,"%s = \"%s\"",ATTR_FETCH_FILES,value);
		InsertJobExpr (buffer);
	}
}

void
SetCompressFiles()
{
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char *value;

	value = condor_param( CompressFiles );
	if(value) {
		sprintf(buffer,"%s = \"%s\"",ATTR_COMPRESS_FILES,value);
		InsertJobExpr (buffer);
	}
}

void
SetAppendFiles()
{
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char *value;

	value = condor_param( AppendFiles );
	if(value) {
		sprintf(buffer,"%s = \"%s\"",ATTR_APPEND_FILES,value);
		InsertJobExpr (buffer);
	}
}

void
SetLocalFiles()
{
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char *value;

	value = condor_param( LocalFiles );
	if(value) {
		sprintf(buffer,"%s = \"%s\"",ATTR_LOCAL_FILES,value);
		InsertJobExpr (buffer);
	}
}

void
SetJarFiles()
{
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char *value;

	value = condor_param( JarFiles );
	if(value) {
		sprintf(buffer,"%s = \"%s\"",ATTR_JAR_FILES,value);
		InsertJobExpr (buffer);
	}
}

void
SetParallelStartupScripts() //JDB
{
	char buffer[ATTRLIST_MAX_EXPRESSION];
	char *value;

	value = condor_param( ParallelScriptShadow );
	if(value) {
		sprintf(buffer,"%s = \"%s\"",ATTR_PARALLEL_SCRIPT_SHADOW,value);
		InsertJobExpr (buffer);
	}
	value = condor_param( ParallelScriptStarter );
	if(value) {
		sprintf(buffer,"%s = \"%s\"",ATTR_PARALLEL_SCRIPT_STARTER,value);
		InsertJobExpr (buffer);
	}
}

void
SetStdFile( int which_file )
{
	bool	transfer_it = true;
	bool	stream_it = true;
	char	*macro_value = NULL;
	char	*macro_value2 = NULL;
	char	*generic_name;
	char	 buffer[_POSIX_PATH_MAX + 32];

	if(JobUniverse==CONDOR_UNIVERSE_GLOBUS) {
		stream_it = true;
	} else {
		stream_it = false;
	}

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
	if(JobUniverse == CONDOR_UNIVERSE_GLOBUS && is_globus_friendly_url(macro_value)) {
		transfer_it = false;
		stream_it = false;
	}
	
	if( !macro_value || *macro_value == '\0') 
	{
		transfer_it = false;
		stream_it = false;
		// always canonicalize to the UNIX null file (i.e. /dev/null)
		macro_value = strdup(UNIX_NULL_FILE);
	}
	
	if( whitespace(macro_value) ) 
	{
		fprintf( stderr,"\nERROR: The '%s' takes exactly one argument (%s)\n", 
				 generic_name, macro_value );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}	

	MyString tmp = macro_value;
	if ( check_and_universalize_path(tmp, generic_name) != 0 ) {
		// we changed the value, so we have to update macro_value
		free(macro_value);
		macro_value = strdup(tmp.Value());
	}
	
	switch( which_file ) 
	{
	case 0:
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_INPUT, macro_value);
		InsertJobExpr (buffer);
		if ( transfer_it ) {
			check_open( macro_value, O_RDONLY );
			sprintf( buffer, "%s = %s", ATTR_STREAM_INPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( buffer );
		} else {
			sprintf( buffer, "%s = FALSE", ATTR_TRANSFER_INPUT );
			InsertJobExpr( buffer );
		}
		break;
	case 1:
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_OUTPUT, macro_value);
		InsertJobExpr (buffer);
		if ( transfer_it ) {
			check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			sprintf( buffer, "%s = %s", ATTR_STREAM_OUTPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( buffer );
		} else {
			sprintf( buffer, "%s = FALSE", ATTR_TRANSFER_OUTPUT );
			InsertJobExpr( buffer );
		}
		break;
	case 2:
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ERROR, macro_value);
		InsertJobExpr (buffer);
		if ( transfer_it ) {
			check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			sprintf( buffer, "%s = %s", ATTR_STREAM_ERROR, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( buffer );
		} else {
			sprintf( buffer, "%s = FALSE", ATTR_TRANSFER_ERROR );
			InsertJobExpr( buffer );
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

	if( hold && (hold[0] == 'T' || hold[0] == 't') ) {
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer);

		(void)sprintf( buffer, "%s=\"submitted on hold at user's request\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );
	} else 
	if ( Remote ) {
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer);

		(void)sprintf( buffer, "%s=\"Spooling input data files\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );
	} else {
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_STATUS, IDLE);
		InsertJobExpr (buffer);
	}

	(void) sprintf( buffer, "%s = %d", ATTR_ENTERED_CURRENT_STATUS,
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

	if( prio != NULL ) 
	{
		prioval = atoi (prio);
		if( prioval < -20 || prioval > 20 ) 
		{
			fprintf( stderr, "\nERROR: Priority must be in the range "
					 "-20 thru 20 (%d)\n", prioval );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		free(prio);
	}
	(void) sprintf (buffer, "%s = %d", ATTR_JOB_PRIO, prioval);
	InsertJobExpr (buffer);

	// also check if the job is "dirt user" priority (i.e., nice_user==True)
	char *nice_user = condor_param( NiceUser, ATTR_NICE_USER );
	if( nice_user && (*nice_user == 'T' || *nice_user == 't') )
	{
		sprintf( buffer, "%s = TRUE", ATTR_NICE_USER );
		InsertJobExpr( buffer );
		free(nice_user);
	}
	else
	{
		sprintf( buffer, "%s = FALSE", ATTR_NICE_USER );
		InsertJobExpr( buffer );

	}
}

void
SetPeriodicHoldCheck(void)
{
	char *phc = condor_param(PeriodicHoldCheck, ATTR_PERIODIC_HOLD_CHECK);

	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		sprintf( buffer, "%s = FALSE", ATTR_PERIODIC_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		sprintf( buffer, "%s = %s", ATTR_PERIODIC_HOLD_CHECK, phc );
		free(phc);
	}

	InsertJobExpr( buffer );

	phc = condor_param(PeriodicReleaseCheck, ATTR_PERIODIC_RELEASE_CHECK);

	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		sprintf( buffer, "%s = FALSE", ATTR_PERIODIC_RELEASE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		sprintf( buffer, "%s = %s", ATTR_PERIODIC_RELEASE_CHECK, phc );
		free(phc);
	}

	InsertJobExpr( buffer );
}

void
SetPeriodicRemoveCheck(void)
{
	char *prc = condor_param(PeriodicRemoveCheck, ATTR_PERIODIC_REMOVE_CHECK);

	if (prc == NULL)
	{
		/* user didn't have one, so add one */
		sprintf( buffer, "%s = FALSE", ATTR_PERIODIC_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		sprintf( buffer, "%s = %s", ATTR_PERIODIC_REMOVE_CHECK, prc );
		free(prc);
	}

	InsertJobExpr( buffer );
}

void
SetExitHoldCheck(void)
{
	char *ehc = condor_param(OnExitHoldCheck, ATTR_ON_EXIT_HOLD_CHECK);

	if (ehc == NULL)
	{
		/* user didn't have one, so add one */
		sprintf( buffer, "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		sprintf( buffer, "%s = %s", ATTR_ON_EXIT_HOLD_CHECK, ehc );
		free(ehc);
	}

	InsertJobExpr( buffer );
}

void
SetLeaveInQueue()
{
	char *erc = condor_param(LeaveInQueue, ATTR_JOB_LEAVE_IN_QUEUE);
	char *mirror_schedd = condor_param(MirrorSchedd,ATTR_MIRROR_SCHEDD);

	if ( mirror_schedd ) {
		
		if ( erc ) {
			fprintf( stderr, 
				"\nERROR: %s may not be specified alongside %s - the mirroring"
				" mechanism relies on setting %s itself\n",
				 LeaveInQueue, MirrorSchedd, LeaveInQueue);
			DoCleanup(0,0,NULL);
			exit(1);
		}
		
			// schedd job mirroring is being used
			// set the mirrored schedd attribute
		sprintf(buffer,"%s = \"%s\"", ATTR_MIRROR_SCHEDD,mirror_schedd);
		InsertJobExpr( buffer );
		free(mirror_schedd);

			// set default WantMatching for mirrored jobs
		sprintf(buffer,"%s = %s =?= False", ATTR_WANT_MATCHING,
			ATTR_MIRROR_RELEASED);
		InsertJobExpr( buffer );

			// set default LeaveInQueue for mirrored jobs (erc is used below)
		sprintf(buffer,"%s =?= True",ATTR_JOB_MANAGED);
		erc = strdup(buffer);
	}


	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		if ( !Remote ) {
			sprintf( buffer, "%s = FALSE", ATTR_JOB_LEAVE_IN_QUEUE );
		} else {
				/* if remote spooling, leave in the queue after the job completes
				   for up to 10 days, so user can grab the output.
				 */
			sprintf( buffer, 
				"%s = %s == %d && (%s =?= UNDEFINED || ((CurrentTime - %s) < %d))",
				ATTR_JOB_LEAVE_IN_QUEUE,
				ATTR_JOB_STATUS,
				COMPLETED,
				ATTR_COMPLETION_DATE,
				ATTR_COMPLETION_DATE,
				60 * 60 * 24 * 10 
				);
		}
	}
	else
	{
		/* user had a value for it, leave it alone */
		sprintf( buffer, "%s = %s", ATTR_JOB_LEAVE_IN_QUEUE, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
}


void
SetExitRemoveCheck(void)
{
	char *erc = condor_param(OnExitRemoveCheck, ATTR_ON_EXIT_REMOVE_CHECK);

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		sprintf( buffer, "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		sprintf( buffer, "%s = %s", ATTR_ON_EXIT_REMOVE_CHECK, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
}

void
SetNotification()
{
	char *how = condor_param( Notification, ATTR_JOB_NOTIFICATION );
	int notification;

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

	(void) sprintf (buffer, "%s = %d", ATTR_JOB_NOTIFICATION, notification);
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
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_NOTIFY_USER, who);
		InsertJobExpr (buffer);
		free(who);
	}
}

void
SetMatchListLen()
{
	int len = 0;
	char* tmp = condor_param( LastMatchListLength, ATTR_LAST_MATCH_LIST_LENGTH );
	if( tmp ) {
		len = atoi(tmp);
		(void) sprintf( buffer, "%s = %d", ATTR_LAST_MATCH_LIST_LENGTH, len );
		InsertJobExpr( buffer );
		free( tmp );
	}
}

void
SetDAGNodeName()
{
	char* name = condor_param( DAGNodeName );
	if( !name ) {
		name = condor_param( "DAGNodeName" );
	}
	if( name ) {
		(void) sprintf( buffer, "%s = \"%s\"", ATTR_DAG_NODE_NAME, name );
		InsertJobExpr( buffer );
		free( name );
	}
}

void
SetDAGManJobId()
{
	char* id = condor_param( DAGManJobId );
	if( !id ) {
		id = condor_param( "DAGManJobId" );
	}
	if( id ) {
		(void) sprintf( buffer, "%s = \"%s\"", ATTR_DAGMAN_JOB_ID, id );
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
	UserNotesVal = condor_param( UserNotesCommand );
	// just in case the user forgets the underscores
	if( !UserNotesVal ) {
		UserNotesVal = condor_param( "SubmitEventUserNotes" );
	}
}

void
SetRemoteInitialDir()
{
	char *who = condor_param(RemoteInitialDir);

	if( ! who ) {
			// isn't there, try ClassAd flavor
		who = condor_param( ATTR_JOB_REMOTE_IWD );
	}

	if (who) {
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_REMOTE_IWD, who);
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
	char	*args = NULL;

	args = condor_param( Arguments, ATTR_JOB_ARGUMENTS );

	if( args == NULL ) {
		args = strdup("");
	} 
	else if (strlen(args) > _POSIX_ARG_MAX) {
		fprintf(stderr, "\nERROR: arguments are too long:\n"
				"\tPosix limits argument lists to %d bytes\n",
				_POSIX_ARG_MAX);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ARGUMENTS, args);
	InsertJobExpr (buffer);

	if( JobUniverse == CONDOR_UNIVERSE_JAVA && !*args)
	{
		fprintf(stderr, "\nERROR: In Java universe, you must specify the class name to run.\nExample:\n\narguments = MyClass\n\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	free(args);
}

void
SetEnvironment()
{
	char *env = condor_param( Environment, ATTR_JOB_ENVIRONMENT );
	char *shouldgetenv = condor_param( GetEnv, "get_env" );
	char *allowscripts = condor_param( AllowStartupScript,
									   "AllowStartupScript" );
	Env envobject;
	MyString newenv;
	char varname[MAXVARNAME];
	int envlen;

	envobject.GenerateParseMessages();
	if(!envobject.Merge(env)) {
		char const *err_msg = envobject.GetParseMessages();
		if(!err_msg || !*err_msg) err_msg = "ERROR parsing environment.";
		fprintf(stderr,
		  "\n%s\nThe environment you specified was: '%s'\n",err_msg,env);
		exit(1);
	}

	char const *warning_msg = envobject.GetParseMessages();
	if(warning_msg && *warning_msg) {
		fprintf(stderr,
		  "\n%s\nThe environment you specified was %s\n",warning_msg,env);
	}

	if (allowscripts && (*allowscripts=='T' || *allowscripts=='t') ) {
		envobject.Put("_CONDOR_NOCHECK","1");
	}

	envlen = newenv.Length();

	// grab user's environment if getenv == TRUE
	if ( shouldgetenv && ( shouldgetenv[0] == 'T' || shouldgetenv[0] == 't' ) )
 	{
		for (int i=0; environ[i]; i++) {
			if(!envobject.IsSafeEnvValue(environ[i])) {
				// We silently filter out anything that is not expressible
				// in the current environment string syntax.
				continue;
			}

			envlen += strlen(environ[i]);
			// don't override submit file environment settings
			// check if environment variable is set in submit file
			int j;
			for (j=0; env && environ[i][j] && environ[i][j] != '='; j++) {
				varname[j] = environ[i][j];
			}
			varname[j] = '\0';
			MyString existing_val;
			if (!envobject.getenv(varname,existing_val)) {
				envobject.Put(environ[i]);
			}
		}
	}

	char *newenv_unescaped = envobject.getDelimitedString();
	MyString E(newenv_unescaped);
	newenv += ATTR_JOB_ENVIRONMENT;
	newenv += " = \"";
	newenv += E.EscapeChars("\"",'\\');
	newenv += "\"";

	InsertJobExpr (newenv.Value());

	if( newenv_unescaped ) {
		delete[] newenv_unescaped;
	}
	if( env ) {
		free(env);
	}
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
		(void) strcpy (JobRootdir, "/");
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
		check_and_universalize_path(rootdir_str, RootDir);
		(void) strcpy (JobRootdir, rootdir_str.Value());
		free(rootdir);
	}
}

void
SetRootDir()
{
	ComputeRootDir();
	(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ROOT_DIR, JobRootdir);
	InsertJobExpr (buffer);
}
#endif

void
SetRequirements()
{
	char *requirements = condor_param( Requirements, NULL );
	char *tmp;
	if( requirements == NULL ) 
	{
		(void) strcpy (JobRequirements, "");
	} 
	else 
	{
		(void) strcpy (JobRequirements, requirements);
		free(requirements);
	}

	tmp = check_requirements( JobRequirements );
	(void) sprintf (buffer, "%s = %s", ATTR_REQUIREMENTS, tmp);
	strcpy (JobRequirements, tmp);

	InsertJobExpr (buffer);
	
	char* fs_domain = NULL;
	if( (should_transfer == STF_NO || should_transfer == STF_IF_NEEDED) 
		&& ! job->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &fs_domain) ) {
		sprintf( buffer, "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, 
				 My_fs_domain ); 
		InsertJobExpr( buffer );
	}
	if( fs_domain ) {
		free( fs_domain );
	}
}

void
SetRank()
{
	static char rank[ATTRLIST_MAX_EXPRESSION];
	char *orig_pref = condor_param( Preferences, NULL );
	char *orig_rank = condor_param( Rank, NULL );
	char *default_rank = NULL;
	char *append_rank = NULL;
	rank[0] = '\0';

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
		default_rank = param("DEFAULT_RANK");
	}
	if( ! append_rank || ! append_rank[0]  ) {
		append_rank = param("APPEND_RANK");
	}

		// If any of these are defined but empty, treat them as
		// undefined, or else, we get nasty errors.  -Derek W. 8/21/98
	if( default_rank && !default_rank[0] ) {
		default_rank = NULL;
	}
	if( append_rank && !append_rank[0] ) {
		append_rank = NULL;
	}

		// If we've got a rank to append to something that's already 
		// there, we need to enclose the origional in ()'s
	if( append_rank && (orig_rank || orig_pref || default_rank) ) {
		(void)strcat( rank, "(" );
	}		

	if( orig_pref && orig_rank ) {
		fprintf( stderr, "\nERROR: %s and %s may not both be specified "
				 "for a job\n", Preferences, Rank );
		exit(1);
	} else if( orig_rank ) {
		(void)strcat( rank, orig_rank );
	} else if( orig_pref ) {
		(void)strcat( rank, orig_pref );
	} else if( default_rank ) {
		(void)strcat( rank, default_rank );
	} 

	if( append_rank ) {
		if( rank[0] ) {
				// We really want to add this expression to our
				// existing Rank expression, not && it, since Rank is
				// just a float.  If we && it, we're forcing the whole
				// expression to now be a bool that evaluates to
				// either 0 or 1, which is obviously not what we
				// want.  -Derek W. 8/21/98
			(void)strcat( rank, ") + (" );
		} else {
			(void)strcat( rank, "(" );
		}
		(void)strcat( rank, append_rank );
		(void)strcat( rank, ")" );
	}
				
	if( rank[0] == '\0' ) {
		(void)sprintf( buffer, "%s = 0.0", ATTR_RANK );
		InsertJobExpr( buffer );
	} else {
		(void)sprintf( buffer, "%s = %s", ATTR_RANK, rank );
		InsertJobExpr( buffer );
	}

	if ( orig_pref ) 
		free(orig_pref);
	if ( orig_rank )
		free(orig_rank);
}

void
ComputeIWD()
{
	char	*shortname;
	char	iwd[ _POSIX_PATH_MAX ];
	char	cwd[ _POSIX_PATH_MAX ];

	memset(iwd, 0, sizeof(iwd));
	memset(cwd, 0, sizeof(cwd));

	shortname = condor_param( InitialDir, ATTR_JOB_IWD );
	if( ! shortname ) {
			// neither "initialdir" nor "iwd" were there, try some
			// others, just to be safe:
		shortname = condor_param( "initial_dir", "job_iwd" );
	}

#if !defined(WIN32)
	ComputeRootDir();
	if( strcmp(JobRootdir,"/") != MATCH )	{	/* Rootdir specified */
		if( shortname ) {
			(void)strcpy( iwd, shortname );
		} 
		else {
			(void)strcpy( iwd, "/" );
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
				(void)strcpy( iwd, shortname );
			} 
			else {
				(void)getcwd( cwd, sizeof cwd );
				(void)sprintf( iwd, "%s%c%s", cwd, DIR_DELIM_CHAR, shortname );
			}
		} 
		else {
			(void)getcwd( iwd, sizeof iwd );
		}
	}

	compress( iwd );
	MyString iwd_str = iwd;
	if ( check_and_universalize_path(iwd_str, InitialDir) != 0 ) {
		// we universalized the path, so we have to update iwd
		strcpy(iwd, iwd_str.Value());
	}
	check_iwd( iwd );
	strcpy (JobIwd, iwd);

	if ( shortname )
		free(shortname);
}

void
SetIWD()
{
	ComputeIWD();
	(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_IWD, JobIwd);
	InsertJobExpr (buffer);
}

void
check_iwd( char *iwd )
{
	char	pathname[ _POSIX_PATH_MAX ];
	pathname[0] = '\0';

#if defined(WIN32)
	(void)sprintf( pathname, "%s", iwd );
#else
	(void)sprintf( pathname, "%s/%s", JobRootdir, iwd );
#endif
	compress( pathname );

	if( access(pathname, F_OK|X_OK) < 0 ) {
		fprintf( stderr, "\nERROR: No such directory: %s\n", pathname );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}

void
SetUserLog()
{
	char *ulog_entry = condor_param( UserLogFile, ATTR_ULOG_FILE );

	if (ulog_entry) {
		if (whitespace(ulog_entry)) {
			fprintf( stderr, "\nERROR: Only one %s can be specified.\n",
					 UserLogFile );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		MyString ulog = full_path(ulog_entry);
		free(ulog_entry);

		// check that the log is a valid path
		FILE* test = fopen(ulog.Value(), "a+");
		if (!test) {
			fprintf(stderr,
				"\nWARNING: Invalid log file: \"%s\"\n", ulog.Value());
			exit( 1 );
		} else {
			fclose(test);
		}

		check_and_universalize_path(ulog, UserLogFile);
		(void) sprintf(buffer, "%s = \"%s\"", ATTR_ULOG_FILE, ulog.Value());
		InsertJobExpr(buffer);
		UserLogSpecified = true;
	}
}

void
SetUserLogXML()
{
	char *use_xml = condor_param(UseLogUseXML,
								 ATTR_ULOG_USE_XML);

	if (use_xml) {
		if ('T' == *use_xml || 't' == *use_xml) {
			UseXMLInLog = true;
			sprintf(buffer, "%s = TRUE", ATTR_ULOG_USE_XML);
			InsertJobExpr( buffer );
		} else {
			sprintf(buffer, "%s = FALSE", ATTR_ULOG_USE_XML);
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

	if (size == NULL) {
#if defined(HPUX) || defined(WIN32) /* RLIMIT_CORE not supported */
		size = "";
#else
		struct rlimit rl;
		if ( getrlimit( RLIMIT_CORE, &rl ) == -1) {
			EXCEPT("getrlimit failed");
		}

		/* RLIM_INFINITY is a magic cookie, don't try converting to kbytes */
		if( rl.rlim_cur == RLIM_INFINITY) {
			coresize = (long)rl.rlim_cur;
		} else {
			coresize = (long)rl.rlim_cur/1024;
		}
#endif
	} else {
		coresize = atoi(size);
		free(size);
	}

	(void)sprintf (buffer, "%s = %ld", ATTR_CORE_SIZE, coresize);
	InsertJobExpr(buffer);
}


void
SetJobLease( void )
{
	static bool warned_too_small = false;
	char *tmp = condor_param( "job_lease_duration", ATTR_JOB_LEASE_DURATION );
	if( ! tmp ) {
		return;
	}
	if( ! universeCanReconnect(JobUniverse) ) { 
		fprintf( stderr, "\nERROR: cannot specify %s for %s universe jobs\n",
				 ATTR_JOB_LEASE_DURATION, CondorUniverseName(JobUniverse) );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	int lease_duration = atoi( tmp );
	if( lease_duration <= 0 ) {
		fprintf( stderr, "\nERROR: invalid %s given: %s\n",
				 ATTR_JOB_LEASE_DURATION, tmp );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	if( lease_duration < 20 ) {
		if( ! warned_too_small ) { 
			fprintf( stderr, "\nWARNING: %s less than 20 is not allowed, "
					 "using 20 instead\n", ATTR_JOB_LEASE_DURATION );
			warned_too_small = true;
		}
		lease_duration = 20;
	}
	if( stream_std_file ) {
		fprintf( stderr, "\nERROR: job_lease_duration cannot be used with stream_input, stream_output, or stream_error.\n");
		DoCleanup(0,0,NULL);
		exit(1);
	}
	MyString val = ATTR_JOB_LEASE_DURATION;
	val += "=";
	val += lease_duration;
	InsertJobExpr( val.Value() );
}


void
SetForcedAttributes()
{
	MyString	name;
	MyString	value;
	char		*exValue;

	forcedAttributes.startIterations();
	while( ( forcedAttributes.iterate( name, value ) ) )
	{
		// expand the value; and insert it into the job ad
		exValue = expand_macro( (char*)value.Value(), ProcVars, PROCVARSIZE );
		if( !exValue )
		{
			fprintf( stderr, "\nWarning: Unable to expand macros in \"%s\"."
					 "  Ignoring.\n", value.Value() );
			continue;
		}
		sprintf( buffer, "%s = %s", name.Value(), exValue );
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
	char buff[2048];
	char *globushost;
	char *tmp;
	char *use_gridshell;

	if ( JobUniverse != CONDOR_UNIVERSE_GLOBUS )
		return;

	if ( !(globushost = condor_param( GlobusScheduler ) ) ) {
		char * grid_type = condor_param( Grid_Type, ATTR_JOB_GRID_TYPE );
		if ((grid_type == NULL ||
				(stricmp (grid_type, "globus") == MATCH) ||
				(stricmp (grid_type, "gt2") == MATCH) ||
				(stricmp (grid_type, "gt3") == MATCH) ||
				(stricmp (grid_type, "nordugrid") == MATCH))) {
			fprintf(stderr, "\nERROR: Globus/gt3 jobs require a \"%s\" parameter\n",
					GlobusScheduler );
			DoCleanup( 0, 0, NULL );
			exit( 1 );
		}
		if (grid_type != NULL)
			free (grid_type);
	}

	sprintf( buffer, "%s = \"%s\"", ATTR_GLOBUS_RESOURCE, globushost );
	InsertJobExpr (buffer);

	if ( strstr(globushost,"$$") ) {
		// We need to perform matchmaking on the job in order to find
		// the GlobusScheduler.
		sprintf(buffer,"%s = FALSE", ATTR_JOB_MATCHED);
		InsertJobExpr (buffer);
		sprintf(buffer,"%s = 0", ATTR_CURRENT_HOSTS);
		InsertJobExpr (buffer);
		sprintf(buffer,"%s = 1", ATTR_MAX_HOSTS);
		InsertJobExpr (buffer);
	}

	free( globushost );

	if ( (use_gridshell = condor_param(GridShell)) ) {
		if( use_gridshell[0] == 't' || use_gridshell[0] == 'T' ) {
			MyString tmp;
			tmp.sprintf( "%s = TRUE", ATTR_USE_GRID_SHELL );
			InsertJobExpr( tmp.GetCStr() );
		}
		free(use_gridshell);
	}

	sprintf( buffer, "%s = \"%s\"", ATTR_GLOBUS_CONTACT_STRING,
			 NULL_JOB_CONTACT );
	InsertJobExpr (buffer);

	sprintf( buffer, "%s = %d", ATTR_GLOBUS_STATUS,
			 GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED );
	InsertJobExpr (buffer);

	sprintf( buffer, "%s = False", ATTR_WANT_CLAIMING );
	InsertJobExpr(buffer);

	sprintf( buffer, "%s = 0", ATTR_NUM_GLOBUS_SUBMITS );
	InsertJobExpr (buffer, false );

	if( (tmp = condor_param(GlobusResubmit,ATTR_GLOBUS_RESUBMIT_CHECK)) ) {
		sprintf( buff, "%s = %s", ATTR_GLOBUS_RESUBMIT_CHECK, tmp );
		free(tmp);
		InsertJobExpr (buff, false );
	} else {
		sprintf( buff, "%s = FALSE", ATTR_GLOBUS_RESUBMIT_CHECK);
		InsertJobExpr (buff, false );
	}

	if( (tmp = condor_param(GlobusRematch,ATTR_REMATCH_CHECK)) ) {
		sprintf( buff, "%s = %s", ATTR_REMATCH_CHECK, tmp );
		free(tmp);
		InsertJobExpr (buff, false );
	}

	if( (tmp = condor_param(GlobusRSL)) ) {
		sprintf( buff, "%s = \"%s\"", ATTR_GLOBUS_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buff );
	}



	//ckireyev: MyProxy-related crap
	if ((tmp = condor_param (ATTR_MYPROXY_HOST_NAME))) {
		sprintf (buff, "%s = \"%s\"", ATTR_MYPROXY_HOST_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buff );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_SERVER_DN))) {
		sprintf (buff, "%s = \"%s\"", ATTR_MYPROXY_SERVER_DN, tmp );
		free( tmp );
		InsertJobExpr ( buff );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_PASSWORD))) {
		if (myproxy_password == NULL) {
			myproxy_password = tmp;
		}
	}

	if ((tmp = condor_param (ATTR_MYPROXY_CRED_NAME))) {
		sprintf (buff, "%s = \"%s\"", ATTR_MYPROXY_CRED_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buff );
	}

	if (myproxy_password) {
		sprintf (buff, "%s = %s", ATTR_MYPROXY_PASSWORD, myproxy_password);
		InsertJobExpr (buff);
	}

	if ((tmp = condor_param (ATTR_MYPROXY_REFRESH_THRESHOLD))) {
		sprintf (buff, "%s = %s", ATTR_MYPROXY_REFRESH_THRESHOLD, tmp );
		free( tmp );
		InsertJobExpr ( buff );
	}

	if ((tmp = condor_param (ATTR_MYPROXY_NEW_PROXY_LIFETIME))) { 
		sprintf (buff, "%s = %s", ATTR_MYPROXY_NEW_PROXY_LIFETIME, tmp );
		free( tmp );
		InsertJobExpr ( buff );
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
	sprintf( buffer, "%s=\"%s\"", ATTR_KILL_SIG, sig_name );
	InsertJobExpr( buffer );
	free( sig_name );

	sig_name = findKillSigName( RmKillSig, ATTR_REMOVE_KILL_SIG );
	if( sig_name ) {
		sprintf( buffer, "%s=\"%s\"", ATTR_REMOVE_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
		sig_name = NULL;
	}

	sig_name = findKillSigName( HoldKillSig, ATTR_HOLD_KILL_SIG );
	if( sig_name ) {
		sprintf( buffer, "%s=\"%s\"", ATTR_HOLD_KILL_SIG, sig_name );
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
	int		force = 0, queue_modifier;

	char* justSeenQueue = NULL;

	JobIwd[0] = '\0';

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
			forcedAttributes.remove( MyString( name ) );
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
			name = expand_macro( name, ProcVars, PROCVARSIZE );
			if (sscanf(name+strlen("queue"), "%d", &queue_modifier) == EOF) {
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
			forcedAttributes.remove( MyString( name ) );
			forcedAttributes.insert( MyString( name ), MyString( value ) );
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

		/* Put the value in the Configuration Table */
		insert( name, value, ProcVars, PROCVARSIZE );

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

	if( ! pval && alt_name ) {
		pval = lookup_macro( alt_name, ProcVars, PROCVARSIZE );
		used_alt = true;
	}

	if( ! pval ) {
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
set_condor_param( const char *name, char *value )
{
	char *tval = strdup( value );

	insert( name, tval, ProcVars, PROCVARSIZE );
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
	char tmp[ BUFSIZ ], *logfile;
	int		rval;

	// First, connect to the schedd if we have not already done so

	if (ActiveQueueConnection != TRUE) 
	{
		connect_to_the_schedd();
	}

	/* queue num jobs */
	for (int i=0; i < num; i++) {

		if (NewExecutable) {
 			if ((ClusterId = NewCluster()) == -1) {
				fprintf(stderr, "\nERROR: Failed to create cluster\n");
				exit(1);
			}
				// We only need to call init_job_ad the second time we see
				// a new Executable, because we call init_job_ad() in main()
			if ( !IsFirstExecutable ) {
				init_job_ad();
			}
			IsFirstExecutable = false;
			ProcId = -1;
			ClusterAdAttrs.clear();
		}

		if ( ClusterId == -1 ) {
			fprintf( stderr, "\nERROR: Used queue command without "
					 "specifying an executable\n" );
			exit(1);
		}

		ProcId = NewProc (ClusterId);

		/*
		**	Insert the current idea of the cluster and
		**	process number into the hash table.
		*/
		GotQueueCommand = 1;
		(void)sprintf(tmp, "%d", ClusterId);
		set_condor_param(Cluster, tmp);
		(void)sprintf(tmp, "%d", ProcId);
		set_condor_param(Process, tmp);

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
//		if ( JobUniverse == CONDOR_UNIVERSE_GLOBUS ) {
			// Find the X509 user proxy
			// First param for it in the submit file. If it's not there,
			// then check the usual locations (as defined by GSI).

		char *proxy_file = condor_param( X509UserProxy );

		if ( proxy_file == NULL ) {
			proxy_file = get_x509_proxy_filename();
		}

		// Issue an error if (no proxy) && (universe=globus,gt2,gt3,nordugrid)
		if ( proxy_file == NULL) {
			char * grid_type = condor_param( Grid_Type, ATTR_JOB_GRID_TYPE );
			if (JobUniverse == CONDOR_UNIVERSE_GLOBUS &&
				(grid_type == NULL ||
					(stricmp (grid_type, "globus") == MATCH) ||
					(stricmp (grid_type, "gt2") == MATCH) ||
					(stricmp (grid_type, "gt3") == MATCH) ||
					(stricmp (grid_type, "nordugrid") == MATCH))) {
				fprintf( stderr, "\nERROR: can't determine proxy filename\n" );
				fprintf( stderr, "x509 user proxy is required for globus, gt2, gt3 or nordugrid jobs\n");
				exit (1);
			}
		}
				

		if (proxy_file != NULL) {
#ifndef WIN32
			if ( check_x509_proxy(proxy_file) != 0 ) {
				fprintf( stderr, "\nERROR: %s\n", x509_error_string() );
				exit( 1 );
			}

			/* Insert the proxy subject name into the ad */
			char *proxy_subject = x509_proxy_subject_name(proxy_file);
			if ( !proxy_subject ) {
				fprintf( stderr, "\nERROR: %s\n", x509_error_string() );
				exit( 1 );
			}
			/* Dreadful hack: replace all the spaces in the cert subject
			* with underscores.... why?  because we need to pass this
			* as a command line argument to the gridmanager, and until
			* daemoncore handles command-line args w/ an argv array, spaces
			* will cause trouble.  
			*/
			char *space_tmp;
			do {
				if ( (space_tmp = strchr(proxy_subject,' ')) ) {
					*space_tmp = '_';
				}
			} while (space_tmp);
			(void) sprintf(buffer, "%s=\"%s\"", ATTR_X509_USER_PROXY_SUBJECT, 
						proxy_subject);
			InsertJobExpr(buffer);	
			free( proxy_subject );
#endif

					
			(void) sprintf(buffer, "%s=\"%s\"", ATTR_X509_USER_PROXY, 
						full_path(proxy_file));
			InsertJobExpr(buffer);	
			free( proxy_file );
		}
	

		/* For MPI only... we have to define $(NODE) to some string
		here so that we don't break the param parser.  In the
		MPI shadow, we'll convert the string into an integer
		corresponding to the mpi node's number. */
		if ( JobUniverse == CONDOR_UNIVERSE_MPI ) {
			set_condor_param ( "NODE", "#MpInOdE#" );
		} else if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL ) {
			set_condor_param ( "NODE", "#pArAlLeLnOdE#" );
		}

		// Note: Due to the unchecked use of global variables everywhere in
		// condor_submit, the order that we call the below Set* functions
		// is very important!
		SetJobStatus();
		SetPriority();
		SetEnvironment();
		SetNotification();
		SetNotifyUser();
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
		SetTransferFiles();	 // must be called _before_ SetImageSize()
		SetImageSize();		// must be called _after_ SetTransferFiles()
		SetRequirements();	// must be called _after_ SetTransferFiles()
		SetJobLease();		// must be called _after_ SetStdFile(0,1,2)
		SetForcedAttributes();
		SetPeriodicHoldCheck();
		SetPeriodicRemoveCheck();
		SetExitHoldCheck();
		SetExitRemoveCheck();
		SetLeaveInQueue();
			//SetArguments needs to be last for Globus universe args
		SetArguments();
		SetGlobusParams();
		SetMatchListLen();
		SetDAGNodeName();
		SetDAGManJobId();
		SetJarFiles();
		SetParallelStartupScripts(); //JDB

		rval = SaveClassAd();

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
	char* ptr;
	int len = strlen( attr_name );
	for( ptr = (char*)buffer; *ptr; ptr++ ) {
		if( strincmp(attr_name,ptr,len) == MATCH ) {
			return true;
		}
	}
	return false;
}


char *
check_requirements( char *orig )
{
	bool	checks_opsys = false;
	bool	checks_arch = false;
	bool	checks_disk = false;
	bool	checks_mem = false;
	bool	checks_fsdomain = false;
	bool	checks_ckpt_arch = false;
	bool	checks_file_transfer = false;
	bool	checks_pvm = false;
	bool	checks_mpi = false;
	char	*ptr, *tmp;
	static char	answer[4096];
	MyString ft_clause;

	if( strlen(orig) ) {
		(void) sprintf( answer, "(%s)", orig );
	} else {
		answer[0] = '\0';
	}

	switch( JobUniverse ) {
	case CONDOR_UNIVERSE_VANILLA:
		ptr = param( "APPEND_REQ_VANILLA" );
		break;
	case CONDOR_UNIVERSE_STANDARD:
		ptr = param( "APPEND_REQ_STANDARD" );
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
		if( answer[0] ) {
				// We've already got something in requirements, so we
				// need to append an AND clause.
			(void) strcat( answer, " && (" );
		} else {
				// This is the first thing in requirements, so just
				// put this as the first clause.
			(void) strcat( answer, "(" );
		}
		(void) strcat( answer, ptr );
		(void) strcat( answer, ")" );
		free( ptr );
	}

	if ( JobUniverse == CONDOR_UNIVERSE_GLOBUS ) {
		// We don't want any defaults at all w/ Globus...
		// If we don't have a req yet, set to TRUE
		if ( answer[0] == '\0' ) {
			sprintf(answer,"TRUE");
		}
		return answer;
	}

	checks_arch = findClause( answer, ATTR_ARCH );
	checks_opsys = findClause( answer, ATTR_OPSYS );
	checks_disk =  findClause( answer, ATTR_DISK );

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
			break;
		}
	}

		// because of the special-case nature of "Memory" and
		// "VirtualMemory", we have to do this one manually...
	for( ptr = answer; *ptr; ptr++ ) {
		if( strincmp(ATTR_MEMORY,ptr,5) == MATCH ) {
				// We found "Memory", but we need to make sure that's
				// not part of "VirtualMemory"...
			if( ptr == answer ) {
					// We're at the beginning, must be Memory, since
					// there's nothing before it.
				checks_mem = true;
				break;
			}
				// Otherwise, it's safe to go back one position:
			tmp = ptr - 1;
			if( *tmp == 'l' || *tmp == 'L' ) {
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
			strcat( answer, " && (" );
		} else {
			(void)strcat( answer, "(" );
		}
		(void)strcat( answer, ATTR_HAS_JAVA );
		(void)strcat( answer, ")" );
	} else {
		if( !checks_arch ) {
			if( answer[0] ) {
				(void)strcat( answer, " && (Arch == \"" );
			} else {
				(void)strcpy( answer, "(Arch == \"" );
			}
			(void)strcat( answer, Architecture );
			(void)strcat( answer, "\")" );
		}

		if( !checks_opsys ) {
			(void)strcat( answer, " && (OpSys == \"" );
			(void)strcat( answer, OperatingSystem );
			(void)strcat( answer, "\")" );
		}
	}

	if ( JobUniverse == CONDOR_UNIVERSE_STANDARD && !checks_ckpt_arch ) {
		(void)strcat( answer, " && ((CkptArch == Arch) ||" );
		(void)strcat( answer, " (CkptArch =?= UNDEFINED))" );
		(void)strcat( answer, " && ((CkptOpSys == OpSys) ||" );
		(void)strcat( answer, "(CkptOpSys =?= UNDEFINED))" );
	}

	if( !checks_disk ) {
		(void)strcat( answer, " && (Disk >= DiskUsage)" );
	}

	if ( !checks_mem ) {
		(void)strcat( answer, " && ( (Memory * 1024) >= ImageSize )" );
	}

	if ( JobUniverse == CONDOR_UNIVERSE_PVM ) {
		ptr = param("PVM_OLD_PVMD");
		if (ptr) {
			if (ptr[0] == 'T' || ptr[0] == 't') {
				(void)strcat( answer, " && (Machine != \"" );
				(void)strcat( answer, my_full_hostname() );
					// XXX Temporary hack: we only want to run on the
					// first node of an SMP machine for pvm jobs.
				(void)strcat( answer,
							  "\" && ((VirtualMachineID =?= UNDEFINED ) "
							  "|| (VirtualMachineID =?= 1)) )" );
			}
			free(ptr);
		}
		if( ! checks_pvm ) {
			(void)strcat( answer, "&& (" );
			(void)strcat( answer, ATTR_HAS_PVM );
			(void)strcat( answer, ")" );
		}
	} 

	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		if( ! checks_mpi ) {
			(void)strcat( answer, "&& (" );
			(void)strcat( answer, ATTR_HAS_MPI );
			(void)strcat( answer, ")" );
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
				(void)strcat( answer, "&& (TARGET." );
				(void)strcat( answer, ATTR_FILE_SYSTEM_DOMAIN );
				(void)strcat( answer, " == MY." );
				(void)strcat( answer, ATTR_FILE_SYSTEM_DOMAIN );
				(void)strcat( answer, ")" );
			} 
			break;
			
		case STF_YES:
				// we're definitely going to use file transfer.  
			if( ! checks_file_transfer ) {
				(void)strcat( answer, "&& (");
				(void)strcat( answer, ATTR_HAS_FILE_TRANSFER );
				(void)strcat( answer, ")");
			}
			break;
			
		case STF_IF_NEEDED:
				// we may or may not use file transfer, so require
				// either the same FS domain OR the ability to file
				// transfer.  if the user already refered to fs
				// domain, but explictly turned on IF_NEEDED, assume
				// they know what they're doing. 
			if( ! checks_fsdomain ) {
				ft_clause = "&& (";
				ft_clause += ATTR_HAS_FILE_TRANSFER;
				ft_clause += " || (TARGET.";
				ft_clause += ATTR_FILE_SYSTEM_DOMAIN;
				ft_clause += " == MY.";
				ft_clause += ATTR_FILE_SYSTEM_DOMAIN;
				ft_clause += "))";
				strcat( answer, ft_clause.Value() );
			}
			break;
		}
	}
	return answer;
}



char *
full_path(const char *name, bool use_iwd)
{
	static char	pathname[_POSIX_PATH_MAX];
	pathname[0] = '\0';
	char *p_iwd;
	char realcwd[_POSIX_PATH_MAX];

	if ( use_iwd ) {
		ASSERT(JobIwd[0]);
		p_iwd = JobIwd;
	} else {
		(void)getcwd(realcwd,sizeof(realcwd));
		p_iwd = realcwd;
	}

#if defined(WIN32)
	if ( name[0] == '\\' || name[0] == '/' || (name[0] && name[1] == ':') ) {
		strcpy(pathname, name);
	} else {
		(void)sprintf( pathname, "%s\\%s", p_iwd, name );
	}
#else
	int root_len, iwd_len, name_len, real_len;

	root_len = strlen(JobRootdir);
	iwd_len = strlen(p_iwd);
	name_len = strlen(name);
	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		if(root_len + name_len >= _POSIX_PATH_MAX) {
		fprintf(stderr, "\nERROR: Value for \"%s/%s\" is too long:\n"
                                "\tPosix limits path names to %d bytes\n",
                                JobRootdir, name, _POSIX_PATH_MAX);
                DoCleanup(0,0,NULL);
                exit( 1 );
	
		}
		real_len=sprintf( pathname, "%s%s", JobRootdir, name );
	} else {	/* relative to iwd which is relative to the root */
		if(root_len + iwd_len + name_len + 2 >= _POSIX_PATH_MAX) {
		fprintf(stderr, "\nERROR: Value for \"%s/%s\%s\" is too long:\n"
                                "\tPosix limits path names to %d bytes\n",
                                JobRootdir, p_iwd,  name, _POSIX_PATH_MAX);
                DoCleanup(0,0,NULL);
                exit( 1 );
	
		}
		real_len=sprintf( pathname, "%s/%s/%s", JobRootdir, p_iwd, name );
	}
#endif

	compress( pathname );

	return pathname;
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

	temp = condor_param( AppendFiles );
	if(temp) {
		list = new StringList(temp);
		if(list->contains_withwildcard(name)) {
			flags = flags & ~O_TRUNC;
		}
		delete list;
	}

	if( (fd=open(strPathname.Value(),flags,0664)) < 0 ) {
		fprintf( stderr, "\nERROR: Can't open \"%s\"  with flags 0%o\n",
				 strPathname.Value(), flags );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	(void)close( fd );

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
	fprintf( stderr, "	-v\t\tverbose output\n" );
	fprintf( stderr, "	-n schedd_name\tsubmit to the specified schedd\n" );
	fprintf( stderr,
			 "	-r schedd_name\tsubmit to the specified remote schedd\n" );
	fprintf( stderr,
			 "	-a line       \tadd line to submit file before processing\n"
			 "                \t(overrides submit file; multiple -a lines ok)\n" );
	fprintf( stderr, "	-d\t\tdisable file permission checks\n\n" );
	fprintf( stderr, "	-s\t\tspool all files to the schedd\n\n" );
	fprintf( stderr, "	-p password\tspecify password to MyProxy server\n\n" );
	fprintf( stderr, "	If [cmdfile] is omitted, input is read from stdin\n" );
	exit( 1 );
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

#if 0
	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		fprintf(stderr,"SPOOL not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
#endif

	Flavor = param( "FLAVOR" );
	if( Flavor == NULL ) {
		Flavor = strdup("none");
	}

	My_fs_domain = param( "FILESYSTEM_DOMAIN" );
		// Will always return something, since config() will put in a
		// value (full hostname) if it's not in the config file.  
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
compress( char *str )
{
	char	*src, *dst;

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
	 UserLog usr_log;
	 SubmitEvent jobSubmit;

	 usr_log.setUseXML(UseXMLInLog);

	if( Quiet ) {
		fprintf(stdout, "Logging submit event(s)");
	}

	strcpy (jobSubmit.submitHost, MySchedd->addr());

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

			usr_log.initialize(owner, ntdomain, simple_name, 0, 0, 0);
			// Output the information
			for (int j=SubmitInfo[i].firstjob; j<=SubmitInfo[i].lastjob; j++) {
				usr_log.initialize(SubmitInfo[i].cluster, j, 0);
				if( ! usr_log.writeEvent(&jobSubmit) ) {
					fprintf(stderr, "\nERROR: Failed to log submit event.\n");
				}
				if( Quiet ) {
					fprintf(stdout, ".");
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
//	ExprTree *tree = NULL, *lhs = NULL, *rhs = NULL;
	char *lhstr, *rhstr;
	int  retval = 0;
	int myprocid = ProcId;
	ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	std::string rhstring;
	static ClassAd* current_cluster_ad = NULL;

	if ( ProcId > 0 ) {
		SetAttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId);
	} else {
		myprocid = -1;		// means this is a cluster ad
		SetAttributeInt (ClusterId, myprocid, ATTR_CLUSTER_ID, ClusterId);
	}

	

//	job->ResetExpr();
//	while( (tree = job->NextExpr()) ) {
	ClassAd::iterator adIter;
	for( adIter = job->begin( ); adIter != job->end( ); adIter++ ) { 
		lhstr = NULL;
		rhstr = NULL;
//		if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
//		if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
//		if( !lhs || !rhs || !lhstr || !rhstr) { retval = -1; }
		lhstr = new char[adIter->first.length( )+1];
		strcpy( lhstr, adIter->first.c_str( ) );
		rhstring = "";
		unp.Unparse( rhstring, adIter->second );
		rhstr = new char[rhstring.length( )+1];
		strcpy( rhstr, rhstring.c_str( ) );
		if( !lhstr || !rhstr ) { retval = -1; }
		if( SetAttribute(ClusterId, myprocid, lhstr, rhstr) == -1 ) {
			fprintf( stderr, "\nERROR: Failed to set %s=%s for job %d.%d\n", 
					 lhstr, rhstr, ClusterId, ProcId );
			retval = -1;
		}
		free(lhstr);
		free(rhstr);
	}

	if ( ProcId == 0 ) {
		SetAttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId);
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
InsertJobExpr (const char *expr, bool clustercheck)
{
	ExprTree *tree = NULL;//, *lhs = NULL;
	char      name[128];
	name[0] = '\0';
	int unused = 0;

	MyString hashkey(expr);
	ClassAdParser parser;
	ClassAd ad;
	std::string adString;

	if ( clustercheck && ProcId > 0 ) {
		// We are inserting proc 1 or above.  So before we actually stick
		// this into the job ad, make certain we did not already place it
		// into the cluster ad.  We do this via a hashtable lookup.

		if ( ClusterAdAttrs.lookup(hashkey,unused) == 0 ) {
			// found it.  so it is already in the cluster ad; we're done.
			return;
		}
	}

//	int retval = Parse (expr, tree);

//	if (retval)
	adString = "[" + std::string( expr ) + "]";
	if( !parser.ParseClassAd( adString , ad ) )
	{
		fprintf (stderr, "\nERROR: Parse error in expression: \n\t%s\n\t", expr);
//		while (retval--) {
//			fputc( ' ', stderr );
//		}
//		fprintf (stderr, "^^^\n");
		fprintf(stderr,"Error in submit file\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

// This is now redundant.  If the "expression" is not an assignment it will
// fail to parse as a ClassAd above. - NAC

//  	if( (lhs = tree->LArg()) ) {
//  		lhs->PrintToStr (name);
//  	} else {
//  		fprintf (stderr, "\nERROR: Expression not assignment: %s\n", expr);
//  		fprintf(stderr,"Error in submit file\n");
//  		DoCleanup(0,0,NULL);
//  		exit( 1 );
//  	}
	
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

static int 
hashFunction (const MyString &str, int numBuckets)
{
	 int i = str.Length() - 1, hashVal = 0;
	 while (i >= 0) 
	 {
		  hashVal += tolower(str[i]);
		  i--;
	 }
	 return (hashVal % numBuckets);
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
		sprintf( buffer, "RENDEZVOUS_DIRECTORY=%s", Rendezvous );
			//putenv because Authentication::authenticate() expects them there.
		putenv( strdup( buffer ) );
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
		return true;
		break;
	default:
		return false;
		break;
	}
	return false;
}



/************************************
	The following are dummy stubs for the DaemonCore class to allow
	tools using DCSchedd to link.  DaemonCore is brought in
	because of the FileTransfer object.
	These stub functions will become obsoluete once we start linking
	in the real DaemonCore library with the tools, -or- once
	FileTransfer is broken down.
*************************************/
#include "../condor_daemon_core.V6/condor_daemon_core.h"
	DaemonCore* daemonCore = NULL;
	int DaemonCore::Kill_Thread(int) { return 0; }
//char * DaemonCore::InfoCommandSinfulString(int) { return NULL; }
	int DaemonCore::Register_Command(int,char*,CommandHandler,char*,Service*,
		DCpermission,int) {return 0;}
	int DaemonCore::Register_Reaper(char*,ReaperHandler,
		char*,Service*) {return 0;}
	int DaemonCore::Create_Thread(ThreadStartFunc,void*,Stream*,
		int) {return 0;}
	int DaemonCore::Suspend_Thread(int) {return 0;}
	int DaemonCore::Continue_Thread(int) {return 0;}
//	int DaemonCore::Register_Reaper(int,char*,ReaperHandler,ReaperHandlercpp,
//		char*,Service*,int) {return 0;}
//	int DaemonCore::Register_Reaper(char*,ReaperHandlercpp,
//		char*,Service*) {return 0;}
//	int DaemonCore::Register_Command(int,char*,CommandHandlercpp,char*,Service*,
//		DCpermission,int) {return 0;}
/**************************************/


