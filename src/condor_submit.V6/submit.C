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

#include "my_username.h"

static int hashFunction( const MyString&, int );
HashTable<MyString,MyString> forcedAttributes( 64, hashFunction ); 

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)	 */

char* mySubSystem = "SUBMIT";	/* Used for SUBMIT_EXPRS */

ClassAd job;
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
int		GotQueueCommand;

char	IckptName[_POSIX_PATH_MAX];	/* Pathname of spooled initial ckpt file */

char	*MyName;
int		Quiet = 1;
int	  ClusterId = -1;
int	  ProcId = -1;
int	  JobUniverse;
int		Remote=0;
int		ClusterCreated = FALSE;
int		ActiveQueueConnection = FALSE;
bool	NewExecutable = false;
bool	UserLogSpecified = false;

#define PROCVARSIZE	32
BUCKET *ProcVars[ PROCVARSIZE ];

#define MEG	(1<<20)

/*
**	Names of possible CONDOR variables.
*/
char	*Cluster 		= "cluster";
char	*Process			= "process";
char	*Priority		= "priority";
char	*Notification	= "notification";
char	*Executable		= "executable";
char	*Arguments		= "arguments";
char	*GetEnv			= "getenv";
char	*Environment	= "environment";
char	*Input			= "input";
char	*Output			= "output";
char	*Error			= "error";
#if !defined(WIN32)
char	*RootDir		= "rootdir";
#endif
char	*InitialDir		= "initialdir";
char	*Requirements	= "requirements";
char	*Preferences	= "preferences";
char	*Rank				= "rank";
char	*ImageSize		= "image_size";
char	*Universe		= "universe";
char	*MachineCount	= "machine_count";
char	*NotifyUser		= "notify_user";
char	*UserLogFile	= "log";
char	*CoreSize		= "coresize";
char	*NiceUser		= "nice_user";
char	*X509CertDir	= "x509certdir";
#if !defined(WIN32)
char	*KillSig			= "kill_sig";
#endif
#if defined(WIN32)
char	*NullFile		= "NUL";
#else
char	*NullFile		= "/dev/null";
#endif

void 	reschedule();
void 	SetExecutable();
void 	SetUniverse();
void 	SetImageSize();
int 	calc_image_size( char *name );
int 	find_cmd( char *name );
char *	get_tok();
void 	SetStdFile( int which_file );
void 	SetPriority();
void 	SetNotification();
void 	SetNotifyUser ();
void 	SetArguments();
void 	SetEnvironment();
#if !defined(WIN32)
void 	SetRootDir();
#endif
void 	SetRequirements();
void	SetRank();
void 	SetIWD();
void	SetUserLog();
void	SetCoreSize();
#if !defined(WIN32)
void	SetKillSig();
#endif
void	SetForcedAttributes();
void 	check_iwd( char *iwd );
int	read_condor_file( FILE *fp, int stopBeforeQueuing=0 );
char * 	condor_param( char *name );
void 	set_condor_param( char *name, char *value );
void 	queue(int num);
char * 	check_requirements( char *orig );
void 	check_open( char *name, int flags );
void 	usage();
char * 	get_owner();
void 	init_params();
int 	whitespace( char *str);
void 	delete_commas( char *ptr );
void 	compress( char *str );
void 	magic_check();
void 	log_submit();
void 	get_time_conv( int &hours, int &minutes );
int	  SaveClassAd (ClassAd &);
void	InsertJobExpr (char *expr);
void	check_umask();

char *owner = NULL;

extern char **environ;

extern "C" {
int SetSyscalls( int foo );
int DoCleanup();
}

struct SubmitRec {
	int cluster;
	int firstjob;
	int lastjob;
	char *logfile;
};

ExtArray <SubmitRec> SubmitInfo(10);
int CurrentSubmitInfo = -1;

// list of files to check for read and write access.
ExtArray <char *> CheckFilesRead(4);
ExtArray <char *> CheckFilesWrite(4);
int NumCheckFilesRead = 0;
int NumCheckFilesWrite = 0;

// explicit template instantiations
template class HashTable<MyString, MyString>;
template class HashBucket<MyString,MyString>;
template class ExtArray<SubmitRec>;

void TestFilePermissions()
{
#ifdef WIN32
	// TODO: need to port to Win32
	gid_t gid = 99999;
	uid_t uid = 99999;
#else
	gid_t gid = getgid();
	uid_t uid = getuid();
#endif
	int result, i;

	for(i = 0; i < NumCheckFilesRead; ++i)
	{
		result = attempt_access(CheckFilesRead[i], ACCESS_READ, uid, gid);
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not readable by condor.\n", 
					CheckFilesRead[i]);
		}
		free (CheckFilesRead[i]);
	}

	for(i = 0; i < NumCheckFilesWrite; ++i)
	{
		result = attempt_access(CheckFilesWrite[i], ACCESS_WRITE, uid, gid);
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not writeable by condor.\n", 
					CheckFilesWrite[i]);
		}
		free (CheckFilesWrite[i]);
	}
}

int
main( int argc, char *argv[] )
{
	FILE	*fp;
	char	**ptr;
	char	*cmd_file = NULL;
	int dag_pause = 0;
	char	*scheddname;

	setbuf( stdout, NULL );

#if !defined(WIN32)	
		// Make sure root isn't trying to submit.
	if( getuid() == 0 || getgid() == 0 ) {
		fprintf( stderr, "ERROR: Submitting jobs as user/group 0 (root) is not "
				 "allowed for security reasons.\n" );
		exit( 1 );
	}
#endif /* not WIN32 */

	MyName = argv[0];
	config( 0 );
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

	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] == '-' ) {
			switch( ptr[0][1] ) {
			case 'v': 
				Quiet = 0;
				break;
			case 'p':
					// the -p option will cause condor_submit to pause for about
					// 4 seconds upon completion.  this prevents 'Broken Pipe'
					// messages when condor_submit is called from DagMan on
					// platforms with crappy popen(), like IRIX - Todd T, 2/97
					dag_pause = 1;
				break;
			case 'r':
				Remote++;
				ptr++;
				if( ! *ptr ) {
					fprintf( stderr, "%s: -r requires another argument\n", 
							 MyName );
					exit(1);
				}					
				if( !(scheddname = get_daemon_name(*ptr)) ) {
					fprintf( stderr, "%s: unknown host %s\n", 
							 MyName, get_host_part(*ptr) );
					exit(1);
				}
				ScheddName = strdup(scheddname);
				break;
			case 'n':
				ptr++;
				if( ! *ptr ) {
					fprintf( stderr, "%s: -n requires another argument\n", 
							 MyName );
					exit(1);
				}					
				if( !(scheddname = get_daemon_name(*ptr)) ) {
					fprintf( stderr, "%s: unknown host %s\n", 
							 MyName, get_host_part(*ptr) );
					exit(1);
				}
				ScheddName = strdup(scheddname);
				break;
			default:
				usage();
			}
		} else {
			cmd_file = *ptr;
		}
	}
	
	// set up types of the ad
	job.SetMyTypeName (JOB_ADTYPE);
	job.SetTargetTypeName (STARTD_ADTYPE);

	if( cmd_file == NULL ) {
		usage();
	}

	if( !(ScheddAddr = get_schedd_addr(ScheddName)) ) {
		if( ScheddName ) {
			fprintf( stderr, "ERROR: Can't find address of schedd %s\n", ScheddName );
		} else {
			fprintf( stderr, "ERROR: Can't find address of local schedd\n" );
		}
		exit(1);
	}

	// We must strdup ScheddAddr, cuz we got it from
	// get_schedd_addr which uses a (gasp!) _static_ buffer.
	ScheddAddr = strdup(ScheddAddr);

	
	// open submit file
	if( (fp=fopen(cmd_file,"r")) == NULL ) {
		fprintf( stderr, "ERROR: Failed to open command file\n");
		exit(1);
	}

	// Need X509_CERT_DIR set before connectQ if in submit file

	//  Parse the file, stopping at "queue" command
	if( read_condor_file( fp, 1 ) < 0 ) {
		fprintf(stderr, "ERROR: Failed to parse command file.\n");
		exit(1);
	}

	//if defined in file, set up to use x509certdir
	char *CertDir;
	if ( CertDir = condor_param( X509CertDir ) ) {
		dprintf( D_FULLDEBUG, "setting env X509_CERT_DIR=%s from submit file\n",
				CertDir );
		char tmpstring[1024];
		sprintf( tmpstring, "X509_CERT_DIR=%s/", CertDir );
		putenv( strdup( tmpstring ) );
		free( CertDir );
	}


	if (ConnectQ(ScheddAddr) == 0) {
		if( ScheddName ) {
			fprintf( stderr, "ERROR: Failed to connect to queue manager %s\n",
					 ScheddName );
		} else {
			fprintf( stderr, "ERROR: Failed to connect to local queue manager\n" );
		}
		exit(1);
	}

	ActiveQueueConnection = TRUE;

	// in case things go awry ...
	_EXCEPT_Cleanup = DoCleanup;

	(void) sprintf (buffer, "%s = %d", ATTR_Q_DATE, (int)time ((time_t *) 0));
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_COMPLETION_DATE);
	InsertJobExpr (buffer);

//	(void) sprintf (buffer, "%s = \"%s\"", ATTR_OWNER, get_owner());
//this should go away, and the owner name be placed in ad by schedd!!!!!
	char* tmp = my_username();
	if( tmp ) {
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_OWNER, tmp);
		free( tmp );
	} else {
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_OWNER, "unknown" );
	}
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

	config_fill_ad( &job );

	if (Quiet) {
		fprintf(stdout, "Submitting job(s)");
	}

		//NOTE: this is actually the SECOND time file gets (at least partially)
		//read. setupAuthentication needs to read it before call to ConnectQ
	//  Parse the file and queue the jobs 
	if( read_condor_file(fp) < 0 ) {
		fprintf(stderr, "\nERROR: Failed to parse command file.\n");
		exit(1);
	}

	DisconnectQ(0);

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

	if( !GotQueueCommand ) {
		fprintf( stderr, "ERROR: \"%s\" doesn't contain any \"queue\"", cmd_file );
		fprintf( stderr, " commands -- no jobs queued\n" );
		exit( 1 );
	}

	TestFilePermissions();

	if (dag_pause)
		sleep(4);


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
	ReliSock sock(ScheddAddr, SCHED_PORT);
	if(sock.get_file_desc() < 0) {
		EXCEPT( "Can't connect to condor scheduler (%s)\n",
				ScheddAddr );
	}

	sock.encode();
	cmd = RESCHEDULE;
	if( !sock.code(cmd) || !sock.eom() ) {
		EXCEPT( "Can't send RESCHEDULE command to condor scheduler\n" );
	}
}


void
check_path_length(char *path, char *lhs)
{
	if (strlen(path) > _POSIX_PATH_MAX) {
		fprintf(stderr, "\nERROR: Value for \"%s\" is too long:\n"
				"\tPosix limits path names to %d bytes\n",
				lhs, _POSIX_PATH_MAX);
		DoCleanup();
		exit( 1 );
	}
}


void
SetExecutable()
{
  char	*ename;

  ename = condor_param(Executable);

  if( ename == NULL ) {
	EXCEPT("No '%s' parameter was provided", Executable);
  }

  check_path_length(ename, Executable);

  (void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_CMD, ename);
  InsertJobExpr (buffer);

  InsertJobExpr ("MinHosts = 1");
  InsertJobExpr ("MaxHosts = 1");
  InsertJobExpr ("CurrentHosts = 0");

  (void) sprintf (buffer, "%s = %d", ATTR_JOB_STATUS, IDLE);
  InsertJobExpr (buffer);

  SetUniverse();
	
  switch(JobUniverse) 
	{
	case STANDARD:
	  (void) sprintf (buffer, "%s = TRUE", ATTR_WANT_REMOTE_SYSCALLS);
	  InsertJobExpr (buffer);
	  (void) sprintf (buffer, "%s = TRUE", ATTR_WANT_CHECKPOINT);
	  InsertJobExpr (buffer);
	  break;
	case PVM:
	case PIPE:
	  (void) sprintf (buffer, "%s = TRUE", ATTR_WANT_REMOTE_SYSCALLS);
	  InsertJobExpr (buffer);
	  (void) sprintf (buffer, "%s = FALSE", ATTR_WANT_CHECKPOINT);
	  InsertJobExpr (buffer);
	  break;
	case VANILLA:
	case SCHED_UNIVERSE:
	case MPI:  // for now
	  (void) sprintf (buffer, "%s = FALSE", ATTR_WANT_REMOTE_SYSCALLS);
	  InsertJobExpr (buffer);
	  (void) sprintf (buffer, "%s = FALSE", ATTR_WANT_CHECKPOINT);
	  InsertJobExpr (buffer);
	  break;
	default:
	  EXCEPT( "Unknown universe (%d)", JobUniverse );
	}

  // generate initial checkpoint file
  strcpy( IckptName, gen_ckpt_name(0,ClusterId,ICKPT,0) );

	if (SendSpoolFile(IckptName) < 0) {
		EXCEPT("permission to transfer executable %s denied", IckptName);
	}

	if (SendSpoolFileBytes(ename) < 0) {
		EXCEPT("failed to transfer executable file %s", ename);
	}
}

void
SetUniverse()
{
	char	*univ;
	char	*mach_count;
	char	*ptr;

	univ = condor_param(Universe);

	if( univ && stricmp(univ,"pvm") == MATCH ) 
	{
		int tmp;

		JobUniverse = PVM;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, PVM);
		InsertJobExpr (buffer);
		InsertJobExpr ("Checkpoint = 0");

		mach_count = condor_param(MachineCount);

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
		}
		return;
	};

	if( univ && stricmp(univ,"mpi") == MATCH ) {
		JobUniverse = MPI;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, MPI);
		InsertJobExpr (buffer);
		InsertJobExpr ("Checkpoint = 0");
		
		mach_count = condor_param( MachineCount );
		
		int tmp;
		if ( mach_count != NULL ) {
			tmp = atoi(mach_count);
		}
		else {
			EXCEPT( "No machine_count specified!\n" );
		}

		(void) sprintf (buffer, "%s = %d", ATTR_MIN_HOSTS, tmp);
		InsertJobExpr (buffer);
		(void) sprintf (buffer, "%s = %d", ATTR_MAX_HOSTS, tmp);
		InsertJobExpr (buffer);

		return;
	}

	if( univ && stricmp(univ,"pipe") == MATCH ) {
		JobUniverse = PIPE;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, PIPE);
		InsertJobExpr (buffer);
		InsertJobExpr ("Checkpoint = 0");
		return;
	};

	if( univ && stricmp(univ,"vanilla") == MATCH ) {
		JobUniverse = VANILLA;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, VANILLA);
		InsertJobExpr (buffer);
		return;
	};

	if( univ && stricmp(univ,"scheduler") == MATCH ) {
		JobUniverse = SCHED_UNIVERSE;
		(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, SCHED_UNIVERSE);
		InsertJobExpr (buffer);
		return;
	}

	JobUniverse = STANDARD;
	(void) sprintf (buffer, "%s = %d", ATTR_JOB_UNIVERSE, STANDARD);
	InsertJobExpr (buffer);

	return;
}

void
SetImageSize()
{
	int		size;
	int		executablesize;
	char	*tmp;
	char	*p;
	char    buff[2048];

	tmp = condor_param(ImageSize);

	ASSERT (job.LookupString ("Cmd", buff));
	size = executablesize = calc_image_size( buff );

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
			EXCEPT( "Image Size must be positive" );
		}
	}

	/* It's reasonable to expect the image size will be at least the
		physical memory requirement, so make sure it is. */
	  
	// At one time there was some stuff to attempt to gleen this from
	// the requirements line, but that caused many problems.
	// Jeff Ballard 11/4/98

	(void)sprintf (buffer, "%s = %d", ATTR_IMAGE_SIZE, size);
	InsertJobExpr (buffer);

	(void)sprintf (buffer, "%s = %d", ATTR_EXECUTABLE_SIZE, executablesize);
	InsertJobExpr (buffer);
}

/*
** Make a wild guess at the size of the image represented by this a.out.
** Should add up the text, data, and bss sizes, then allow something for
** the stack.  But how we gonna do that if the executable is for some
** other architecture??  Our answer is in kilobytes.
*/
int
calc_image_size( char *name )
{
	struct stat	buf;

	if( stat(name,&buf) < 0 ) {
		EXCEPT( "Cannot stat \"%s\"", name );
	}
	return (buf.st_size + 1023) / 1024;
}

void
SetStdFile( int which_file )
{
	char	*macro_value;
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
		EXCEPT( "Unknown standard file descriptor (%d)\n", which_file );
	}


	macro_value = condor_param( generic_name );
	
	if( !macro_value || *macro_value == '\0') 
	{
		macro_value = NullFile;
	}
	
	if( whitespace(macro_value) ) 
	{
		EXCEPT("The '%s' takes exactly one argument (%s)", generic_name, 
				macro_value);
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
			EXCEPT("Priority must be in the range -20 thru 20 (%d)", prioval );
		}
	}
	(void) sprintf (buffer, "%s = %d", ATTR_JOB_PRIO, prioval);
	InsertJobExpr (buffer);

	// also check if the job is "dirt user" priority (i.e., nice_user==True)
	char *nice_user = condor_param(NiceUser);
	if( nice_user && (*nice_user == 'T' || *nice_user == 't') )
	{
		sprintf( buffer, "%s = TRUE", ATTR_NICE_USER );
		InsertJobExpr( buffer );
	}
	else
	{
		job.Delete( ATTR_NICE_USER );
	}
}

void
SetNotification()
{
	char *how = condor_param(Notification);
	int notification;

	if( (how == NULL) || (stricmp(how, "COMPLETE") == 0) ) {
		notification = NOTIFY_COMPLETE;
	} else if( stricmp(how, "NEVER") == 0 ) {
		notification = NOTIFY_NEVER;
	} else if( stricmp(how, "ALWAYS") == 0 ) {
		notification = NOTIFY_ALWAYS;
	} else if( stricmp(how, "ERROR") == 0 ) {
		notification = NOTIFY_ERROR;
	} else 
	{
	EXCEPT("Notification must be 'Never', 'Always', 'Complete', or 'Error'");
	}

	(void) sprintf (buffer, "%s = %d", ATTR_JOB_NOTIFICATION, notification);
	InsertJobExpr (buffer);
}

void
SetNotifyUser()
{
	char *who = condor_param(NotifyUser);

	if (who)
	{
		(void) sprintf (buffer, "%s = \"%s\"", ATTR_NOTIFY_USER, who);
		InsertJobExpr (buffer);
	}
}
	
void
SetArguments()
{
	char	*args = condor_param(Arguments);

	if( args == NULL ) {
		args = "";
	}

	if (strlen(args) > _POSIX_ARG_MAX) {
		fprintf(stderr, "\nERROR: arguments are too long:\n"
				"\tPosix limits argument lists to %d bytes\n",
				_POSIX_ARG_MAX);
		DoCleanup();
		exit( 1 );
	}

	sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ARGUMENTS, args);
	InsertJobExpr (buffer);
}

void
SetEnvironment()
{
	char *env = condor_param(Environment);
	char *shouldgetenv = condor_param(GetEnv);
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

	envlen = strlen(newenv);

	// grab user's environment if getenv == TRUE
	if (shouldgetenv && (shouldgetenv[0] == 'T' || shouldgetenv[0] == 't')) {

		for (int i=0; environ[i] && envlen < ATTRLIST_MAX_EXPRESSION; i++) {

			// ignore env settings that contain ';' to avoid syntax problems

			if (strchr(environ[i], ';') == NULL) {
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
							strcat(newenv, ";");
						}
						strcat(newenv, environ[i]);
					}

				}
			}
		}
	}

	strcat(newenv, "\"");

	InsertJobExpr (newenv);
}

#if !defined(WIN32)
void
SetRootDir()
{
	char *rootdir = condor_param(RootDir);

	if( rootdir == NULL ) 
	{
		InsertJobExpr ("Rootdir = \"/\"");
		(void) strcpy (JobRootdir, "/");
	} 
	else 
	{
		if( access(rootdir, F_OK|X_OK) < 0 ) {
			EXCEPT("No such directory: %s", rootdir);
		}

		check_path_length(rootdir, RootDir);

		(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ROOT_DIR, rootdir);
		InsertJobExpr (buffer);
		(void) strcpy (JobRootdir, rootdir);
	}
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

	if ( JobUniverse == STANDARD ) {
		default_rank = param("DEFAULT_RANK_STANDARD");
		append_rank = param("APPEND_RANK_STANDARD");
	}
	if ( JobUniverse == VANILLA ) {
		default_rank = param("DEFAULT_RANK_VANILLA");
		append_rank = param("APPEND_RANK_VANILLA");
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
		fprintf(stderr, "\nERROR: %s and %s may not both be specified for a job\n",
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
}

void
SetIWD()
{
	char	*shortname;
	char	iwd[ _POSIX_PATH_MAX ];
	char	cwd[ _POSIX_PATH_MAX ];

	shortname = condor_param( InitialDir );

#if !defined(WIN32)
	if( strcmp(JobRootdir,"/") != MATCH )	{	/* Rootdir specified */
		if( shortname ) {
			(void)strcpy( iwd, shortname );
		} else {
			(void)strcpy( iwd, "/" );
		}
	} else {
#endif
		if( shortname  ) {
#if defined(WIN32)
			// if a drive letter or share is specified, we have a full pathname
			if( shortname[1] == ':' || (shortname[0] == '\\' && shortname[1] == '\\')) {
#else
			if( shortname[0] == '/' ) {
#endif
				(void)strcpy( iwd, shortname );
			} else {
				(void)getcwd( cwd, sizeof cwd );
				(void)sprintf( iwd, "%s%c%s", cwd, DIR_DELIM_CHAR, shortname );
			}
		} else {
			(void)getcwd( iwd, sizeof iwd );
		}
#if !defined(WIN32)
	}
#endif

	compress( iwd );
	check_path_length(iwd, InitialDir);
	check_iwd( iwd );

	(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_IWD, iwd);
	InsertJobExpr (buffer);
	strcpy (JobIwd, iwd);
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
		EXCEPT("No such directory: %s", pathname);
	}
}

void
SetUserLog()
{
	char *ulog = condor_param(UserLogFile);

	if (ulog) {
		if (whitespace(ulog)) {
			EXCEPT("Only one %s can be specified.", UserLogFile);
		}
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
		InsertJobExpr( buffer );

		// free memory allocated by macro expansion module
		free( exValue );
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
}
#endif  // of ifndef WIN32


int
read_condor_file( FILE *fp, int stopBeforeQueuing )
{
	char	*name, *value;
	char	*ptr;
	int		force = 0, queue_modifier;

	LineNo = 0;
	

	for(;;) {
		force = 0;

		name = getline(fp);
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
			job.Delete( name );
			continue;
		}

		if( strincmp(name, "queue", strlen("queue")) == 0 ) {
			//sleazy hack to deal with fact that queue must happen AFTER
			//connection is authenticated, but cert_dir must be read
			//before user or connection authentication -- mju
			if ( stopBeforeQueuing ) {
				rewind( fp );
				return 0;
			}
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

		if (strcmp(name, Executable) == 0) {
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
condor_param( char *name )
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
queue(int num)
{
	char tmp[ BUFSIZ ], *logfile;
	int		rval;

	/* queue num jobs */
	for (int i=0; i < num; i++) {

		if (NewExecutable) {
			NewExecutable = false;
 			if ((ClusterId = NewCluster()) == -1) {
				fprintf(stderr, "\nERROR: Failed to create cluster\n");
				exit(1);
			}
			ProcId = -1;
			SetExecutable();
		}

		if ( ClusterId == -1 ) {
			fprintf(stderr,"\nERROR: Used queue command without specifying an executable\n");
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
		SetRootDir();
#endif
		SetIWD();
		SetPriority();
		SetArguments();
		SetEnvironment();
		SetNotification();
		SetNotifyUser();
		SetUserLog();
		SetCoreSize();
#if !defined(WIN32)
		SetKillSig();
#endif
		SetRequirements();
		SetRank();
		SetStdFile( 0 );
		SetStdFile( 1 );
		SetStdFile( 2 );
		SetImageSize();
		SetForcedAttributes();

		rval = SaveClassAd( job );

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
				job.fPrint (stdout);
			}

		logfile = condor_param(UserLogFile);

		if (CurrentSubmitInfo == -1 ||
			SubmitInfo[CurrentSubmitInfo].cluster != ClusterId ||
			strcmpnull(SubmitInfo[CurrentSubmitInfo].logfile, logfile) != 0) {
			CurrentSubmitInfo++;
			SubmitInfo[CurrentSubmitInfo].cluster = ClusterId;
			SubmitInfo[CurrentSubmitInfo].firstjob = ProcId;
			if (logfile) {
				SubmitInfo[CurrentSubmitInfo].logfile = strdup(logfile);
			} else {
				SubmitInfo[CurrentSubmitInfo].logfile = NULL;
			}
		}
		SubmitInfo[CurrentSubmitInfo].lastjob = ProcId;

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
	int		has_virtmem = FALSE;
	int		has_fsdomain = FALSE;
	char	*ptr;
	static char	answer[2048];

	for( ptr = orig; *ptr; ptr++ ) {
		if( strincmp(ATTR_ARCH,ptr,4) == MATCH ) {
			has_arch = TRUE;
			break;
		}
	}

	for( ptr = orig; *ptr; ptr++ ) {
		if( strincmp(ATTR_OPSYS,ptr,5) == MATCH ) {
			has_opsys = TRUE;
			break;
		}
	}
 
	for( ptr = orig; *ptr; ptr++ ) {
		if( strincmp(ATTR_DISK,ptr,5) == MATCH ) {
			has_disk = TRUE;
			break;
		}
	}
 
	for( ptr = orig; *ptr; ptr++ ) {
		if( strincmp(ATTR_VIRTUAL_MEMORY,ptr,5) == MATCH ) {
			has_virtmem = TRUE;
			break;
		}
	}
 
	if( strlen(orig) ) {
		(void)sprintf( answer, "(%s)", orig );
	} else {
		answer[0] = '\0';
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

	if( !has_opsys && !has_arch ) {
		magic_check();
	}

	if( !has_disk ) {
		(void)strcat( answer, " && (Disk >= ExecutableSize)" );
	}

	if ( !has_virtmem ) {
		(void)strcat( answer, " && (VirtualMemory >= ImageSize)" );
	}

	if ( (JobUniverse == PVM) || (JobUniverse == MPI) ) {
		(void)strcat( answer, " && (Machine != \"" );
		(void)strcat( answer, my_full_hostname() );
		(void)strcat( answer, "\")" );
	} 

	if ( JobUniverse == VANILLA ) {
		ptr = param("APPEND_REQ_VANILLA");
		if ( ptr != NULL ) {
			(void) strcat( answer," && (" );
			(void) strcat( answer, ptr );
			(void) strcat( answer,")" );
		}
		for( ptr = answer; *ptr; ptr++ ) {
			if( strincmp("FileSystemDo",ptr,12) == MATCH ) {
				has_fsdomain = TRUE;
				break;
			}
		}

		if ( !has_fsdomain ) {
			(void)strcat( answer, " && (FileSystemDomain == \"" );
			(void)strcat( answer, My_fs_domain );
			(void)strcat( answer, "\")" );
		} 

	}

	if ( JobUniverse == STANDARD ) {
		ptr = param("APPEND_REQ_STANDARD");
		if ( ptr != NULL ) {
			(void) strcat( answer," && (" );
			(void) strcat( answer, ptr );
			(void) strcat( answer,")" );
		}
	}
				
	return answer;
}

void
check_open( char *name, int flags )
{
	int		fd;
	char	pathname[_POSIX_PATH_MAX];
	pathname[0] = '\0';

	/* No need to check for existence of the Null file. */
	if (strcmp(name, NullFile) == MATCH) return;

#if defined(WIN32)
	strcpy(pathname, name);
#else
	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		(void)sprintf( pathname, "%s%s", JobRootdir, name );
	} else {	/* relative to iwd which is relative to the root */
		(void)sprintf( pathname, "%s/%s/%s", JobRootdir, JobIwd, name );
	}
#endif
	compress( pathname );

	if( (fd=open(pathname,flags,0664)) < 0 ) {
		fprintf(stdout,"\n");
		EXCEPT( "Can't open \"%s\"  with flags 0%o", pathname, flags );
	}
	(void)close( fd );

	// Queue files for testing access.
	if( flags & O_WRONLY )
	{
		CheckFilesWrite[NumCheckFilesWrite++] = strdup(pathname);
	}
	else
	{
		CheckFilesRead[NumCheckFilesRead++] = strdup(pathname);
	}
}


void
usage()
{
	fprintf( stderr, "Usage: %s [options] cmdfile\n", MyName );
	fprintf( stderr, "	Valid options:\n" );
	fprintf( stderr, "	-v\t\tverbose output\n" );
	fprintf( stderr, "	-n schedd_name\tsubmit to the specified schedd\n" );
	fprintf( stderr, 
			 "	-r schedd_name\tsubmit to the specified remote schedd\n" );
	exit( 1 );
}


extern "C" {
int
DoCleanup()
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
			DestroyCluster( ClusterId );
			DisconnectQ(0);
		}
	}
	if( IckptName[0] ) 
	{
		(void)unlink( IckptName );
	}

	return 0;		// For historical reasons...
}
} /* extern "C" */

char *
get_owner()
{
	if ( ( owner && !strcmp( owner, "nobody" ) ) || (Remote && !owner ) ) {
		 return ("nobody");
	}

#if defined(WIN32)
	char username[UNLEN+1];
	unsigned long usernamelen = UNLEN+1;
	if (GetUserName(username, &usernamelen) < 0) {
		EXCEPT( "GetUserName failed.\n" );
	}
	return strdup(username);
#else
	struct passwd	*pwd;
	if( (pwd=getpwuid(getuid())) == NULL ) {
		EXCEPT( "Can't get passwd entry for uid %d\n", getuid() );
	}
	return strdup(pwd->pw_name);
#endif
}

void
init_params()
{
	Architecture = param( "ARCH" );

	if( Architecture == NULL ) {
		EXCEPT( "ARCH not specified in config file" );
	}

	OperatingSystem = param( "OPSYS" );
	if( OperatingSystem == NULL ) {
		EXCEPT( "OPSYS not specified in config file" );
	}

	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		EXCEPT( "SPOOL not specified in config file" );
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
whitespace( char *str)
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
magic_check()
{
	return;
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
	 char	 *path;
	 char	 tmp[ _POSIX_PATH_MAX ];
	char	 owner[64];
	 UserLog usr_log;
	 SubmitEvent jobSubmit;

	if (Quiet) fprintf(stdout, "Logging submit event(s)");

	job.LookupString (ATTR_OWNER, owner);
	strcpy (jobSubmit.submitHost, ScheddAddr);

	for (int i=0; i <= CurrentSubmitInfo; i++) {

		if ((simple_name = SubmitInfo[i].logfile) != NULL) {

			// Convert to a pathname using IWD if needed
			if( simple_name[0] == '/' ) {
				path = simple_name;
			} else {
				sprintf( tmp, "%s/%s", JobIwd, simple_name );
				path = tmp;
			}

			usr_log.initialize(owner, path, 0, 0, 0);

			// Output the information
			for (int j=SubmitInfo[i].firstjob; j<=SubmitInfo[i].lastjob; j++) {
				usr_log.initialize(SubmitInfo[i].cluster, j, 0);
				if (!usr_log.writeEvent (&jobSubmit))
					fprintf (stderr, "\nERROR: Failed to log submit event.\n");
				if (Quiet) fprintf(stdout, ".");
			}
		}
	}

	if (Quiet) fprintf(stdout, "\n");
}


int
SaveClassAd (ClassAd &ad)
{
	ExprTree *tree, *lhs, *rhs;
	char lhstr[128], rhstr[ATTRLIST_MAX_EXPRESSION];
	int  retval = 0;

	SetAttributeInt (ClusterId, ProcId, "ClusterId", ClusterId);
	SetAttributeInt (ClusterId, ProcId, "ProcId", ProcId);

	job.ResetExpr();
	while (tree = job.NextExpr())
	{
		lhstr[0] = '\0';
		rhstr[0] = '\0';
		if (lhs = tree->LArg()) lhs->PrintToStr (lhstr);
		if (rhs = tree->RArg()) rhs->PrintToStr (rhstr);
		if (!lhs || !rhs) retval = -1;
		if (SetAttribute (ClusterId, ProcId, lhstr, rhstr) == -1) {
			fprintf(stderr, "\nERROR: Failed to set %s=%s for job %d.%d\n", lhstr, rhstr, ClusterId, ProcId);
			retval = -1;
		}
	}

	return retval;
}


void 
InsertJobExpr (char *expr)
{
	ExprTree *tree, *lhs;
	char      name[128];
	name[0] = '\0';

	int retval = Parse (expr, tree);

	if (retval)
	{
		fprintf (stderr, "\nERROR: Parse error in expression: \n\t%s\n\t", expr);
		while (retval--) {
			fputc( ' ', stderr );
		}
		fprintf (stderr, "^^^\n");
		EXCEPT ("Error in submit file");
	}

	if (lhs = tree->LArg()) 
		lhs->PrintToStr (name);
	else
	{
		fprintf (stderr, "\nERROR: Expression not assignment: %s\n", expr);
		EXCEPT ("Error in submit file");
	}
	
	if (!job.InsertOrUpdate (expr))
	{	
		EXCEPT ("Unable to insert expression: %s\n", expr);
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

