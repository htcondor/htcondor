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
/* #include "condor_jobqueue.h" */
#include "condor_constants.h"
#include "condor_expressions.h"
#include "condor_network.h"
#include "condor_string.h"
#include "condor_ckpt_name.h"
#include "proc_obj.h"
#include "sched.h"
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>

#include "../condor_schedd/condor_qmgr.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char	*OperatingSystem;
char	*Architecture;
char	*Spool;
char	*Flavor;
char	*ThisHost;
char	*My_fs_domain;


int		LineNo;
int		GotQueueCommand;

DBM		*Q, *OpenJobQueue();
PROC	Proc;

char	IckptName[_POSIX_PATH_MAX];	/* Pathname of spooled initial ckpt file */


char	*MyName;
int		Quiet;
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

extern int	Terse;
extern int	DontDisplayTime;

void reschedule();
void SetExecutable();
void SetUniverse();
void SetImageSize();
int calc_image_size( char *name );
void SetPipes();
void check_pipe_name( char *name );
BOOLEAN topology_check( P_DESC pipe_array[], int n_pipes );
int find_cmd( char *name );
char * get_tok();
void SetStdFile( int which_file );
void SetPriority();
void SetNotification();
void SetArguments();
void SetEnvironment();
void SetRootDir();
void SetRequirements();
void SetPreferences();
void SetIWD();
void check_iwd( char *iwd );
int read_condor_file( FILE *fp );
char * condor_param( char *name );
void set_condor_param( char *name, char *value );
void queue();
char * check_requirements( char *orig );
void check_open( char *name, int flags );
void usage();
int DoCleanup();
char * get_owner();
char * get_owner();
void init_params();
int whitespace( char *str);
void check_expr_syntax( char *expr );
CONTEXT	* fake_machine_context();
void delete_commas( char *ptr );
void compress( char *str );
void magic_check();
void log_submit();
void get_time_conv( int &hours, int &minutes );
char *my_hostname();

extern "C" {
void _mkckpt( char *, char * );
int SetSyscalls( int foo );
int	get_machine_name(char*);
}

int
main( int argc, char *argv[] )
{
	FILE	*fp;
	char	queue_name[_POSIX_PATH_MAX];
	char	**ptr;
	char	*cmd_file = NULL;

	setbuf( stdout, NULL );

	MyName = argv[0];

	config( MyName, (CONTEXT *)0 );
	init_params();


	if( argc < 2 || argc > 3 ) {
		usage();
	}

	DebugFlags |= D_EXPR;
	Terse = 1;
	DontDisplayTime = TRUE;

	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if( ptr[0][1] == 'q' ) {
				Quiet++;
			} else {
				usage();
			}
		} else {
			cmd_file = *ptr;
		}
	}

	if( cmd_file == NULL ) {
		usage();
	}

	ThisHost = my_hostname();

	if( (fp=fopen(cmd_file,"r")) == NULL ) {
		EXCEPT( "fopen(%s,\"r\")", argv[1] );
	}

#if DBM_QUEUE
		/* Open job queue and initialize a new cluster */
	(void)sprintf( queue_name, "%s/job_queue", Spool );
	if( (Q=OpenJobQueue(queue_name,O_RDWR | O_CREAT,0664)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue_name );
	}
	LockJobQueue( Q, WRITER );

#else
	if (ConnectQ() == 0) {
		fprintf(stderr, "Failed to connect to qmgr\n");
		exit(1);
	}
#endif
	Proc.q_date = (int)time( (time_t *)0 );
	Proc.completion_date = 0;
	Proc.version_num = PROC_VERSION;
	Proc.owner = get_owner();
	Proc.id.cluster = NewCluster();
	if (Proc.id.cluster == -1) {
		fprintf(stderr, "Failed to create cluster\n");
		exit(1);
	}
	Proc.id.proc = 0;

	dprintf( D_FULLDEBUG, "Proc.version_num = %d\n", Proc.version_num );

	_EXCEPT_Cleanup = DoCleanup;

		/* Parse the file and queue the jobs */
	if( read_condor_file(fp) < 0 ) {
		EXCEPT( "CONDOR description file error" );
	}

#if 0
	CloseJobQueue( Q );
	Q = (DBM *)0;
#endif
	DisconnectQ(0);

	if( Proc.id.proc == 0 ) {
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



		/* Connect to the schedd */
	if( (sock = do_connect(ThisHost, "condor_schedd", SCHED_PORT)) < 0 ) {
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
		fprintf(stderr,
"ERROR: Value for \"%s\" is too long:\n\tPosix limits path names to %d bytes\n",
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
	int		i;

	if( exec_set ) {
		return;
	}

	ename = condor_param(Executable);

	if( ename == NULL ) {
		EXCEPT("No '%s' parameter was provided", Executable);
	}

	check_path_length(ename, Executable);

	ename = string_copy( ename );


	delete_commas( ename );
	mkargv( &argc, argv, ename );

	Proc.n_cmds = argc;
	Proc.cmd = (char **)malloc( Proc.n_cmds * sizeof(char *) );
	Proc.args = (char **)malloc( Proc.n_cmds * sizeof(char *) );
	Proc.in = (char **)malloc( Proc.n_cmds * sizeof(char *) );
	Proc.out = (char **)malloc( Proc.n_cmds * sizeof(char *) );
	Proc.err = (char **)malloc( Proc.n_cmds * sizeof(char *) );
	Proc.remote_usage = (struct rusage *)
						calloc( Proc.n_cmds, sizeof(struct rusage) );
	Proc.exit_status = (int *) calloc( Proc.n_cmds, sizeof(int) );
	Proc.min_needed = Proc.max_needed = 1;
	Proc.status = UNEXPANDED;

	SetUniverse();
/*	fprintf( stderr, "Universe = %d\n", Proc.universe ); */

	switch( Proc.universe ) {
		case STANDARD:
			Proc.remote_syscalls = TRUE;
			Proc.checkpoint = TRUE;
			break;
		case PVM:
			Proc.remote_syscalls = TRUE;
			Proc.checkpoint = FALSE;
			break;
		case PIPE:
			Proc.remote_syscalls = TRUE;
			Proc.checkpoint = FALSE;
			break;
		case VANILLA:
			Proc.remote_syscalls = FALSE;
			Proc.checkpoint = FALSE;
			break;
		default:
			EXCEPT( "Unknown Proc.universe (%d)", Proc.universe );
	}

	for( i=0; i<argc; i++ ) {
		strcpy( IckptName, gen_ckpt_name(Spool,Proc.id.cluster,ICKPT,i) );
		_mkckpt(IckptName,argv[i]);
		Proc.cmd[i] = string_copy( argv[i] );
	}


	exec_set = 1;
}

void
SetUniverse()
{
	char	*univ;
	char	*mach_count;
	int		i;
	P_DESC	dummy;
	char	*ptr;

	univ = condor_param(Universe);

	if( univ && stricmp(univ,"pvm") == MATCH ) {
		Proc.universe = PVM;
		Proc.checkpoint = FALSE;
		mach_count = condor_param(MachineCount);
		if (mach_count != NULL) {
			for (ptr = mach_count; *ptr && *ptr != '.'; ptr++) ;
			if (*ptr != '\0') {
				*ptr = '\0';
				ptr++;
			}
			Proc.min_needed = atoi(mach_count);
			Proc.max_needed = Proc.min_needed;
			for ( ; !isdigit(*ptr) && *ptr; ptr++) ;
			if (*ptr != '\0') {
				Proc.max_needed = atoi(ptr);
			}
		}
		return;
	};

	if( univ && stricmp(univ,"pipe") == MATCH ) {
		Proc.universe = PIPE;
		Proc.checkpoint = FALSE;
		return;
	};

	if( univ && stricmp(univ,"vanilla") == MATCH ) {
		Proc.universe = VANILLA;
		return;
	};

	if( Proc.n_cmds == 1 ) {
#if defined(ALL_VANILLA)
		Proc.universe = VANILLA;
#else
		Proc.universe = STANDARD;
		Proc.checkpoint = TRUE;
#endif
		Proc.n_pipes = 0;
	} else {
		Proc.universe = PIPE;
	}
}

void
SetImageSize()
{
	int		size;
	char	*tmp;
	char	*p;
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
		size = 0;
		for( i=0; i<Proc.n_cmds; i++ ) {
			size += calc_image_size( Proc.cmd[i] );
		}
	}

		/* It's reasonable to expect the image size will be at least the
		   physical memory requirement, so make sure it is. */
	for( p=Proc.requirements; *p; p++ ) {
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
	if( mem_req > size ) {
		Proc.image_size = mem_req;
	} else {
		Proc.image_size = size;
	}
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

/*
** The "pipe" command is a list of pipe specifications.  The list
** may be separated by commas.  Each specification looks like
**
** 	writer_name < opt_pipe_name > reader_name
**
** writer_name and reader_name must be names of commands listed in
** the "executable" command.  opt_pipe_name is either the name
** associated with a named pipe, or may be null.  In that case
** the pipe will be buile from fd 0 of the writer to fd 1
** of the reader.
*/
char	*TokStream;
void
SetPipes()
{
	char	*str;
	char	*token;
	int		i;
	P_DESC	pipe_array[256];

	if( (str = condor_param("pipe")) == NULL ) {
/*		Proc.n_pipes = 0; */
		return;
	}
		
	printf( "Pipe macro = \"%s\"\n", str );

	TokStream = str;

	for( i=0; ; i++ ) {
		
			/* name of writer */
		token = get_tok();
		if( token == NULL )
			break;

		pipe_array[i].writer = find_cmd( token );

			/* Should be '<' */
		token = get_tok();	/* < */
		if( token[0] != '<' ) {
			EXCEPT( "Syntax error on \"pipe\" command" );
		}

			/* May be name of pipe, or '>' */
		token = get_tok();	
		if( token[0] == '>' ) {
			pipe_array[i].name = NULL;
		} else {
			pipe_array[i].name = token;
			check_pipe_name( token );
			token = get_tok();
			if( token[0] != '>' ) {
				EXCEPT( "Syntax error on \"pipe\" command" );
			}
		}

		token = get_tok();
		pipe_array[i].reader = find_cmd( token );
	}

	if( !topology_check(pipe_array,i) ) {
		EXCEPT( "Only simple left to right pipelines are allowed" );
	}

	Proc.n_pipes = i;
	Proc.pipe = (P_DESC *)malloc( Proc.n_pipes * sizeof(P_DESC) );
	memcpy( Proc.pipe, pipe_array, Proc.n_pipes * sizeof(P_DESC) );


}

/*
** Check the name for a named pipe.  For now this must be a
** simple file name not containing any path, either relative
** or absolute.
*/
void
check_pipe_name( char *name )
{
	while( *name ) {
		if( *name == '/' ) {
			EXCEPT( "Named pipes must be simple file name, no paths" );
		}
		name += 1;
	}
}

/*
** Check the topology of a set of pipes between processes.  For now
** the pipes must be a single line left to right with "left to right"
** being defined by the order in which the processes were named in
** the "executable" line.
*/
BOOLEAN
topology_check( P_DESC pipe_array[], int n_pipes )
{
	int		i;

	for( i=0; i<n_pipes; i++ ) {
		if( pipe_array[i].reader < pipe_array[i].writer ) {
			return FALSE;
		}
	}
	return TRUE;
}

/*
** Given the name of an executable which is already stored in the
** Proc structure, return it's index in the "cmd" array.  Return
** -1 if it is not found.
*/
int
find_cmd( char *name )
{
	int		i;

	for( i=0; i<Proc.n_cmds; i++ ) {
		if( strcmp(name,Proc.cmd[i]) == MATCH ) {
			return i;
		}
	}
	EXCEPT( "Can't build pipe for \"%s\" - not in your \"executable\" command");

		// Can never get here.
	return -1;
}

/*
** Trivial lexical analyzer for statements describing pipes.  The
** tokens are character strings, '<', and '>'.  Commas and
** white space are ignored.  NULL is returned both at end of
** input and in case of errors.
*/
char *
get_tok()
{
	char	buf[256], *dst = buf;
	char	*p = TokStream;

	while( *p && isspace(*p) ) {
		p++;
	}

	if( isalpha(*p) ) {
		while( *p && *p != '<' && *p != '>' && *p != ','
												&& !isspace(*p) ) {
			*dst++ = *p++;
		}
		*dst = '\0';
		TokStream = p;
		return string_copy( buf );
	}

	if( *p == '<' || * p == '>' ) {
		*dst++ = *p++;
		*dst = '\0';
		TokStream = p;
		return string_copy( buf );
	}

	if( *p == ',' ) {
		TokStream++;
		return get_tok();
	}

	return NULL;
}


void
SetStdFile( int which_file )
{
	int		i;
	char	macro_name[128];
	char	*macro_value;
	char	*generic_name;

	switch( which_file ) {
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


	for( i=0; i<Proc.n_cmds; i++ ) {

			/* Name of file should be stored under macro
			<cmd_name>.<generic_name> where <generic_name> is
			one of "input", "output", "error" - try to find
			it that way. */
		sprintf( macro_name, "%s.%s", Proc.cmd[i], generic_name );
		macro_value = condor_param(macro_name);

			/* If we didn't find it, and there is only one
			   executable, this could be an old job description
			   file - try macro name <generic_name>. */
		if( !macro_value && Proc.n_cmds == 1 ) {
			macro_value = condor_param( generic_name );
		}

		if( !macro_value ) {
			macro_value = "/dev/null";
		}

		if( whitespace(macro_value) ) {
			EXCEPT("The '%s' takes exactly one argument (%s)",
					generic_name, macro_value);
		}

		check_path_length(macro_value, generic_name);

		switch( which_file ) {
			case 0:
				Proc.in[i] = macro_value;
				check_open( macro_value, O_RDONLY );
				break;
			case 1:
				Proc.out[i] = macro_value;
				check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
				break;
			case 2:
				Proc.err[i] = macro_value;
				check_open( macro_value, O_WRONLY|O_CREAT|O_TRUNC );
				break;
		}
	}
}

void
SetPriority()
{
	char *prio = condor_param(Priority);

	if( prio == NULL ) {
		Proc.prio = 0;
	} else {
		Proc.prio = atoi( prio );
		if( Proc.prio < -20 || Proc.prio > 20 ) {
			EXCEPT("Priority must be in the range -20 thru 20 (%d)",
					Proc.prio );
		}
	}
}

void
SetNotification()
{
	char *how = condor_param(Notification);

	if( (how == NULL) || (stricmp(how, "COMPLETE") == 0) ) {
		Proc.notification = NOTIFY_COMPLETE;
	} else if( stricmp(how, "NEVER") == 0 ) {
		Proc.notification = NOTIFY_NEVER;
	} else if( stricmp(how, "ALWAYS") == 0 ) {
		Proc.notification = NOTIFY_ALWAYS;
	} else if( stricmp(how, "ERROR") == 0 ) {
		Proc.notification = NOTIFY_ERROR;
	} else {
		EXCEPT("Notification must be 'Never', 'Always', 'Complete', or 'Error'");
	}
}

void
SetArguments()
{
	char	*args = condor_param(Arguments);
	char	*macro_value;
	char	macro_name[ 128 ];
	int		i;

	if( args == NULL ) {
		args = "";
	}

	for( i=0; i<Proc.n_cmds; i++ ) {

			/* Command line arguments should be stored under macro
			<cmd_name>.arguments - try to find it that way. */
		sprintf( macro_name, "%s.arguments", Proc.cmd[i] );
		macro_value = condor_param(macro_name);
		if( !macro_value ) {
			sprintf( macro_name, "%s.args", Proc.cmd[i] );
			macro_value = condor_param(macro_name);
		}

			/* If we didn't find it, and there is only one
			   executable, this could be an old job description
			   file - try macro name <generic_name>. OR, it's PVM
			   and only one executable is allowed, but n_cmds has
			   machine count */
		if( !macro_value && (Proc.n_cmds == 1 || Proc.universe == PVM)) {
			macro_value = args;
		}

		if( !macro_value ) {
			macro_value = "";
		}

		check_path_length(macro_value, Arguments);

		Proc.args[i] = macro_value;
	}
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
	struct rlimit rl;
	char tmp[BUFSIZ];

	Proc.env = "";
	/* Defined in command file */
	if( env != NULL ) {
		Proc.env = env;
	}
	/* Defined in shell environment variable */
	else if( shell_env = getenv("CONDOR_CORESIZE") ) {
		sprintf( tmp, "CONDOR_CORESIZE=%s", shell_env );
		env = string_copy( tmp );
		Proc.env = env;
	} 
	/* Defined by shell variable */
	else {
#if defined(HPUX9) 		/* hp-ux doesn't support RLIMIT_CORE */
		Proc.env = "";
#else
		if ( getrlimit( RLIMIT_CORE, &rl ) == -1) {
			EXCEPT("getrlimit failed");
		}

		/* RLIM_INFINITY is a magic cookie, don't try converting to kbytes */
		if( rl.rlim_cur == RLIM_INFINITY) {
			sprintf( tmp, CoreSizeFmt, rl.rlim_cur );
		} else {
			sprintf( tmp, CoreSizeFmt, rl.rlim_cur/1024 );
		}
		env = string_copy( tmp );

		check_path_length(env, Environment);

		Proc.env = env;
#endif
	}
}

void
SetRootDir()
{
	char *rootdir = condor_param(RootDir);

	if( rootdir == NULL ) {
		Proc.rootdir = "/";
	} else {
		if( access(rootdir, F_OK|X_OK) < 0 ) {
			EXCEPT("No such directory: %s", rootdir);
		}

		check_path_length(rootdir, RootDir);

		Proc.rootdir = rootdir;
	}
}

void
SetRequirements()
{
	char *requirements = condor_param(Requirements);
	if( requirements == NULL ) {
		Proc.requirements = "";
	} else {
		Proc.requirements = requirements;
	}

	Proc.requirements = check_requirements( Proc.requirements );

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

	if ( Proc.universe == STANDARD ) {
		ptr = param("APPEND_PREF_STANDARD");
	} 
	if ( Proc.universe == VANILLA ) {
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
				
	if( preferences[0] == '\0' ) {
		Proc.preferences = "TRUE";
	} else {
		check_expr_syntax(  preferences );
		Proc.preferences = preferences;
	}
}

void
SetIWD()
{
	char	*shortname;
	char	iwd[ _POSIX_PATH_MAX ];
	char	cwd[ _POSIX_PATH_MAX ];

	shortname = condor_param( InitialDir );

	if( strcmp(Proc.rootdir,"/") != MATCH )	{	/* Rootdir specified */
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

	Proc.iwd = string_copy( iwd );
}

void
check_iwd( char *iwd )
{
	char	pathname[ _POSIX_PATH_MAX ];

	(void)sprintf( pathname, "%s/%s", Proc.rootdir, iwd );
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
	char  *blah;
	ProcObj	*proc_obj;

	/*
	**	Insert the current idea of the cluster and
	**	process number into the hash table.
	*/
	GotQueueCommand = 1;
	(void)sprintf(tmp, "%d", Proc.id.cluster);
	set_condor_param(Cluster, tmp);
	(void)sprintf(tmp, "%d", Proc.id.proc);
	set_condor_param(Process, tmp);


	SetExecutable();
	blah = string_copy(Proc.cmd[0]);
	SetRootDir();
	SetIWD();
	SetPriority();
	SetArguments();
	SetEnvironment();
	SetNotification();
	SetRequirements();
	SetPreferences();
	SetPipes();
	SetStdFile( 0 );
	SetStdFile( 1 );
	SetStdFile( 2 );
	Proc.cmd[0] = blah;
	SetImageSize();


#if 0
	if (Proc.id.cluster == -1) {
		Proc.id.cluster = NewCluster();
		if (Proc.id.cluster == -1) {
			EXCEPT("NewCluster");
		}
	}
#endif

	Proc.id.proc = NewProc( Proc.id.cluster );
#if 0
	rval = StoreProc( Q, &Proc );
#else
	rval = SaveProc( &Proc );
#endif

	switch( rval ) {
		case 0:			/* Success */
		case 1:
			break;
		case E2BIG:		/* Exceedes size limit of dbm/ndbm implementation */
			DoCleanup();
			exit( 1 );
		default:		/* Failed for some other reason... */
			EXCEPT( "SaveProc failed" );
	}

	ClusterCreated = TRUE;
	
	if( !Quiet ) {
		// proc_obj = ProcObj::create( (GENERIC_PROC *)&Proc );
		proc_obj = ProcObj::create( (GENERIC_PROC *) &Proc );
		proc_obj->display();
		// display_proc_long( &Proc );
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

/*	if (Proc.universe == PVM) {
		char	localhost[80];
		gethostname(localhost);
		(void)strcat( answer, " && (Machine != \"" );
		(void)strcat( answer, localhost );
		(void)strcat( answer, "\")" );
	} */

	if ( Proc.universe == VANILLA ) {
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
		if ( !has_fsdomain ) {
			(void)strcat( answer, " && (FilesystemDomain == \"" );
			(void)strcat( answer, My_fs_domain );
			(void)strcat( answer, "\")" );
		} 
	}

	if ( Proc.universe == STANDARD ) {
		ptr = param("APPEND_REQ_STANDARD");
		if ( ptr != NULL ) {
			(void) strcat( answer," && (" );
			(void) strcat( answer, ptr );
			(void) strcat( answer,")" );
		}
	}
				
	check_expr_syntax(  answer );
	return answer;
}

void
check_open( char *name, int flags )
{
	int		fd;
	char	pathname[_POSIX_PATH_MAX];
	int		i;

		/* See is the file refers to a named pipe.  In that
		** case we'll build it at run time.
		*/
	if (Proc.universe == PIPE) {
		for( i=0; i<Proc.n_pipes; i++ ) {
			if( strcmp(name,Proc.pipe[i].name) == MATCH ) {
				return;
			}
		}
	}
			
	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		(void)sprintf( pathname, "%s%s", Proc.rootdir, name );
	} else {	/* relative to iwd which is relative to the root */
		(void)sprintf( pathname, "%s/%s/%s", Proc.rootdir, Proc.iwd, name );
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
	fprintf( stderr, "Usage: %s [-q] cmdfile\n", MyName );
	exit( 1 );
}

int
DoCleanup()
{
	if( ClusterCreated ) {
		// TerminateCluster() may call EXCEPT() which in turn calls 
		// DoCleanup().  This lead to infinite recursion which is bad.
		ClusterCreated = 0;
#if 0
		TerminateCluster( Q, Proc.id.cluster, SUBMISSION_ERR );
#endif
		DestroyCluster( Proc.id.cluster );
	}
	if( IckptName[0] ) {
		(void)unlink( IckptName );
	}

#if 0
	if( Q ) {
		CloseJobQueue( Q );
		Q = 0;
	}
#endif
	return 0;		// For historical reasons...
}


char *
get_owner()
{
	struct passwd	*pwd;

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

CONTEXT	*MachineContext;

void
check_expr_syntax( char *expr )
{
		CONTEXT	*context;
	char	line[1024];
	ELEM	*dummy;

	if( MachineContext == NULL ) {
		MachineContext = fake_machine_context();
	}

	(void)sprintf( line, "DUMMY = %s && (Disk >= 0)", expr );
	context = create_context();
	store_stmt( scan(line), context );

	dummy = eval( "DUMMY", context, MachineContext );
	if( !dummy || dummy->type != BOOL ) {
		fprintf( stderr, "Syntax error: \"%s\"\n", expr );
		DoCleanup();
		exit( 1 );
	}
	free_elem( dummy );
	free_context( context );
}

CONTEXT	*
fake_machine_context()
{
	CONTEXT	*answer;
	char	line[1024];

	answer = create_context();

	(void)sprintf( line, "Arch = \"fake\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "OpSys = \"fake\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Disk = 0" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Memory = 0" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Machine = \"nobody\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Subnet = \"128.105.255\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Flavor = \"none\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Nest = \"none\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "FilesystemDomain = \"somewhere.xxx\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "UidDomain = \"somewhere.xxx\"" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "VirtualMemory = 0" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "ConsoleIdle = 0" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Clock_Min = 0" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "Clock_Day = 0" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "MIPS = 0" );
	store_stmt( scan(line), answer );

	(void)sprintf( line, "KFLOPS = 0" );
	store_stmt( scan(line), answer );

	return answer;
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

extern "C" {
#	include "user_log.h"
}

void
log_submit()
{
	Environ env;
	char	*simple_name;
	char	*path;
	char	tmp[ _POSIX_PATH_MAX ];
	USER_LOG	*usr_log;
	int		hours, minutes;

		// Get name of user log file (if any)
	env.add_string( Proc.env );
	if( (simple_name=env.getenv("LOG")) == NULL ) {
		return;
	}

		// Convert to a pathname using IWD if needed
	if( simple_name[0] == '/' ) {
		path = simple_name;
	} else {
		sprintf( tmp, "%s/%s", Proc.iwd, simple_name );
		path = tmp;
	}

		// Figure out conversion from GMT to local time
	get_time_conv( hours, minutes );

		// Output the information
	usr_log = OpenUserLog( Proc.owner, path, Proc.id.cluster, Proc.id.proc, 0 );
	PutUserLog( usr_log, SUBMIT, ThisHost, hours, minutes );
	CloseUserLog( usr_log );
}

#define SECOND 1
#define MINUTE (SECOND * 60)
#define HOUR (MINUTE * 60)

void
get_time_conv( int &hours, int &minutes )
{
	time_t	gmt_now;
	time_t	local_now;
	time_t	diff;
	int		is_dst;
	struct tm	*tm;
	static int	init_done;
	static int	save_hours;
	static int	save_minutes;

	if( init_done ) {
		hours = save_hours;
		minutes = save_minutes;
		return;
	}
		
		/* Get the current calendar time */
	time( &local_now );

		/* Find out if local time is affected by Daylight Savings Time */
	tm = localtime( &local_now );
	is_dst = tm->tm_isdst;

		/* Convert calendar time to struct tm in GMT */
	tm = gmtime( &local_now );

		/* Convert struct tm back to a calendar time */
	tm->tm_isdst = is_dst;
	gmt_now = mktime( tm );

		/* Get difference in seconds between GMT and Local */
	diff = (local_now - gmt_now);

		/* Convert to hours and minutes */
	hours = diff / HOUR;
	minutes = diff % HOUR;
	if( minutes < 0 ) {
		minutes = -minutes;
	}

	save_hours = hours;
	save_minutes = minutes;
	init_done = TRUE;
}
