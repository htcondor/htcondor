/* ** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
** Modified by: Cai, Weiru
** Modified by: Derek Wright 7/14/97 to use config() instead of
** 				the complicated configuration done in here.  If/when
**				we have a working config_server, we can integrate it
**				into config(), not all the individual daemons/tools.
**
*/ 

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
#include "get_full_hostname.h"
#include "condor_qmgr.h"

#ifdef __GNUG__
#pragma implementation "extArray.h"
#pragma implementation "HashTable.h"
#endif

#include "extArray.h"
#include "HashTable.h"
#include "MyString.h"

static int hashFunction( MyString&, int );
HashTable<MyString,MyString> forcedAttributes( 64, hashFunction ); 

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

ClassAd job;
char    buffer[_POSIX_ARG_MAX + 64];

char	*OperatingSystem;
char	*Architecture;
char	*Spool;
char	*Flavor;
char	*ThisHost;
char	*My_fs_domain;
char    JobRequirements[2048];
char    JobRootdir[_POSIX_PATH_MAX];
char    JobIwd[_POSIX_PATH_MAX];

int		LineNo;
int		GotQueueCommand;

char	IckptName[_POSIX_PATH_MAX];	/* Pathname of spooled initial ckpt file */

char	*MyName;
int		Quiet = 1;
int     ClusterId = -1;
int     ProcId = -1;
int     JobUniverse;
int		Remote=0;
int		ClusterCreated = FALSE;
int		ActiveQueueConnection = FALSE;
char	*queue_file = NULL;
bool	NewExecutable = false;
bool	UserLogSpecified = false;

#define PROCVARSIZE	32
BUCKET *ProcVars[ PROCVARSIZE ];

#define MEG	(1<<20)

/*
**	Names of possible CONDOR variables.
*/
char	*Cluster 		= "cluster";
char	*Process   		= "process";
char	*Priority		= "priority";
char	*Notification	= "notification";
char	*Executable		= "executable";
char	*Arguments		= "arguments";
char	*GetEnv			= "getenv";
char	*Environment	= "environment";
char	*Input			= "input";
char	*Output			= "output";
char	*Error			= "error";
char	*RootDir		= "rootdir";
char	*InitialDir		= "initialdir";
char	*Requirements	= "requirements";
char	*Preferences	= "preferences";
char	*Rank			= "rank";
char	*ImageSize		= "image_size";
char	*Universe		= "universe";
char	*MachineCount	= "machine_count";
char    *NotifyUser     = "notify_user";
char    *UserLogFile    = "log";
char	*CoreSize		= "coresize";
char	*NiceUser		= "nice_user";
#if !defined(WIN32)
char	*KillSig		= "kill_sig";
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
void 	SetRootDir();
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
int 	read_condor_file( FILE *fp );
char * 	condor_param( char *name );
void 	set_condor_param( char *name, char *value );
void 	queue(int num);
char * 	check_requirements( char *orig );
void 	check_open( char *name, int flags );
void 	usage();
int 	DoCleanup();
char * 	get_owner();
char * 	get_owner();
void 	init_params();
int 	whitespace( char *str);
void 	delete_commas( char *ptr );
void 	compress( char *str );
void 	magic_check();
void 	log_submit();
void 	get_time_conv( int &hours, int &minutes );
int     SaveClassAd (ClassAd &);
void	InsertJobExpr (char *expr);

extern char **environ;

extern "C" {

int SetSyscalls( int foo );
int	get_machine_name(char*);
char *get_schedd_addr(char *);

}

struct SubmitRec {
	int cluster;
	int firstjob;
	int lastjob;
	char *logfile;
};

ExtArray <SubmitRec> SubmitInfo(10);
int CurrentSubmitInfo = -1;

int
main( int argc, char *argv[] )
{
	FILE	*fp;
	char	**ptr;
	char	*cmd_file = NULL;
	int dag_pause = 0;
	char	*fullhost;
	
	setbuf( stdout, NULL );

	MyName = argv[0];
	config( 0 );
	init_params();

	DebugFlags |= D_EXPR;

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if( ptr[0][1] == 'v' ) {
				Quiet = 0;
			} else 
			if( ptr[0][1] == 'p' ) {
               // the -p option will cause condor_submit to pause for about
               // 4 seconds upon completion.  this prevents 'Broken Pipe'
               // messages when condor_submit is called from DagMan on
               // platforms with crappy popen(), like IRIX - Todd T, 2/97
               dag_pause = 1;
			} else
			if ( ptr[0][1] == 'r' ) {
				Remote++;
				ptr++;
				if( !(fullhost = get_full_hostname(*ptr)) ) {
					fprintf( stderr, "%s: unknown host %s\n", 
							 MyName, *ptr );
					exit(1);
				}
				queue_file = strdup(fullhost);
			} else {
				usage();
			}
		} else {
			cmd_file = *ptr;
		}
	}
	
	// set up types of the ad
	job.SetMyTypeName (JOB_ADTYPE);
	job.SetTargetTypeName (STARTD_ADTYPE);

	DebugFlags |= D_EXPR;

	if( cmd_file == NULL ) {
		usage();
	}

	if (Remote) {
		ThisHost = queue_file;
	} else {
		ThisHost = my_full_hostname();
	}

	// open submit file
	if( (fp=fopen(cmd_file,"r")) == NULL ) {
		fprintf( stderr, "Failed to open command file\n");
		exit(1);
	}

	// connect to the schedd
	if (ConnectQ(queue_file) == 0) {
		fprintf(stderr, "Failed to connect to qmgr\n");
		exit(1);
	}

	ActiveQueueConnection = TRUE;

	// in case things go awry ...
	_EXCEPT_Cleanup = DoCleanup;

	(void) sprintf (buffer, "%s = %d", ATTR_Q_DATE, (int)time ((time_t *) 0));
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = 0", ATTR_COMPLETION_DATE);
	InsertJobExpr (buffer);

	(void) sprintf (buffer, "%s = \"%s\"", ATTR_OWNER, get_owner());
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

	if (Quiet) {
		fprintf(stdout, "Submitting job(s)");
	}

	//  Parse the file and queue the jobs 
	if( read_condor_file(fp) < 0 ) {
		fprintf(stderr, "Failed to parse command file.\n");
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
		fprintf( stderr, "\"%s\" doesn't contain any \"queue\"", cmd_file );
		fprintf( stderr, " commands -- no jobs queued\n" );
		exit( 1 );
	}

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
	char		*scheddAddr;

	if ((scheddAddr = get_schedd_addr(ThisHost)) == NULL)
	{
		EXCEPT("Can't find schedd address on %s\n", ThisHost);
	}

	/* Connect to the schedd */
	ReliSock sock(scheddAddr, SCHED_PORT);
	if(sock.get_file_desc() < 0) {
		EXCEPT( "Can't connect to condor scheduler (%s)\n",
				scheddAddr );
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
		fprintf(stderr, "ERROR: Value for \"%s\" is too long:\n"
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
  static int exec_set = 0;

  if( exec_set ) {
	return;
  }

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

  (void) sprintf (buffer, "%s = %d", ATTR_JOB_STATUS, UNEXPANDED);
  InsertJobExpr (buffer);

  SetUniverse();
	
  switch(JobUniverse) 
	{
	case STANDARD:
	  (void) sprintf (buffer, "%s = 1", ATTR_WANT_REMOTE_SYSCALLS);
	  InsertJobExpr (buffer);
	  (void) sprintf (buffer, "%s = 1", ATTR_WANT_CHECKPOINT);
	  InsertJobExpr (buffer);
	  break;
	case PVM:
	case PIPE:
	  (void) sprintf (buffer, "%s = 1", ATTR_WANT_REMOTE_SYSCALLS);
	  InsertJobExpr (buffer);
	  (void) sprintf (buffer, "%s = 0", ATTR_WANT_CHECKPOINT);
	  InsertJobExpr (buffer);
	  break;
	case VANILLA:
	case SCHED_UNIVERSE:
	  (void) sprintf (buffer, "%s = 0", ATTR_WANT_REMOTE_SYSCALLS);
	  InsertJobExpr (buffer);
	  (void) sprintf (buffer, "%s = 0", ATTR_WANT_CHECKPOINT);
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

  exec_set = 1;
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
	char	*tmp;
	char	*p;
	char    buff[2048];
	int		mem_req = 0;

	tmp = condor_param(ImageSize);

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
	} else {
		ASSERT (job.LookupString ("Cmd", buff));
		size = calc_image_size( buff );
	}


	/* It's reasonable to expect the image size will be at least the
	   physical memory requirement, so make sure it is. */
	  
	strcpy (buff, JobRequirements);	
	for( p=buff; *p; p++ ) {
		if( strincmp("Memory",p,6) == 0 ) {
			while( *p && !isdigit(*p) ) {
				p++;
			}
			if( *p ) {
				mem_req = atoi( p ) * 1024;
			}
			break;
		}
	}

	(void)sprintf (buffer, "%s = %d", ATTR_IMAGE_SIZE,
				   mem_req > size ? mem_req : size);
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
	char    buffer[_POSIX_PATH_MAX + 32];

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
		fprintf(stderr, "ERROR: arguments are too long:\n"
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
	char *ptr = NULL;

	if (orig_pref && orig_rank) {
		fprintf(stderr, "%s and %s may not both be specified for a job\n",
			   Preferences, Rank);
		exit(1);
	} else if (orig_rank) {
		(void)strcpy(rank, orig_rank);
	} else if (orig_pref) {
		(void)strcpy(rank, orig_pref);
	} else {
		rank[0] = '\0';
	}

	if ( JobUniverse == STANDARD ) 
	{
		ptr = param("APPEND_RANK_STANDARD");
	} 
	if ( JobUniverse == VANILLA ) 
	{
		ptr = param("APPEND_RANK_VANILLA");
	} 

	if ( ptr != NULL ) {
		if( rank[0] ) {
			(void)strcat( rank, " && (" );
		} else {
			(void)strcpy( rank, "(" );
		}
		(void) strcat( rank, ptr );
		(void) strcat( rank,")" );
	}
				
	if( rank[0] == '\0' ) 
	{
		(void) sprintf (buffer, "%s = 0", ATTR_RANK);
		InsertJobExpr (buffer);
	} 
	else 
	{
		(void) sprintf (buffer, "%s = %s", ATTR_RANK, rank);
		InsertJobExpr (buffer);
	}
}

void
SetIWD()
{
	char	*shortname;
	char	iwd[ _POSIX_PATH_MAX ];
	char	cwd[ _POSIX_PATH_MAX ];

	shortname = condor_param( InitialDir );

	if( strcmp(JobRootdir,"/") != MATCH )	{	/* Rootdir specified */
		if( shortname ) {
			(void)strcpy( iwd, shortname );
		} else {
			(void)strcpy( iwd, "/" );
		}
	} else {
		if( shortname  ) {
			if( shortname[0] == '/' ) {
				(void)strcpy( iwd, shortname );
			} else {
				(void)getcwd( cwd, sizeof cwd );
				(void)sprintf( iwd, "%s/%s", cwd, shortname );
			}
		} else {
			(void)getcwd( iwd, sizeof iwd );
		}
	}

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
#if defined(HPUX9) || defined(WIN32) /* RLIMIT_CORE not supported */
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
			fprintf( stderr, "Warning:  Unable to expand macros in \"%s\"."
							"  Ignoring.\n", value.Value() );
			continue;
		}
		sprintf( buffer, "%s = %s", name.Value(), exValue );
		InsertJobExpr( buffer );

		// free memory allocated by macro expansion module
		free( exValue );
	}	
}


int
sig_name_lookup(char sig[])
{
	for (int i = 0; SigNameArray[i].n != 0; i++) {
		if (stricmp(SigNameArray[i].n, sig) == 0) {
			return SigNameArray[i].v;
		}
	}
	fprintf( stderr, "unknown signal %s\n", sig );
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
#endif

int
read_condor_file( FILE *fp )
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
		} else
		if (*name == '-') {
			name++;
			forcedAttributes.remove( name );
			job.Delete( name );
			continue;
		}

		if( strincmp(name, "queue", strlen("queue")) == 0 ) {
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
				fprintf(stderr, "failed to expand macros in: %s\n", name);
				return( -1 );
			}

		}

		/* if the user wanted to force the parameter into the classad, do it */
		if (force == 1) {
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
		fprintf(stderr, "failed to expand macros in: %s\n", name);
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
				fprintf(stderr, "\nError: Failed to create cluster\n");
				exit(1);
			}
			ProcId = -1;
		}

		if ( ClusterId == -1 ) {
			fprintf(stderr,"\nError: Used queue command without specifying an executable\n");
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

		SetExecutable();
		SetRootDir();
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
			fprintf( stderr, "Failed to queue job.\n" );
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
	int		has_fsdomain = FALSE;
	char	*ptr;
	static char	answer[2048];

	for( ptr = orig; *ptr; ptr++ ) {
		if( strincmp("Arch",ptr,4) == MATCH ) {
			has_arch = TRUE;
			break;
		}
	}

	for( ptr = orig; *ptr; ptr++ ) {
		if( strincmp("OpSys",ptr,5) == MATCH ) {
			has_opsys = TRUE;
			break;
		}
	}
 
	(void)strcpy( answer, orig );

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

	if (JobUniverse == PVM) {
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
			if( strincmp("FilesystemDo",ptr,12) == MATCH ) {
				has_fsdomain = TRUE;
				break;
			}
		}
/*
		if ( !has_fsdomain ) {
			(void)strcat( answer, " && (FilesystemDomain == \"" );
			(void)strcat( answer, My_fs_domain );
			(void)strcat( answer, "\")" );
		} 
*/
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

	/* No need to check for existence of the Null file. */
	if (strcmp(name, NullFile) == MATCH) return;

	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		(void)sprintf( pathname, "%s%s", JobRootdir, name );
	} else {	/* relative to iwd which is relative to the root */
		(void)sprintf( pathname, "%s/%s/%s", JobRootdir, JobIwd, name );
	}
	compress( pathname );

	if( (fd=open(pathname,flags,0664)) < 0 ) {
		EXCEPT( "Can't open \"%s\"  with flags 0%o", pathname, flags );
	}
	(void)close( fd );
}


void
usage()
{
	fprintf( stderr, "Usage: %s [-q] [-v] [-r \"hostname\"] cmdfile\n", MyName );
	exit( 1 );
}


int
DoCleanup()
{
	if( ClusterCreated ) 
	{
		// TerminateCluster() may call EXCEPT() which in turn calls 
		// DoCleanup().  This lead to infinite recursion which is bad.
		ClusterCreated = 0;
		if (!ActiveQueueConnection) {
			ActiveQueueConnection = (ConnectQ(queue_file) != 0);
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


char *
get_owner()
{
	if (Remote) return ("nobody");

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
	if ( My_fs_domain == NULL ) {
		My_fs_domain = (char*)malloc(150);
		My_fs_domain[0] = '\0';
		get_machine_name( My_fs_domain );
	}
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

	while( *dst ) {
		*dst++ = *src++;
		while( *(src - 1) == '/' && *src == '/' ) {
			src++;
		}
	}
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
    char    *simple_name;
    char    *path;
    char    tmp[ _POSIX_PATH_MAX ];
	char    owner[64];
    UserLog usr_log;
    SubmitEvent jobSubmit;

	if (Quiet) fprintf(stdout, "Logging submit event(s)");

	job.LookupString (ATTR_OWNER, owner);
	strcpy (jobSubmit.submitHost, ThisHost);

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
					fprintf (stderr, "Error logging submit event.\n");
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
	char lhstr[128], rhstr[2048];
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
			fprintf(stderr, "Failed to set %s=%s for job %d.%d\n", lhstr, rhstr, ClusterId, ProcId);
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
	int retval = Parse (expr, tree);

	if (retval)
	{
		fprintf (stderr, "Parse error in expression: \n\t%s\n\t", expr);
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
		fprintf (stderr, "Expression not assignment: %s\n", expr);
		EXCEPT ("Error in submit file");
	}
	
	if (!job.InsertOrUpdate (expr))
	{	
		EXCEPT ("Unable to insert expression: %s\n", expr);
	}
}

static int 
hashFunction (MyString &str, int numBuckets)
{
    int i = str.Length() - 1, hashVal = 0;
    while (i >= 0) 
    {
        hashVal += tolower(str[i]);
        i--;
    }
    return (hashVal % numBuckets);
}

