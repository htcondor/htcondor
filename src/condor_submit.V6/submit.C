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
**
** 		The configuration is rather complicated now that we have config_server.
**		We will first try to find a condor_config.master in condor directory.
**		If failed, configure as usual. Otherwise, read condor_config.master
**		and look for CONDOR_CONFIG_LOCATION to find out where the master placed
**		the configuration file. If CONDOR_CONFIG_LOCATION is not configured,
**		look in the default directory for the location of the config file.
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
#include "url_condor.h"
#include "proc_obj.h"
#include "sched.h"
#include "condor_classad.h"
#include "condor_parser.h"
#include "files.h"
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>

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

extern int	Terse;
extern int	DontDisplayTime;
extern BUCKET	*ConfigTab[];

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

extern "C" {

void _mkckpt( char *, char * );
int SetSyscalls( int foo );
int	get_machine_name(char*);
int gethostname();
int read_config(char*, char*, CONTEXT*, BUCKET**, int, int);
char *get_schedd_addr(char *);
void Config();
}

int
main( int argc, char *argv[] )
{
	FILE	*fp;
	char	queue_name[_POSIX_PATH_MAX];
	char	**ptr;
	char	*cmd_file = NULL, *queue_file = NULL;
	
	setbuf( stdout, NULL );

	MyName = argv[0];
	config( MyName, (CONTEXT *)0 );
	init_params();

	DebugFlags |= D_EXPR;
	Terse = 1;
	DontDisplayTime = TRUE;

	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if( ptr[0][1] == 'q' ) {
				Quiet++;
			} else if ( ptr[0][1] == 'r' ) {
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

	/* Weiru */
	Config(MyName);
	
	init_params();

	DebugFlags |= D_EXPR;
	Terse = 1;
	DontDisplayTime = TRUE;

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

	(void) sprintf (buffer, "Q_Date = %d", (int)time ((time_t *) 0));
	ASSERT (job.InsertOrUpdate (buffer));

	(void) sprintf (buffer, "Completion_date = 0");
	ASSERT (job.InsertOrUpdate (buffer));

	(void) sprintf (buffer, "Owner = \"%s\"", get_owner());
	ASSERT (job.InsertOrUpdate (buffer));

	(void) sprintf (buffer, "Local_CPU = 0.0");
	ASSERT (job.InsertOrUpdate (buffer));

	(void) sprintf (buffer, "Remote_CPU = 0.0");
	ASSERT (job.InsertOrUpdate (buffer));

	(void) sprintf (buffer, "Exit_status = 0");
	ASSERT (job.InsertOrUpdate (buffer));

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

	return 0;
}


/*
** Send the reschedule command to the local schedd to get the jobs running
** as soon as possible.
*/
void
reschedule()
{
	int			sock = -1;
	int			cmd;
	XDR			xdr, *xdrs = NULL;
	char			*scheddAddr;

	if ((scheddAddr = get_schedd_addr(ThisHost)) == NULL)
	{
		EXCEPT("Can't find schedd address on %s\n", ThisHost);
	}

		/* Connect to the schedd */
	if ((sock = do_connect(scheddAddr, "condor_schedd", SCHED_PORT)) < 0) {
		dprintf(D_ALWAYS, "Can't connect to condor_schedd on %s\n", ThisHost);
		return;
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	cmd = RESCHEDULE;
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );
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
	char	*argv[64];
	int		argc;

	if( exec_set ) {
		return;
	}

	ename = condor_param(Executable);

	if( ename == NULL ) {
		EXCEPT("No '%s' parameter was provided", Executable);
	}

	check_path_length(ename, Executable);

	(void) sprintf (buffer, "Cmd = \"%s\"", ename);
	ASSERT (job.InsertOrUpdate (buffer));

	ASSERT (job.InsertOrUpdate ("MinHosts = 1"));
	ASSERT (job.InsertOrUpdate ("MaxHosts = 1"));

	(void) sprintf (buffer, "Status = %d", UNEXPANDED);
	ASSERT (job.InsertOrUpdate (buffer));

	SetUniverse();
	
	switch(JobUniverse) 
	{
	  case STANDARD:
		ASSERT (job.InsertOrUpdate ("Remote_syscalls = 1"));
		ASSERT (job.InsertOrUpdate ("Checkpoint = 1"));
		break;
	  case PVM:
		ASSERT (job.InsertOrUpdate ("Remote_syscalls = 1"));
		ASSERT (job.InsertOrUpdate ("Checkpoint = 0"));
		break;
	  case PIPE:
		ASSERT (job.InsertOrUpdate ("Remote_syscalls = 1"));
		ASSERT (job.InsertOrUpdate ("Checkpoint = 0"));
		break;
	  case VANILLA:
		ASSERT (job.InsertOrUpdate ("Remote_syscalls = 0"));
		ASSERT (job.InsertOrUpdate ("Checkpoint = 0"));
		break;
	  default:
		EXCEPT( "Unknown universe (%d)", JobUniverse );
	}

	// generate initial checkpoint file
	strcpy( IckptName, gen_ckpt_name(0,ClusterId,ICKPT,0) );
	_mkckpt(IckptName, ename);
		
	exec_set = 1;
}

void
SetUniverse()
{
	char	*univ;
	char	*mach_count;
	int		i;
	int     ckpt;
	P_DESC	dummy;
	char	*ptr;

	univ = condor_param(Universe);

	if( univ && stricmp(univ,"pvm") == MATCH ) 
	{
		int tmp;

		JobUniverse = PVM;
		(void) sprintf (buffer, "Universe = %d", PVM);
		ASSERT (job.InsertOrUpdate (buffer));
		ASSERT (job.InsertOrUpdate ("Checkpoint = 0"));

		mach_count = condor_param(MachineCount);

		if (mach_count != NULL) {
			for (ptr = mach_count; *ptr && *ptr != '.'; ptr++) ;
			if (*ptr != '\0') {
				*ptr = '\0';
				ptr++;
			}

			tmp = atoi(mach_count);
			(void) sprintf (buffer, "MinHosts = %d", tmp);
			ASSERT (job.InsertOrUpdate (buffer));
			
			for ( ; !isdigit(*ptr) && *ptr; ptr++) ;
			if (*ptr != '\0') {
				tmp = atoi(ptr);
			}

			(void) sprintf (buffer, "MaxHosts = %d", tmp);
			ASSERT (job.InsertOrUpdate (buffer));
		}
		return;
	};

	if( univ && stricmp(univ,"pipe") == MATCH ) {
		JobUniverse = PIPE;
		(void) sprintf (buffer, "Universe = %d", PIPE);
		ASSERT (job.InsertOrUpdate (buffer));
		ASSERT (job.InsertOrUpdate ("Checkpoint = 0"));
		return;
	};

	if( univ && stricmp(univ,"vanilla") == MATCH ) {
		JobUniverse = VANILLA;
		(void) sprintf (buffer, "Universe = %d", VANILLA);
		ASSERT (job.InsertOrUpdate (buffer));
		return;
	};

	JobUniverse = STANDARD;
	(void) sprintf (buffer, "Universe = %d", STANDARD);
	ASSERT (job.InsertOrUpdate (buffer));

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
	int		i;

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

	(void)sprintf (buffer, "Image_size = %d", mem_req > size ? mem_req : size);
	ASSERT (job.InsertOrUpdate (buffer));
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
	int		i;
	char	macro_name[128];
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
		macro_value = "/dev/null";
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
		(void) sprintf (buffer, "In = \"%s\"", macro_value);
		ASSERT (job.InsertOrUpdate (buffer));
		check_open( macro_value, O_RDONLY );
		break;
	  case 1:
		(void) sprintf (buffer, "Out = \"%s\"", macro_value);
		ASSERT (job.InsertOrUpdate (buffer));
		check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
		break;
	  case 2:
		(void) sprintf (buffer, "Err = \"%s\"", macro_value);
		ASSERT (job.InsertOrUpdate (buffer));
		check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
		break;
	}
}

void
SetPriority()
{
	char *prio = condor_param(Priority);
	int  prioval = 0;

	if( prio == NULL ) 
	{
		ASSERT (job.InsertOrUpdate ("Prio = 0"));
	} 
	else 
	{
		prioval = atoi (prio);
		if( prioval < -20 || prioval > 20 ) 
		{
			EXCEPT("Priority must be in the range -20 thru 20 (%d)", prioval );
		}
		(void) sprintf (buffer, "Prio = %d", prioval);
		ASSERT (job.InsertOrUpdate (buffer));
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

	(void) sprintf (buffer, "Notification = %d", notification);
	ASSERT (job.InsertOrUpdate (buffer));
}

void
SetNotifyUser()
{
	char *who = condor_param(NotifyUser);

	if (who)
	{
		(void) sprintf (buffer, "NotifyUser = \"%s\"", who);
		ASSERT (job.InsertOrUpdate (buffer));
	}
}
	
void
SetArguments()
{
	char	*args = condor_param(Arguments);

	if( args == NULL ) {
		args = "";
	}

	sprintf (buffer, "Args = \"%s\"", args);
	ASSERT (job.InsertOrUpdate (buffer));
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
	struct rlimit rl;
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
		env = string_copy( tmp );
		envvalue = env;
	} 
	/* Defined by shell variable */
	else 
	{
#if defined(HPUX9) 		/* hp-ux doesn't support RLIMIT_CORE */
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
		env = string_copy( tmp );

		check_path_length(env, Environment);

		envvalue = env;
#endif
	}

	(void) sprintf (buffer, "Env = \"%s\"", envvalue);
	ASSERT (job.InsertOrUpdate (buffer));
}

void
SetRootDir()
{
	char *rootdir = condor_param(RootDir);

	if( rootdir == NULL ) 
	{
		ASSERT (job.InsertOrUpdate ("Rootdir = \"/\""));
		(void) strcpy (JobRootdir, "/");
	} 
	else 
	{
		if( access(rootdir, F_OK|X_OK) < 0 ) {
			EXCEPT("No such directory: %s", rootdir);
		}

		check_path_length(rootdir, RootDir);

		(void) sprintf (buffer, "Rootdir = \"%s\"", rootdir);
		ASSERT (job.InsertOrUpdate (buffer));
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
	(void) sprintf (buffer, "Requirements = %s", tmp);
	strcpy (JobRequirements, tmp);

	ASSERT (job.InsertOrUpdate (buffer));
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
		ASSERT (job.InsertOrUpdate ("Preferences = TRUE"));
	} 
	else 
	{
		(void) sprintf (buffer, "Preferences = %s", preferences);
		ASSERT (job.InsertOrUpdate (buffer));
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

	(void) sprintf (buffer, "Iwd = \"%s\"", iwd);
	ASSERT (job.InsertOrUpdate (buffer));
	strcpy (JobIwd, iwd);
}

void
check_iwd( char *iwd )
{
	char	pathname[ _POSIX_PATH_MAX ];

	(void)sprintf( pathname, "%s/%s", JobRootdir, iwd );
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

	LineNo = 0;

	for(;;) {
		name = getline(fp);
		if( name == NULL ) {
			break;
		}

			/* Skip over comments */
		if( *name == '#' || blankline(name) )
			continue;
		
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
	char *tval = string_copy( value );

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
		case E2BIG:		/* Exceedes size limit of dbm/ndbm implementation */
			DoCleanup();
			exit( 1 );
		default:		/* Failed for some other reason... */
			EXCEPT( "Failed to queue" );
	}

	ClusterCreated = TRUE;
	
	if( !Quiet ) 
	{
		fprintf(stdout, "Proc %d.%d:\n", ClusterId, ProcId);
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
	int		i;

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
	struct passwd	*pwd;

	if (Remote) return ("nobody");

	if( (pwd=getpwuid(getuid())) == NULL ) {
		EXCEPT( "Can't get passwd entry for uid %d\n", getuid() );
	}
	return string_copy(pwd->pw_name);
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
		Flavor = string_copy("none");
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
#include "user_log.c++.h"

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

#define SECOND 1
#define MINUTE (SECOND * 60)
#define HOUR (MINUTE * 60)


int
SaveClassAd (ClassAd &ad)
{
	ExprTree *tree, *lhs, *rhs;
	char lhstr[128], rhstr[2048];
	char buffer[4096];
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


int 
InsertJobExpr (char *expr)
{
	ExprTree *tree, *lhs, *rhs;
	char      name[128];
	int retval = Parse (expr, tree);

	if (retval)
	{
		fprintf (stderr, "Parse error in expression: %s\n", expr);
		EXCEPT ("Error in submit file");
	}

	if (lhs = tree->LArg()) 
		lhs->PrintToStr (name);
	else
	{
		fprintf (stderr, "Expression not assignment: %s\n", expr);
		EXCEPT ("Error in submit file");
	}
	
	if (!job.Lookup (name)) 
		job.InsertOrUpdate (expr);
	else
	{
		if (rhs = tree->RArg()) 
			job.UpdateExpr (name, tree);
		else
		{
			EXCEPT ("Error in expression: %s\n", expr);
		}
	}
}
