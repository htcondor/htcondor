/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
 

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_string.h"
#include "condor_ckpt_name.h"
#include "environ.h"
#include "basename.h"
#include <time.h>
#include "user_log.c++.h"
#include "url_condor.h"
#include "sched.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_parser.h"
#include "condor_scanner.h"
#include "files.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif
#include "my_hostname.h"
#include "get_daemon_addr.h"
#include "condor_qmgr.h"
#include "sig_install.h"
#include "access.h"

#include "extArray.h"
#include "HashTable.h"
#include "MyString.h"
#include "string_list.h"

#include "my_username.h"

#include "globus_utils.h"

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
char	*Spool;
char	*Flavor;
char	*ScheddName = NULL;
char	*ScheddAddr = NULL;
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
bool never_transfer = false;  // never transfer files or do transfer files
// environment vars in the ClassAd attribute are seperated via
// the env_delimiter character; currently a '|' on NT and ';' on Unix
#ifdef WIN32
	char env_delimiter[] = "|";
#else
	char env_delimiter[] = ";";
#endif

char* LogNotesVal = NULL;

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
char	*MachineCount	= "machine_count";
char	*NotifyUser		= "notify_user";
char	*ExitRequirements = "exit_requirements";
char	*UserLogFile	= "log";
char	*CoreSize		= "coresize";
char	*NiceUser		= "nice_user";

char	*X509UserProxy	= "x509userproxy";
char	*GlobusScheduler = "globusscheduler";
char	*GlobusRSL = "globusrsl";
char	*RendezvousDir	= "rendezvousdir";

char	*FileRemaps = "file_remaps";
char	*BufferFiles = "buffer_files";
char	*BufferSize = "buffer_size";
char	*BufferBlockSize = "buffer_block_size";

char	*FetchFiles = "fetch_files";
char	*CompressFiles = "compress_files";
char	*AppendFiles = "append_files";

char	*TransferInputFiles = "transfer_input_files";
char	*TransferOutputFiles = "transfer_output_files";
char	*TransferFiles = "transfer_files";

char	*CopyToSpool = "copy_to_spool";

char	*PeriodicHoldCheck = "periodic_hold";
char	*PeriodicRemoveCheck = "periodic_remove";
char	*OnExitHoldCheck = "on_exit_hold";
char	*OnExitRemoveCheck = "on_exit_remove";

char	*DAGNodeName = "dag_node_name";
char	*DAGManJobId = "dagman_job_id";

char	*LogNotes = "submit_event_notes";

#if !defined(WIN32)
char	*KillSig			= "kill_sig";
char	*RmKillSig			= "remove_kill_sig";
#endif

void 	reschedule();
void 	SetExecutable();
void 	SetUniverse();
void	SetMachineCount();
void 	SetImageSize();
int 	calc_image_size( char *name);
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
void	SetRank();
void 	SetIWD();
void 	ComputeIWD();
void	SetUserLog();
void	SetCoreSize();
void	SetFileOptions();
#if !defined(WIN32)
void	SetKillSig();
#endif
void	SetForcedAttributes();
void 	check_iwd( char *iwd );
int	read_condor_file( FILE *fp );
char * 	condor_param( const char *name );
void 	set_condor_param( char *name, char *value );
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
void	InsertJobExpr (char *expr, bool clustercheck = true);
void	check_umask();
void setupAuthentication();
void	SetPeriodicHoldCheck(void);
void	SetPeriodicRemoveCheck(void);
void	SetExitHoldCheck(void);
void	SetExitRemoveCheck(void);
void SetDAGNodeName();
void SetDAGManJobId();
void SetLogNotes();

char *owner = NULL;

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
};

ExtArray <SubmitRec> SubmitInfo(10);
int CurrentSubmitInfo = -1;

// explicit template instantiations
template class HashTable<MyString, MyString>;
template class HashBucket<MyString,MyString>;
template class ExtArray<SubmitRec>;

void TestFilePermissions( char *scheddAddr = NULL )
{
#ifdef WIN32
	// TODO: need to port to Win32
	gid_t gid = 99999;
	uid_t uid = 99999;
#else
	gid_t gid = getgid();
	uid_t uid = getuid();
#endif
	int result, crap;
	MyString name;

	CheckFilesRead.startIterations();
	while( ( CheckFilesRead.iterate( name, crap ) ) )
	{
		result = attempt_access((char *)name.Value(), ACCESS_READ, uid, gid, scheddAddr);
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not readable by condor.\n", 
					name.Value());
		}
	}

	CheckFilesWrite.startIterations();
	while( ( CheckFilesWrite.iterate( name, crap ) ) )
	{
		result = attempt_access((char *)name.Value(), ACCESS_WRITE, uid, gid, scheddAddr );
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not writeable by condor.\n", 
					name.Value());
		}
	}
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
	char *ntdomain = strnewp(get_condor_username());
	if (ntdomain) {
		char *slash = strchr(ntdomain,'/');
		if ( slash ) {
			*slash = '\0';
			if ( strlen(ntdomain) > 0 ) {
				if ( strlen(ntdomain) > 80 ) {
					fprintf(stderr,"NT DOMAIN OVERFLOW (%s)\n",ntdomain);
					exit(1);
				}
				(void) sprintf (buffer, "%s = \"%s\"", ATTR_NT_DOMAIN, 
										ntdomain);
				InsertJobExpr (buffer);
			}
		}
		delete [] ntdomain;
	}
#endif
		
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
		owner = "unknown";
	}

	MyName = basename(argv[0]);
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
				DisableFileChecks = 1;
				break;
			case 'r':
				Remote++;
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
			default:
				usage();
			}
		} else {
			cmd_file = *ptr;
		}
	}
	

	if( !(ScheddAddr = get_schedd_addr(ScheddName)) ) {
		if( ScheddName ) {
			fprintf( stderr, "\nERROR: Can't find address of schedd %s\n", ScheddName );
		} else {
			fprintf( stderr, "\nERROR: Can't find address of local schedd\n" );
		}
		exit(1);
	}

	// We must strdup ScheddAddr, cuz we got it from
	// get_schedd_addr which uses a (gasp!) _static_ buffer.
	ScheddAddr = strdup(ScheddAddr);

	
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

	if(ProcId != -1 ) 
	{
		reschedule();
	}

	if ( !DisableFileChecks ) {
		TestFilePermissions( ScheddAddr );
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
	int			cmd;

	/* Connect to the schedd */
	ReliSock sock;
	if(!sock.connect(ScheddAddr)) {
		fprintf(stderr, "Can't connect to condor scheduler (%s)\n",
				ScheddAddr );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	sock.encode();
	cmd = RESCHEDULE;
	if( !sock.code(cmd) || !sock.eom() ) {
		fprintf(stderr, "Can't send RESCHEDULE command to condor scheduler\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}


void
check_path_length(const char *path, char *lhs)
{

#ifdef WIN32
	/* On Win32, also make certain this path is on the local system.
	 * Remove this code once we support networked filesystems.
	 */

	char volume[8];
	volume[0] = '\0';
	if ( path[0] && (path[1]==':') ) {
		sprintf(volume,"%c:\\",path[0]);
	}

	if ( (strncmp(path,"\\\\",2) == MATCH) ||
		 (strncmp(path,"//",2) == MATCH) ||
		 (strincmp(path,"\\UNC\\",5) == MATCH) ||
		 (strincmp(path,"/UNC/",5) == MATCH) ||
		 (volume[0] && (GetDriveType(volume)==DRIVE_REMOTE))
		 ) 
	{
		fprintf(stderr, "\nERROR: \"%s\" is a network drive/path.\n",path);
		fprintf(stderr,
				"\tThis version of Condor is a *preview* edition, and does\n"
				"\tnot support file operations on network drives.  All files\n"
				"\tto be accessed by this preview version of Condor must reside\n"
				"\ton your local hard disk (not on a network volume). The \n"
				"\tcompleted release of Condor NT will support network\n"
				"\tdrives.  Please check the Condor Website at \n"
				"\t\thttp://www.cs.wisc.edu/condor\n"
				"\tto see if an updated version of Condor NT is available.\n"
				);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
#endif

	if (strlen(path) > _POSIX_PATH_MAX) {
		fprintf(stderr, "\nERROR: Value for \"%s\" is too long:\n"
				"\tPosix limits path names to %d bytes\n",
				lhs, _POSIX_PATH_MAX);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}


void
SetExecutable()
{
	char	*ename = NULL;
	char	*copySpool = NULL;

	ename = condor_param(Executable);

	if( ename == NULL ) {
		fprintf( stderr, "No '%s' parameter was provided\n", Executable);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	check_path_length(full_path(ename,false), Executable);

	(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_CMD, full_path(ename,false));
	InsertJobExpr (buffer);

		/* MPI REALLY doesn't like these! */
	if ( JobUniverse != MPI && JobUniverse != PVM ) {
		InsertJobExpr ("MinHosts = 1");
		InsertJobExpr ("MaxHosts = 1");
	}

	InsertJobExpr ("CurrentHosts = 0");

	switch(JobUniverse) 
	{
	case STANDARD:
		(void) sprintf (buffer, "%s = TRUE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		(void) sprintf (buffer, "%s = TRUE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	case PVM:
	case VANILLA:
	case SCHED_UNIVERSE:
	case MPI:  // for now
	case GLOBUS_UNIVERSE:
		(void) sprintf (buffer, "%s = FALSE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		(void) sprintf (buffer, "%s = FALSE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	default:
		fprintf(stderr, "Unknown universe (%d)\n", JobUniverse );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	copySpool = condor_param(CopyToSpool);
	if( copySpool == NULL)
	{
		copySpool = (char *)malloc(16);
		strcpy(copySpool, "TRUE");
	}

	// generate initial checkpoint file
	strcpy( IckptName, gen_ckpt_name(0,ClusterId,ICKPT,0) );

	// spool executable only if no $$(arch).$$(opsys) specified

	if ( !strstr(ename,"$$") && *copySpool != 'F' && *copySpool != 'f' ) {	

		if (SendSpoolFile(IckptName) < 0) {
			fprintf(stderr,"permission to transfer executable %s denied\n",IckptName);
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		if (SendSpoolFileBytes(full_path(ename,false)) < 0) {
			fprintf(stderr,"failed to transfer executable file %s\n", ename);
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

	univ = condor_param(Universe);

#if !defined(WIN32)
	if( univ && stricmp(univ,"pvm") == MATCH ) 
	{
		char *pvmd = param("PVMD");

		if (!pvmd || access(pvmd, R_OK|X_OK) != 0) {
			fprintf(stderr, "Error: Condor PVM support is not installed.\n"
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

		JobUniverse = PVM;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, PVM);
		InsertJobExpr (buffer);
		InsertJobExpr ("Checkpoint = 0");

		free(univ);
		return;
	};

	if( univ && stricmp(univ,"mpi") == MATCH ) {
		JobUniverse = MPI;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, MPI);
		InsertJobExpr (buffer);
		InsertJobExpr ("Checkpoint = 0");
		
		free(univ);
		return;
	}

	if( univ && stricmp(univ,"scheduler") == MATCH ) {
		JobUniverse = SCHED_UNIVERSE;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, SCHED_UNIVERSE);
		InsertJobExpr (buffer);
		free(univ);
		return;
	};

	if( univ && stricmp(univ,"globus") == MATCH ) {
		if ( have_globus_support() == 0 ) {
			fprintf( stderr, "This version of Condor doesn't support Globus Universe jobs.\n" );
			exit( 1 );
		}
		JobUniverse = GLOBUS_UNIVERSE;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, GLOBUS_UNIVERSE);
		InsertJobExpr (buffer);
		free(univ);
		return;
	};
#endif  // of !defined(WIN32)

	if( univ && stricmp(univ,"vanilla") == MATCH ) {
		JobUniverse = VANILLA;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, VANILLA);
		InsertJobExpr (buffer);
		free(univ);
		return;
	};

	if (!univ) {
		univ = strdup("standard");
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

	JobUniverse = STANDARD;
	(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, STANDARD);
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

	if (JobUniverse == PVM) {

		mach_count = condor_param(MachineCount);

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

	} else if (JobUniverse == MPI) {

		mach_count = condor_param( MachineCount );
		
		int tmp;
		if ( mach_count != NULL ) {
			tmp = atoi(mach_count);
			free(mach_count);
		}
		else {
			fprintf(stderr, "No machine_count specified!\n" );
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

	tmp = condor_param(ImageSize);

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
			fprintf(stderr, "Image Size must be positive\n" );
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

	tmp = condor_param(FileRemaps);
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

	tmp = condor_param(BufferSize);
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

	tmp = condor_param(BufferBlockSize);
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
calc_image_size( char *name)
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
// SetTransferFiles also sets a global "never_transfer", which is 
// used by SetRequirements().  So, SetTransferFiles must be called _before_
// calling SetRequirements() as well.
void
SetTransferFiles()
{
	char *macro_value;
	int count;
	char *tmp;
	bool files_specified = false;
	char	 buffer[ATTRLIST_MAX_EXPRESSION];
	char	 input_files[ATTRLIST_MAX_EXPRESSION];
	char	 output_files[ATTRLIST_MAX_EXPRESSION];

	never_transfer = false;

	buffer[0] = input_files[0] = output_files[0] = '\0';

	macro_value = condor_param( TransferInputFiles ) ;
	TransferInputSize = 0;
	if( macro_value ) 
	{
		StringList files(macro_value,",");
		files.rewind();
		count = 0;
		while ( (tmp=files.next()) ) {
			count++;
			check_path_length(tmp,TransferInputFiles);
			check_open(tmp, O_RDONLY);
			TransferInputSize += calc_image_size(tmp);
		}
		if ( count ) {
			(void) sprintf (input_files, "%s = \"%s\"", 
				ATTR_TRANSFER_INPUT_FILES, macro_value);
			files_specified = true;
		}
		free(macro_value);
	}


	macro_value = condor_param( TransferOutputFiles ) ;
	if( macro_value ) 
	{
		StringList files(macro_value,",");
		files.rewind();
		count = 0;
		while ( (tmp=files.next()) ) {
			count++;
			check_path_length(tmp,TransferOutputFiles);
			check_open(tmp, O_WRONLY|O_CREAT|O_TRUNC );
		}
		if ( count ) {
			(void) sprintf (output_files, "%s = \"%s\"", 
				ATTR_TRANSFER_OUTPUT_FILES, macro_value);
			files_specified = true;
		}
		free(macro_value);
	}

	macro_value = condor_param( TransferFiles ) ;
	if( macro_value ) 
	{
		// User explicitly specified TransferFiles; do what user says
		switch ( macro_value[0] ) {
				// Handle "Never"
			case 'n':
			case 'N':
				// Handle "Never"
				sprintf(buffer,"%s = \"%s\"",ATTR_TRANSFER_FILES,"NEVER");
				never_transfer = true;
				break;
			case 'o':
			case 'O':
				// Handle "OnExit"
				sprintf(buffer,"%s = \"%s\"",ATTR_TRANSFER_FILES,"ONEXIT");
				break;
			case 'a':
			case 'A':
				// Handle "Always"
				sprintf(buffer,"%s = \"%s\"",ATTR_TRANSFER_FILES,"ALWAYS");
				break;
			default:
				// Unrecognized
				fprintf(stderr,"Unrecognized argument for parameter '%s'\n", 
						TransferFiles);
				DoCleanup(0,0,NULL);
				exit( 1 );
				break;
		}	// end of switch

		free(macro_value);		// condor_param() calls malloc; free it!
	} else {
		// User did not explicitly specify TransferFiles; choose a default
		if ( files_specified ) {
			sprintf(buffer,"%s = \"%s\"",ATTR_TRANSFER_FILES,"ONEXIT");
		} else {
#ifdef WIN32
			sprintf(buffer,"%s = \"%s\"",ATTR_TRANSFER_FILES,"ONEXIT");
#else
			sprintf(buffer,"%s = \"%s\"",ATTR_TRANSFER_FILES,"NEVER");
			never_transfer = true;
#endif
		}
	}

	// Insert what we want for ATTR_TRANSFER_FILES
	InsertJobExpr (buffer);

	// if we did not set ATTR_TRANSFER_FILES to NEVER, 
	// insert in input/output exprs
	if ( !never_transfer ) {
		if ( input_files[0] ) {
			InsertJobExpr (input_files);
		}
		if ( output_files[0] ) {
			InsertJobExpr (output_files);
		}
	}
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
SetStdFile( int which_file )
{
	char	*macro_value = NULL;
	char	*generic_name;
	char	 buffer[_POSIX_PATH_MAX + 32];

	switch( which_file ) 
	{
	  case 0:
		generic_name = Input;
		break;
	  case 1:
		generic_name = Output;
		break;
	  case 2:
		generic_name = Error;
		break;
	  default:
		fprintf(stderr, "Unknown standard file descriptor (%d)\n", which_file );
		DoCleanup(0,0,NULL);
	}


	macro_value = condor_param( generic_name );
#if !defined(WIN32)
		//if no files in Globus job, don't set anything
	if ( !macro_value && ( JobUniverse == GLOBUS_UNIVERSE ) ) {
		return;
	}
#endif
	
	if( !macro_value || *macro_value == '\0') 
	{
		macro_value = strdup(NULL_FILE);
	}
	
	if( whitespace(macro_value) ) 
	{
		fprintf(stderr,"The '%s' takes exactly one argument (%s)\n", 
				generic_name, macro_value);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}	

	check_path_length(macro_value, generic_name);

	switch( which_file ) 
	{
	  case 0:
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_INPUT, macro_value);
		InsertJobExpr (buffer);
		check_open( macro_value, O_RDONLY );
		break;
	  case 1:
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_OUTPUT, macro_value);
		InsertJobExpr (buffer);
		check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
		break;
	  case 2:
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ERROR, macro_value);
		InsertJobExpr (buffer);
		check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
		break;
	}
		
	if ( macro_value )
		free(macro_value);
}

void
SetJobStatus()
{
	char *hold = condor_param(Hold);

	if( hold && (hold[0] == 'T' || hold[0] == 't') ) {
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer);
	} else {
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_STATUS, IDLE);
		InsertJobExpr (buffer);
	}

	if( hold ) free(hold);
}

void
SetPriority()
{
	char *prio = condor_param(Priority);
	int  prioval = 0;

	if( prio != NULL ) 
	{
		prioval = atoi (prio);
		if( prioval < -20 || prioval > 20 ) 
		{
			fprintf(stderr,"Priority must be in the range -20 thru 20 (%d)\n", 
					prioval );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		free(prio);
	}
	(void) sprintf (buffer, "%s = %d", ATTR_JOB_PRIO, prioval);
	InsertJobExpr (buffer);

	// also check if the job is "dirt user" priority (i.e., nice_user==True)
	char *nice_user = condor_param(NiceUser);
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
	char *phc = condor_param(PeriodicHoldCheck);

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
}

void
SetPeriodicRemoveCheck(void)
{
	char *prc = condor_param(PeriodicRemoveCheck);

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
	char *ehc = condor_param(OnExitHoldCheck);

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
SetExitRemoveCheck(void)
{
	char *erc = condor_param(OnExitRemoveCheck);

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
	char *how = condor_param(Notification);
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
		fprintf(stderr,"Notification must be 'Never', 'Always', 'Complete', "
		"or 'Error'\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	(void) sprintf (buffer, "%s = %d", ATTR_JOB_NOTIFICATION, notification);
	InsertJobExpr (buffer);

	if ( how )
		free(how);
}

void
SetNotifyUser()
{
	char *who = condor_param(NotifyUser);

	if( ! who ) {
			// If "notify_user" isn't there, try "NotifyUser" 
		who = condor_param( "NotifyUser" );
	}

	if (who) {
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_NOTIFY_USER, who);
		InsertJobExpr (buffer);
		free(who);
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
	LogNotesVal = condor_param( LogNotes );
	if( !LogNotesVal ) {
		LogNotesVal = condor_param( "SubmitEventNotes" );
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
	char *who = condor_param(ExitRequirements);

	if( ! who ) {
			// If "exit_requirements" isn't there, try "ExitRequirements" 
		who = condor_param( "ExitRequirements" );
	}

	if (who) {
		(void) sprintf (buffer, "%s = %s", ATTR_JOB_EXIT_REQUIREMENTS, who);
		InsertJobExpr (buffer);
		free(who);
	}
}
	
void
SetArguments()
{
	char	*args = NULL;

	args = condor_param(Arguments);

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

	free(args);
}

void
SetEnvironment()
{
	char *env = condor_param(Environment);
	char *shouldgetenv = condor_param(GetEnv);
	char *allowscripts = condor_param(AllowStartupScript);
	Environ envobject;
	char newenv[ATTRLIST_MAX_EXPRESSION];
	char varname[MAXVARNAME];
	int envlen;
	bool first = true;


	sprintf(newenv, "%s = \"", ATTR_JOB_ENVIRONMENT);

	if (env) {
		strcat(newenv, env);
		envobject.add_string(env);
		first = false;
	}

	if (allowscripts && (*allowscripts=='T' || *allowscripts=='t') ) {
		if ( !first ) {
			strcat(newenv,env_delimiter);
		}
		strcat(newenv,"_CONDOR_NOCHECK=1");
		first = false;
		free(allowscripts);
	}


	envlen = strlen(newenv);

	// grab user's environment if getenv == TRUE
	if ( shouldgetenv && ( shouldgetenv[0] == 'T' || shouldgetenv[0] == 't' ) )
 	{

		for (int i=0; environ[i] && envlen < ATTRLIST_MAX_EXPRESSION; i++) {

			// ignore env settings that contain env_delimiter to avoid 
			// syntax problems

			if (strchr(environ[i], env_delimiter[0]) == NULL) {
				envlen += strlen(environ[i]);
				if (envlen < ATTRLIST_MAX_EXPRESSION) {

					// don't override submit file environment settings
					// check if environment variable is set in submit file

					int j;
					for (j=0; env && environ[i][j] && environ[i][j] != '='; j++) {
						varname[j] = environ[i][j];
					}
					varname[j] = '\0';
					if (env == NULL || envobject.getenv(varname) == NULL) {
						if (first) {
							first = false;
						} else {
							strcat(newenv, env_delimiter);
						}
						strcat(newenv, environ[i]);
					}

				}
			}
		}
	}

	strcat(newenv, "\"");

	InsertJobExpr (newenv);
	if ( env )
		free(env);
	if ( shouldgetenv ) 
		free(shouldgetenv);
}

#if !defined(WIN32)
void
ComputeRootDir()
{
	char *rootdir = condor_param(RootDir);

	if( rootdir == NULL ) 
	{
		(void) strcpy (JobRootdir, "/");
	} 
	else 
	{
		if( access(rootdir, F_OK|X_OK) < 0 ) {
			fprintf(stderr,"No such directory: %s\n", rootdir);
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		check_path_length(rootdir, RootDir);
		(void) strcpy (JobRootdir, rootdir);
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
	char *requirements = condor_param(Requirements);
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
}

void
SetRank()
{
	static char rank[ATTRLIST_MAX_EXPRESSION];
	char *orig_pref = condor_param(Preferences);
	char *orig_rank = condor_param(Rank);
	char *default_rank = NULL;
	char *append_rank = NULL;
	rank[0] = '\0';

	switch( JobUniverse ) {
	case STANDARD:
		default_rank = param("DEFAULT_RANK_STANDARD");
		append_rank = param("APPEND_RANK_STANDARD");
		break;
	case VANILLA:
		default_rank = param("DEFAULT_RANK_VANILLA");
		append_rank = param("APPEND_RANK_VANILLA");
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
		fprintf(stderr,"\nERROR: %s and %s may not both be specified for a job\n",
			   Preferences, Rank);
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
		(void)sprintf( buffer, "%s = 0", ATTR_RANK );
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

	shortname = condor_param( InitialDir );

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
	check_path_length(iwd, InitialDir);
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
		fprintf(stderr, "No such directory: %s\n", pathname);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
}

void
SetUserLog()
{
	char *ulog_entry = condor_param(UserLogFile);

	if (ulog_entry) {
		if (whitespace(ulog_entry)) {
			fprintf(stderr,"Only one %s can be specified.\n", UserLogFile);
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
		char *ulog = full_path(ulog_entry,false);
		free(ulog_entry);
		check_path_length(ulog, UserLogFile);
		(void) sprintf(buffer, "%s = \"%s\"", ATTR_ULOG_FILE, ulog);
		InsertJobExpr(buffer);
		UserLogSpecified = true;
	}
}

#if defined(ALPHA)
	char	*CoreSizeFmt = "CONDOR_CORESIZE=%ld";
#else
	char	*CoreSizeFmt = "CONDOR_CORESIZE=%d";
#endif


void
SetCoreSize()
{
	char *size = condor_param(CoreSize);
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
			fprintf( stderr, "\nWarning:  Unable to expand macros in \"%s\"."
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

	if ( JobUniverse != GLOBUS_UNIVERSE )
		return;

	if ( !(globushost = condor_param( GlobusScheduler ) ) ) {
		fprintf(stderr, "Globus universe jobs require a \"%s\" parameter\n",
				GlobusScheduler );
		DoCleanup( 0, 0, NULL );
		exit( 1 );
	}

	sprintf( buffer, "%s = \"%s\"", ATTR_GLOBUS_RESOURCE, globushost );
	InsertJobExpr (buffer, false );

	free( globushost );

	sprintf( buffer, "%s = \"%s\"", ATTR_GLOBUS_CONTACT_STRING, "X" );
	InsertJobExpr (buffer, false );

	sprintf( buffer, "%s = %d", ATTR_GLOBUS_STATUS, G_UNSUBMITTED );
	InsertJobExpr (buffer, false );

	if ( tmp = condor_param(GlobusRSL) ) {
		sprintf( buff, "%s = \"%s\"", ATTR_GLOBUS_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buff, false );
	}
}

#if !defined(WIN32)
struct SigTable { int v; char *n; };

static struct SigTable SigNameArray[] = {
	{ SIGABRT, "SIGABRT" },
	{ SIGALRM, "SIGALRM" },
	{ SIGFPE, "SIGFPE" },
	{ SIGHUP, "SIGHUP" },
	{ SIGILL, "SIGILL" },
	{ SIGINT, "SIGINT" },
	{ SIGKILL, "SIGKILL" },
	{ SIGPIPE, "SIGPIPE" },
	{ SIGQUIT, "SIGQUIT" },
	{ SIGSEGV, "SIGSEGV" },
	{ SIGTERM, "SIGTERM" },
	{ SIGUSR1, "SIGUSR1" },
	{ SIGUSR2, "SIGUSR2" },
	{ SIGCHLD, "SIGCHLD" },
	{ SIGTSTP, "SIGTSTP" },
	{ SIGTTIN, "SIGTTIN" },
	{ SIGTTOU, "SIGTTOU" },
	{ -1, 0 }
};

int
sig_name_lookup(char sig[])
{
	for (int i = 0; SigNameArray[i].n != 0; i++) {
		if (stricmp(SigNameArray[i].n, sig) == 0) {
			return SigNameArray[i].v;
		}
	}
	fprintf( stderr, "\nERROR: unknown signal %s\n", sig );
	exit(1);
	return -1;
}

void
SetKillSig()
{
	char *sig = condor_param(KillSig);
	int signo;

	if (sig) {
		signo = atoi(sig);
		if (signo == 0 && isalnum(sig[0])) {
			signo = sig_name_lookup(sig);
		}
		if( signo == 0 ) {
			fprintf( stderr, "\nERROR: invalid signal %s\n", sig );
			exit( 1 );
		}
		free(sig);
	} else {
		switch(JobUniverse) {
		case STANDARD:
			signo = SIGTSTP;
			break;
		default:
			signo = SIGTERM;
			break;
		}
	}

	(void) sprintf (buffer, "%s = %d", ATTR_KILL_SIG, signo);
	InsertJobExpr(buffer);

	sig = condor_param(RmKillSig);

	if (sig) {
		signo = atoi(sig);
		if (signo == 0 && isalnum(sig[0])) {
			signo = sig_name_lookup(sig);
		}
		if( signo == 0 ) {
			fprintf( stderr, "\nERROR: invalid signal %s\n", sig );
			exit( 1 );
		}
		free(sig);
		(void) sprintf (buffer, "%s = %d", ATTR_REMOVE_KILL_SIG, signo);
		InsertJobExpr(buffer);
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
		if( *name == '#' || blankline(name) )
			continue;
		
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
				fprintf(stderr, 
					"\nERROR: Failed to expand macros in: %s\n", name);
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
				fprintf(stderr, "\nERROR: Failed to expand macros in: %s\n", name);
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

		lower_case( name );

		if ( strcmp(name, Executable) == 0 )
		{
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
condor_param( const char *name )
{
	char *pval = lookup_macro(name, ProcVars, PROCVARSIZE);

	if( pval == NULL ) {
		return( NULL );
	}

	pval = expand_macro(pval, ProcVars, PROCVARSIZE);

	if (pval == NULL) {
		fprintf(stderr, "\nERROR: Failed to expand macros in: %s\n", name);
		exit(1);
	}

	return( pval );
}

void
set_condor_param( char *name, char *value )
{
	char *tval = strdup( value );

	insert( name, tval, ProcVars, PROCVARSIZE );
}


int
strcmpnull(const char *str1, const char *str2)
{
	if (str1 && str2) return strcmp(str1, str2);
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

	if (ConnectQ(ScheddAddr) == 0) {
		if( ScheddName ) {
			fprintf( stderr, 
					"\nERROR: Failed to connect to queue manager %s\n",
					 ScheddName );
		} else {
			fprintf( stderr, 
				"\nERROR: Failed to connect to local queue manager\n" );
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
			fprintf(stderr,
			"\nERROR: Used queue command without specifying an executable\n");
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
		if ( JobUniverse == GLOBUS_UNIVERSE ) {
			char *proxy_file = condor_param( X509UserProxy );
			char *rm_contact = condor_param( GlobusScheduler );
			if ( check_x509_proxy(proxy_file) != 0 ) {
				exit( 1 );
			}
/*
			if ( rm_contact && (check_globus_rm_contacts(rm_contact) != 0) ) {
				fprintf( stderr, "\nERROR: Can't find scheduler in MDS\n" );
				exit( 1 );
			}
*/
			if ( proxy_file )
				free( proxy_file );
			if ( rm_contact )
				free( rm_contact );
		}

			/* For MPI only... we have to define $(NODE) to some string
			   here so that we don't break the param parser.  In the 
			   MPI shadow, we'll convert the string into an integer 
			   corresponding to the mpi node's number. */
		if ( JobUniverse == MPI ) {
			set_condor_param ( "NODE", "#MpInOdE#" );
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
		SetTransferFiles();	 // must be called _before_ SetImageSize() 
		SetImageSize();		// must be called _after_ SetTransferFiles()
		SetRequirements();	// must be called _after_ SetTransferFiles()
		SetForcedAttributes();
		SetPeriodicHoldCheck();
		SetPeriodicRemoveCheck();
		SetExitHoldCheck();
		SetExitRemoveCheck();
			//SetArguments needs to be last for Globus universe args
		SetArguments(); 
		SetGlobusParams();
		SetDAGNodeName();
		SetDAGManJobId();

		rval = SaveClassAd();

		SetLogNotes();

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

		logfile = condor_param(UserLogFile);
		// Convert to a pathname using IWD if needed
		if ( logfile ) {
			logfile = full_path(logfile);
		}

		if (CurrentSubmitInfo == -1 ||
			SubmitInfo[CurrentSubmitInfo].cluster != ClusterId ||
			strcmpnull( SubmitInfo[CurrentSubmitInfo].logfile,
						logfile ) != 0 ||
			strcmpnull( SubmitInfo[CurrentSubmitInfo].lognotes,
						LogNotesVal ) != 0 ) {
			CurrentSubmitInfo++;
			SubmitInfo[CurrentSubmitInfo].cluster = ClusterId;
			SubmitInfo[CurrentSubmitInfo].firstjob = ProcId;
			if (logfile) {
				// Store the full pathname to the log file
				SubmitInfo[CurrentSubmitInfo].logfile = strdup(logfile);
			} else {
				SubmitInfo[CurrentSubmitInfo].logfile = NULL;
			}
			if( LogNotesVal ) {
				SubmitInfo[CurrentSubmitInfo].lognotes = strdup( LogNotesVal );
			}
			else {
				SubmitInfo[CurrentSubmitInfo].lognotes = NULL;
			}
		}
		SubmitInfo[CurrentSubmitInfo].lastjob = ProcId;

		delete job;
		job = new ClassAd();

		if (Quiet) {
			fprintf(stdout, ".");
		}

	}
}

char *
check_requirements( char *orig )
{
	int		has_opsys = FALSE;
	int		has_arch = FALSE;
	int		has_disk = FALSE;
	int		has_mem = FALSE;
	int		has_fsdomain = FALSE;
	int		has_ckpt_arch = FALSE;
	char	*ptr, *tmp;
	static char	answer[4096];

	if( strlen(orig) ) {
		(void) sprintf( answer, "(%s)", orig );
	} else {
		answer[0] = '\0';
	}

	switch( JobUniverse ) {
	case VANILLA:
		ptr = param( "APPEND_REQ_VANILLA" );
		break;
	case STANDARD:
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

				
	for( ptr = answer; *ptr; ptr++ ) {
		if( strincmp(ATTR_ARCH,ptr,4) == MATCH ) {
			has_arch = TRUE;
			break;
		}
	}

	for( ptr = answer; *ptr; ptr++ ) {
		if( strincmp(ATTR_OPSYS,ptr,5) == MATCH ) {
			has_opsys = TRUE;
			break;
		}
	}
 
	for( ptr = answer; *ptr; ptr++ ) {
		if( strincmp(ATTR_DISK,ptr,5) == MATCH ) {
			has_disk = TRUE;
			break;
		}
	}
 
	for( ptr = answer; *ptr; ptr++ ) {
		if( strincmp(ATTR_MEMORY,ptr,5) == MATCH ) {
				// We found "Memory", but we need to make sure that's
				// not part of "VirtualMemory"...
			if( ptr == answer ) {
					// We're at the beginning, must be Memory, since
					// there's nothing before it.
				has_mem = TRUE;
				break;
			}
				// Otherwise, it's safe to go back one position:
			tmp = ptr - 1;
			if( *tmp == 'l' || *tmp == 'L' ) {
					// Must be VirtualMemory, keep searching...
				continue;
			}
				// If it wasn't an 'l', we must have found it...
			has_mem = TRUE;
			break;
		}
	}
 
	for( ptr = answer; *ptr; ptr++ ) {
		if( strincmp(ATTR_CKPT_ARCH,ptr,4) == MATCH ) {
			has_ckpt_arch = TRUE;
			break;
		}
	}

	if( !has_arch ) {
		if( answer[0] ) {
			(void)strcat( answer, " && (Arch == \"" );
		} else {
			(void)strcpy( answer, "(Arch == \"" );
		}
		(void)strcat( answer, Architecture );
		(void)strcat( answer, "\")" );
	}

	if( !has_opsys ) {
		(void)strcat( answer, " && (OpSys == \"" );
		(void)strcat( answer, OperatingSystem );
		(void)strcat( answer, "\")" );
	}

	if ( JobUniverse == STANDARD && !has_ckpt_arch ) {
		(void)strcat( answer, " && ((CkptArch == Arch) ||" );
		(void)strcat( answer, " (CkptArch =?= UNDEFINED))" );
		(void)strcat( answer, " && ((CkptOpSys == OpSys) ||" );
		(void)strcat( answer, "(CkptOpSys =?= UNDEFINED))" );
	}

	if( !has_disk ) {
		(void)strcat( answer, " && (Disk >= DiskUsage)" );
	}

	if ( !has_mem ) {
		(void)strcat( answer, " && ( (Memory * 1024) >= ImageSize )" );
	}

	if ( JobUniverse == PVM ) {
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
	} 

	if ( JobUniverse == VANILLA ) {
		for( ptr = answer; *ptr; ptr++ ) {
			if( strincmp("FileSystemDo",ptr,12) == MATCH ) {
				has_fsdomain = TRUE;
				break;
			}
		}
		if ( !has_fsdomain && never_transfer) {
			(void)strcat( answer, " && (FileSystemDomain == \"" );
			(void)strcat( answer, My_fs_domain );
			(void)strcat( answer, "\")" );
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
	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		(void)sprintf( pathname, "%s%s", JobRootdir, name );
	} else {	/* relative to iwd which is relative to the root */
		(void)sprintf( pathname, "%s/%s/%s", JobRootdir, p_iwd, name );
	}
#endif

	compress( pathname );

	return pathname;
}

void
check_open( const char *name, int flags )
{
	int		fd;
	char	*pathname, *temp;
	StringList *list;

	/* No need to check for existence of the Null file. */
	if (strcmp(name, NULL_FILE) == MATCH) return;

	pathname = full_path(name);

		/* This is only for MPI.  We test for our string that
		   we replaced "$(NODE)" with, and replace it with "0".  Thus, 
		   we will really only try and access the 0th file only */
	if ( JobUniverse == MPI ) {
		if ( (temp = strstr( pathname, "#MpInOdE#" ) ) != NULL ) {
			*(temp++) = '0';
			*temp = '\0';
			memmove ( temp, temp+8, strlen(temp+8) );
		}
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

	if( (fd=open(pathname,flags,0664)) < 0 ) {
		fprintf(stderr, "\nCan't open \"%s\"  with flags 0%o\n", pathname, flags );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	(void)close( fd );

	// Queue files for testing access if not already queued
	MyString pathname_key(pathname);
	int crap;
	if( flags & O_WRONLY )
	{
		if ( CheckFilesWrite.lookup(pathname_key,crap) < 0 ) {
			// this file not found in our list; add it
			CheckFilesWrite.insert(pathname_key,crap);
		}
	}
	else
	{
		if ( CheckFilesRead.lookup(pathname_key,crap) < 0 ) {
			// this file not found in our list; add it
			CheckFilesRead.insert(pathname_key,crap);
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
			 "	-a line\tadd line to submit file before processing\n"
			 "         \t(overrides lines in the submit file;"
			 "         \t multiple -a arguments are supported)" );
	fprintf( stderr, "	-d\t\tdisable file permission checks\n\n" );
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
			ActiveQueueConnection = (ConnectQ(ScheddAddr) != 0);
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

	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		fprintf(stderr,"SPOOL not specified in config file\n" );
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

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

	if (Quiet) fprintf(stdout, "Logging submit event(s)");

	strcpy (jobSubmit.submitHost, ScheddAddr);

	if( LogNotesVal ) {
		jobSubmit.submitEventLogNotes = strnewp( LogNotesVal );
		free( LogNotesVal );
	}

	for (int i=0; i <= CurrentSubmitInfo; i++) {

		if ((simple_name = SubmitInfo[i].logfile) != NULL) {
			if( jobSubmit.submitEventLogNotes ) {
				delete[] jobSubmit.submitEventLogNotes;
			}
			jobSubmit.submitEventLogNotes = strnewp( SubmitInfo[i].lognotes );

			usr_log.initialize(owner, simple_name, 0, 0, 0);

			// Output the information
			for (int j=SubmitInfo[i].firstjob; j<=SubmitInfo[i].lastjob; j++) {
				usr_log.initialize(SubmitInfo[i].cluster, j, 0);
				if (!usr_log.writeEvent (&jobSubmit))
					fprintf(stderr, "\nERROR: Failed to log submit event.\n");
				if (Quiet) fprintf(stdout, ".");
			}
		}
	}

	if (Quiet) fprintf(stdout, "\n");
}


int
SaveClassAd ()
{
	ExprTree *tree = NULL, *lhs = NULL, *rhs = NULL;
	char lhstr[128], rhstr[ATTRLIST_MAX_EXPRESSION];
	int  retval = 0;
	int myprocid = ProcId;

	if ( ProcId > 0 ) {
		SetAttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId);
	} else {
		myprocid = -1;
		SetAttributeInt (ClusterId, myprocid, ATTR_CLUSTER_ID, ClusterId);
	}

	

	job->ResetExpr();
	while (tree = job->NextExpr())
	{
		lhstr[0] = '\0';
		rhstr[0] = '\0';
		if (lhs = tree->LArg()) lhs->PrintToStr (lhstr);
		if (rhs = tree->RArg()) rhs->PrintToStr (rhstr);
		if (!lhs || !rhs) retval = -1;
		if (SetAttribute (ClusterId, myprocid, lhstr, rhstr) == -1) {
			fprintf(stderr, "\nERROR: Failed to set %s=%s for job %d.%d\n", 
				lhstr, rhstr, ClusterId, ProcId);
			retval = -1;
		}
	}

	if ( ProcId == 0 ) {
		SetAttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId);
	}

	return retval;
}


void 
InsertJobExpr (char *expr, bool clustercheck)
{
	ExprTree *tree = NULL, *lhs = NULL;
	char      name[128];
	name[0] = '\0';
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

	int retval = Parse (expr, tree);

	if (retval)
	{
		fprintf (stderr, "\nERROR: Parse error in expression: \n\t%s\n\t", expr);
		while (retval--) {
			fputc( ' ', stderr );
		}
		fprintf (stderr, "^^^\n");
		fprintf(stderr,"Error in submit file\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if (lhs = tree->LArg()) 
		lhs->PrintToStr (name);
	else
	{
		fprintf (stderr, "\nERROR: Expression not assignment: %s\n", expr);
		fprintf(stderr,"Error in submit file\n");
		DoCleanup(0,0,NULL);
		exit( 1 );
	}
	
	if (!job->InsertOrUpdate (expr))
	{	
		fprintf(stderr,"Unable to insert expression: %s\n", expr);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	if ( clustercheck && ProcId < 1 ) {
		// We are working on building the ad which will serve as our
		// cluster ad.  Thus insert this expr into our hashtable.
		if ( ClusterAdAttrs.insert(hashkey,unused) < 0 ) {
			fprintf(stderr,"Unable to insert expression into hashtable: %s\n", expr);
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
	if ( Rendezvous = condor_param( RendezvousDir ) )
	{
		dprintf( D_FULLDEBUG,"setting RENDEZVOUS_DIRECTORY=%s\n", Rendezvous );
		sprintf( buffer, "RENDEZVOUS_DIRECTORY=%s", Rendezvous );
			//putenv because Authentication::authenticate() expects them there.
		putenv( strdup( buffer ) );
		free( Rendezvous );
	}

}
