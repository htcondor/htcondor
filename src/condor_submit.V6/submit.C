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

#define _POSIX_SOURCE
#if defined(Solaris)
#include "_condor_fix_types.h"
#include <sys/fcntl.h>
#endif

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_expressions.h"
#include "condor_network.h"
#include "condor_string.h"
#include "condor_ckpt_name.h"
#include <time.h>
#include "user_log.c++.h"
#include "url_condor.h"
#include "sched.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_parser.h"
#include "files.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif

#include "condor_qmgr.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

ClassAd job;
char    buffer[_POSIX_PATH_MAX + 64];

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
int		Quiet;
int     ClusterId;
int     ProcId;
int     JobUniverse;
int		Remote=0;
int		ClusterCreated = FALSE;

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
char	*Environment	= "environment";
char	*Input			= "input";
char	*Output			= "output";
char	*Error			= "error";
char	*RootDir		= "rootdir";
char	*InitialDir		= "initialdir";
char	*Requirements	= "requirements";
char	*Preferences	= "preferences";
char	*ImageSize		= "image_size";
char	*Universe		= "universe";
char	*MachineCount	= "machine_count";
char    *NotifyUser     = "notify_user";
char    *UserLogFile    = "user_log";
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
void 	SetPreferences();
void 	SetIWD();
void 	check_iwd( char *iwd );
int 	read_condor_file( FILE *fp );
char * 	condor_param( char *name );
void 	set_condor_param( char *name, char *value );
void 	queue();
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
char *	my_hostname();
int     SaveClassAd (ClassAd &);
void	InsertJobExpr (char *expr);

extern "C" {

int SetSyscalls( int foo );
int	get_machine_name(char*);
char *get_schedd_addr(char *);

}

int
main( int argc, char *argv[] )
{
	FILE	*fp;
	char	**ptr;
	char	*cmd_file = NULL, *queue_file = NULL;
	int dag_pause = 0;
	
	setbuf( stdout, NULL );

	MyName = argv[0];
	config( 0 );
	init_params();

	DebugFlags |= D_EXPR;

	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if( ptr[0][1] == 'q' ) {
				Quiet++;
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
				queue_file = *ptr;
			} else {
				usage();
			}
		} else {
			cmd_file = *ptr;
		}
	}
	
	MyName = argv[0];

	// set up types of the ad
	job.SetMyTypeName (JOB_ADTYPE);
	job.SetTargetTypeName (STARTD_ADTYPE);

	init_params();

	DebugFlags |= D_EXPR;

	if( cmd_file == NULL ) {
		usage();
	}

	if (Remote) {
		ThisHost = queue_file;
	} else {
		ThisHost = my_hostname();
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

	// in case things go awry ...
	_EXCEPT_Cleanup = DoCleanup;

	// begin to initialize the job ad
	if ((ClusterId = NewCluster()) == -1)
	{
		fprintf(stderr, "Failed to create cluster\n");
		exit(1);
	}
	ProcId = -1;

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

	//  Parse the file and queue the jobs 
	if( read_condor_file(fp) < 0 ) {
		fprintf(stderr, "Failed to parse command file.\n");
		exit(1);
	}

	DisconnectQ(0);

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

	sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ARGUMENTS, args);
	InsertJobExpr (buffer);
}

#if defined(ALPHA)
	char	*CoreSizeFmt = "CONDOR_CORESIZE=%ld";
#else
	char	*CoreSizeFmt = "CONDOR_CORESIZE=%d";
#endif


void
SetEnvironment()
{
	char *env = condor_param(Environment);
	char *shell_env;
	char *envvalue;
#if !defined(WIN32)
	struct rlimit rl;
#endif
	char tmp[BUFSIZ];

	envvalue = "";
	/* Defined in command file */
	if( env != NULL ) 
	{
		envvalue = env;
	}
	/* Defined in shell environment variable */
	else 
	if( shell_env = getenv("CONDOR_CORESIZE") ) 
	{
		(void) sprintf( tmp, "CONDOR_CORESIZE=%s", shell_env );
		env = strdup( tmp );
		envvalue = env;
	} 
	/* Defined by shell variable */
	else 
	{
#if defined(HPUX9) || defined(WIN32)		/* hp-ux doesn't support RLIMIT_CORE */
		envvalue = "";
#else
		if ( getrlimit( RLIMIT_CORE, &rl ) == -1) {
			EXCEPT("getrlimit failed");
		}

		/* RLIM_INFINITY is a magic cookie, don't try converting to kbytes */
		if( rl.rlim_cur == RLIM_INFINITY) {
			(void) sprintf( tmp, CoreSizeFmt, rl.rlim_cur );
		} else {
			(void) sprintf( tmp, CoreSizeFmt, rl.rlim_cur/1024 );
		}
		env = strdup( tmp );

		check_path_length(env, Environment);

		envvalue = env;
#endif
	}

	(void) sprintf (buffer, "%s = \"%s\"", ATTR_JOB_ENVIRONMENT, envvalue);
	InsertJobExpr (buffer);
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
SetPreferences()
{
	static char preferences[2048];
	char *orig_pref = condor_param(Preferences);
	char *ptr = NULL;

	if( orig_pref == NULL ) {
		preferences[0] = '\0';
	} else {
		(void)strcpy( preferences, orig_pref );
	}

	if ( JobUniverse == STANDARD ) 
	{
		ptr = param("APPEND_PREF_STANDARD");
	} 
	if ( JobUniverse == VANILLA ) 
	{
		ptr = param("APPEND_PREF_VANILLA");
	} 

	if ( ptr != NULL ) {
		if( preferences[0] ) {
			(void)strcat( preferences, " && (" );
		} else {
			(void)strcpy( preferences, "(" );
		}
		(void) strcat( preferences, ptr );
		(void) strcat( preferences,")" );
	}
				
	if( preferences[0] == '\0' ) 
	{
		(void) sprintf (buffer, "%s = TRUE", ATTR_PREFERENCES);
		InsertJobExpr (buffer);
	} 
	else 
	{
		(void) sprintf (buffer, "%s = %s", ATTR_PREFERENCES, preferences);
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

int
read_condor_file( FILE *fp )
{
	char	*name, *value;
	char	*ptr;
	int		force = 0;

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
			job.Delete (name);
			continue;
		}

		if( stricmp(name, "queue") == 0 ) {
			queue();
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

			/* Expand references to other parameters */
		name = expand_macro( name, ProcVars, PROCVARSIZE );
		if( name == NULL ) {
			(void)fclose( fp );
			return( -1 );
		}

		/* if the user wanted to force the parameter into the classad, do it */
		if (force == 1) {
			char	buffer[2048];
			char 	*exValue = expand_macro (value, ProcVars, PROCVARSIZE);
			if (exValue == NULL) {
				fprintf (stderr, "Error in expanding %s when processing %s\n", 
					value, name);
				exit (1);
			}
			sprintf (buffer, "%s = %s", name, exValue);
			InsertJobExpr (buffer);
			free (exValue);
		} 

		lower_case( name );

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

	return( pval );
}

void
set_condor_param( char *name, char *value )
{
	char *tval = strdup( value );

	insert( name, tval, ProcVars, PROCVARSIZE );
}


void
queue()
{
	char tmp[ BUFSIZ ];
	static int i = 0;
	int		rval;

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
	SetRequirements();
	SetPreferences();
	SetStdFile( 0 );
	SetStdFile( 1 );
	SetStdFile( 2 );
	SetImageSize();


	rval = SaveClassAd( job );

	switch( rval ) {
		case 0:			/* Success */
		case 1:
			break;
		default:		/* Failed for some other reason... */
			EXCEPT( "Failed to queue" );
	}

	ClusterCreated = TRUE;
	
	if( !Quiet ) 
	{
		fprintf(stdout, "\n** Proc %d.%d:\n", ClusterId, ProcId);
		job.fPrint (stdout);
	}

	log_submit();
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
		char	localhost[80];
		gethostname(localhost, 80);
		(void)strcat( answer, " && (Machine != \"" );
		(void)strcat( answer, localhost );
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
	fprintf( stderr, "Usage: %s [-q] [-r \"hostname\"] cmdfile\n", MyName );
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
		DestroyCluster( ClusterId );
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

#include "environ.h"

void
log_submit()
{
    Environ env;
    char    *simple_name;
    char    *path;
    char    tmp[ _POSIX_PATH_MAX ];
	char    owner[64];
    UserLog usr_log;
    SubmitEvent jobSubmit;

    // Get name of user log file (if any)
	if (job.LookupString ("Env", tmp) == FALSE) 
		return;
    env.add_string( tmp );
    if( (simple_name=env.getenv("LOG")) == NULL ) {
        return;
    }

    // Convert to a pathname using IWD if needed
    if( simple_name[0] == '/' ) {
        path = simple_name;
    } else {
        sprintf( tmp, "%s/%s", JobIwd, simple_name );
        path = tmp;
    }

        // Output the information
    strcpy (jobSubmit.submitHost, ThisHost);
	job.LookupString ("Owner", owner);
    usr_log.initialize ( owner, path, ClusterId, ProcId, 0 );
    if (!usr_log.writeEvent (&jobSubmit))
        dprintf (D_ALWAYS, "Error logging submit event.\n");
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
		if (SetAttribute (ClusterId, ProcId, lhstr, rhstr) == -1)
			retval = -1;
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
