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

 

/*
  This is the startup routine for ALL condor programs.
  If the appropriate command-line arguments are given, remote syscalls
  and checkpointing will be initialized.  If not, only checkpointing
  will be enabled.  We assume here that
  our parent will provide command line arguments which tell us how to
  attach to a "command stream", and then provide commands which control
  how we start things.

  The command stream here is a list of commands issued by the
  condor_starter to the condor user process on a special file
  descriptor which the starter will make available to the user process
  when it is born (via a pipe).  The purpose is to control the user
  process's initial execution state and the checkpoint, restart, and
  migrate functions.

  If the command line arguments look like:
  		<program_name> '-_condor_cmd_fd' <fd_number>
  then <fd_number> is the file descriptor from which the process should
  read its commands.

  If the command line arguments look like
		<program_name> '-_condor_cmd_file' <file_name>
  then <file_name> is the name of a *local* file which the program should
  open and read commands from.  This interface is useful for debugging
  since you can run the command from a shell or a debugger without
  the need for a parent process to set up a pipe.

  In any case, once the command stream processing is complete, main()
  will be called in such a way that it appears the above described
  arguments never existed.

  Commands are:

	iwd <pathname>
		Change working directory to <pathname>.  This is intended to
		get the process into the working directory specified in the
		user's job description file.

	fd <n> <pathname> <open_mode>
		Open the file <pathname> with the mode <open_mode>.  Set things up
		(using dup2()), so that the file is available at file descriptor
		number <n>.  This is intended for redirection of the standard
		files 0, 1, and 2.

	ckpt <pathname>
		The process should write its state information to the file
		<pathname> so that it can be restarted at a later time.
		We don't actually do a checkpoint here, we just set things
		up so that when we checkpoint, the given file name will
		be used.  The actual checkpoint is triggered by recipt of
		the signal SIGTSTP, or by the user code calling the ckpt()
		routine.

	restart <pathname>
		The process should read its state information from the file
		<pathname> and do a restart.  (In this case, main() will not
		get called, as we are jumping back to wherever the process
		left off at checkpoint time.)

	migrate_to <host_name> <port_number>
		A process on host <host_name> is listening at <port_number> for
		a TCP connection.  This process should connect to the given
		port, and write its state information onto the TCP connection
		exactly as it would for a ckpt command.  It is intended that
		the other process is running the same a.out file that this
		process is, and will use the state information to effect
		a restart on the new machine (a migration).

	migrate_from <fd>
		This process's parent (the condor_starter) has passed it an open
		file descriptor <fd> which is in reality a TCP socket.  The
		remote process will write its state information onto the TCP
		socket, which this process will use to effect a restart.  Since
		the restart is on a different machine, this is a migration.

	exit <status>
		The process should exit now with status <status>.  This
		is intended to be issued after a "ckpt" or "migrate_to"
		command.  We don't want to assume that the process
		should always exit after one of these commands because
		we want the flexibility to create interim checkpoints.
		(It's not clear why we would want to want to send a
		copy of a process to another machine and continue
		running on the current machine, but we have that
		flexibility too if we want it...)

	end
		We are temporarily at the end of commands.  Now it is time to
		call main() or effect the restart as requested.  Note that
		the process should not close the file descriptor at this point.
		Instead a signal handler will be set up to handle the signal
		SIGTSTP.  The signal handler will invoke the command stream
		interpreter again.  This is done so that the condor_starter can
		send a "ckpt" or a "migrate_to" command to the process after it
		has been running for some time.  In the case of the "ckpt"
		command the name of the file could have been known in advance, but
		in the case of the "migrate_to" command the name and port
		number of the remote host are presumably not known until
		the migration becomes necessary.
*/


#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "condor_debug.h"

enum result { NOT_OK = 0, OK = 1, END };

enum command {
	IWD,
	FD,
	CKPT,
	RESTART,
	MIGRATE_TO,
	MIGRATE_FROM,
	EXIT,
	END_MARKER,
	NO_COMMAND
};

typedef struct {
	enum command id;
	char *name;
} COMMAND;

COMMAND CmdTable[] = {
	IWD,			"iwd",
	FD,				"fd",
	CKPT,			"ckpt",
	RESTART,		"restart",
	MIGRATE_TO,		"migrate_to",
	MIGRATE_FROM,	"migrate_from",
	EXIT,			"exit",
	END_MARKER,		"end",
	NO_COMMAND,		"",
};

void _condor_interp_cmd_stream( int fd );
static void _condor_scan_cmd( char *buf, int *argc, char *argv[] );
static enum result _condor_do_cmd( int argc, char *argv[] );
static enum command _condor_find_cmd( const char *name );
static BOOLEAN condor_iwd( const char *path );
static BOOLEAN condor_fd( const char *num, const char *path, const char *open_mode );
static BOOLEAN condor_ckpt( const char *path );
static BOOLEAN condor_restart( );
static BOOLEAN condor_migrate_to( const char *host_addr, const char *port_num );
static BOOLEAN condor_migrate_from( const char *fd_no );
static BOOLEAN condor_exit( const char *status );
static int open_tcp_stream( unsigned int ip_addr, unsigned short port );
void display_ip_addr( unsigned int addr );
void open_std_file( int which );
void _condor_set_iwd();
int open_file_stream( const char *local_path, int flags, size_t *len );
int open_ckpt_file( const char *name, int flags, size_t n_bytes );
void get_ckpt_name();
extern volatile int InRestart;
void _condor_setup_dprintf();

extern int _condor_dprintf_works;
extern int	_condor_DebugFD;
static int do_remote_syscalls = 1;

int
#if defined(HPUX)
_START( int argc, char *argv[], char **envp )
#else
MAIN( int argc, char *argv[], char **envp )
#endif
{
	int		cmd_fd = -1;
	char	*cmd_name;
	char	*extra;
	int		scm;
	char	*arg;
	int		i, warning = TRUE;
	int	should_restart = FALSE;

	char	ckpt_file[_POSIX_PATH_MAX];

		/*
		  These will hold the argc and argv we actually give to the
		  user's code, with any Condor-specific flags stripped out. 
		*/
	int		user_argc;
	char*	user_argv[_POSIX_ARG_MAX];

		/* The first arg will always be the same */
	user_argv[0] = argv[0];
	user_argc = 1;

		/* Default checkpoint file is argv[0].ckpt */
	sprintf(ckpt_file,"%s.ckpt",argv[0]);

		/*
		  Now, process our command-line args to see if there are
		  any ones that begin with "-_condor" and deal with them. 
		*/
	for( i=1; (i<argc && argv[i]); i++ ) {
		if( (strncmp(argv[i], "-_condor_", 9) != MATCH ) ) {
				/* Non-Condor arg so save it. */
			user_argv[user_argc] = argv[i];
			user_argc++;
			continue;
		}

			/* 
			   This must be a Condor arg.  Let's just deal with the 
			   part of it that'll be unique...
			*/
		arg = argv[i]+9;

			/* 
			   '-_condor_cmd_fd N' is used to specify which fd our
			   command stream from the starter is on.
			*/
		if( (strcmp(arg, "cmd_fd") == MATCH) ) {
			if( ! argv[i+1] ) {
				_condor_error_fatal("Error in command line syntax");
			} else {
				i++;
				cmd_fd = strtol( argv[i], &extra, 0 );
				if( extra[0] ) {
					_condor_error_fatal("Can't parse cmd stream fd (%s)", argv[i] );
				}
			}
			continue;
		}


			/* 
			   '-_condor_cmd_file filename' is used to specify a file
			   where we're supposed to read our command stream from.
			   This doesn't seem to be used anymore, but I thought I'd
			   leave it in here just in case someone does use it, or
			   it's useful for debugging or something. -Derek 9/29/99 
			*/
		if( (strcmp(arg, "cmd_file") == MATCH) ) {
			dprintf( D_FULLDEBUG, "Found -_condor_cmd_file\n" );
			if( ! argv[i+1] ) {
				_condor_error_fatal("Error in command line syntax");
			} else {
				i++;
				cmd_fd = open( argv[i], O_RDONLY );
				if( cmd_fd < 0 ) {
					_condor_error_fatal("Can't read cmd file %s", argv[i] );
				}
			}
			continue;
		}

			/*
			  '-_condor_debug_wait' can be set to setup an infinite
			  loop so that we can attach to the user job.  This
			  argument can be specified in the submit file, but must
			  be the first argument to the job.  The argv is
			  compensated so that the job gets the vector it
			  expected.  --RR
			*/
		if( (strcmp(arg, "debug_wait") == MATCH) ) {
			int j = 1;
			while (j);
			continue;
		}

			/* 
			   '-_condor_nowarn' is only used when running a job
			   linked for Condor outside of Condor to surpress the
			   opening warning message.
			*/
		if( (strcmp(arg, "nowarn") == MATCH) ) {
			warning = FALSE;
			continue;
		}

			/*
			  '-_condor_D_XXX' can be set to add the D_XXX
			  (e.g. D_ALWAYS) debug level for this job's output.
			  This is for debug messages inside our checkpointing and
			  remote syscalls code.  This option is predominantly
			  useful for running outside of Condor, though it can
			  also be set in the submit file for jobs run inside of
			  Condor.  If this is not set, regular jobs get D_ALWAYS
			  printed into the ShadowLog on the submit machine, and
			  jobs running outside of Condor drop all dprintf()
			  messages.  If a user sets a given level, messages from
			  that level either go to the ShadowLog if we're
			  connected to a shadow, or stderr, if not.
			  -Derek Wright 9/29/99
			*/
		if( (strncmp(arg, "D_", 2) == MATCH) ) {
			_condor_set_debug_flags( arg );
			continue;
		}


			/* 
			   '-_condor_ckpt <ckpt_filename>' is used to specify the 
			   file we want to checkpoint to.
			*/
		if( (strcmp(arg, "ckpt") == MATCH) ) {
			if( ! argv[i+1] ) {
				_condor_error_fatal("-_condor_ckpt requires another argument");
			} else {
				i++;
				strcpy( ckpt_file, argv[i] );
			 }	
			continue;
		}
		
			/*
			  '-_condor_restart <ckpt_file>' is used to specify that
			  we should restart from the given checkpoint file.
			*/
		if( (strcmp(arg, "restart") == MATCH) ) {
			if( ! argv[i+1] ) {
				_condor_error_fatal("-_condor_restart requires an argument");
			} else {
				i++;
				strcpy( ckpt_file, argv[i] );
				should_restart = TRUE;
			}
			continue;
		}
		
			 /*
			   '-_condor_D_XXX' can be set to add the D_XXX
			   (e.g. D_ALWAYS) debug level for this job's output.
			   This is for debug messages inside our checkpointing and
			   remote syscalls code.  If a user sets a given level,
			   messages from that level go to stderr.  
			   -Derek Wright 9/30/99
			 */
		if( (strncmp(arg, "D_", 2) == MATCH) ) {
			_condor_set_debug_flags( arg );
			continue;
		}

			/*
			-_condor_aggravate_bugs is helpful for causing
			the file table to generate virtual fds which 
			are not the same as the real fds.
			*/

		if( (strcmp(arg, "aggravate_bugs")) == MATCH ) {
			_condor_file_table_aggravate(1);
			continue;
		}

		_condor_error_fatal("I don't understand argument %s",arg);
	}

		/*
		  We're done processing all the args, so copy the 
		  user_arg* stuff back into the real argv and argc. 
		  First, we must terminate the final element.
		*/

	user_argv[user_argc] = NULL;
	argc = user_argc;
	for( i=0; i<=argc; i++ ) {
		argv[i] = user_argv[i];
	}

	/* If a command fd is given, we are doing remote system calls. */

	if( cmd_fd>=0 ) {

		/* This is the remote syscalls startup */

		_condor_prestart( SYS_REMOTE );
		init_syscall_connection(0);
		_condor_setup_dprintf();
		_condor_set_iwd();
		_condor_interp_cmd_stream( cmd_fd );

		_condor_file_table_init();

		dprintf( D_ALWAYS | D_NOHEADER , "User Job - %s\n", CondorPlatform() );
		dprintf( D_ALWAYS | D_NOHEADER , "User Job - %s\n", CondorVersion() );
			// Also, register the version with the shadow
		REMOTE_syscall( CONDOR_register_syscall_version, CondorVersion() );

		SetSyscalls( SYS_REMOTE | SYS_MAPPED );

		get_ckpt_name();
		open_std_file( 0 );
		open_std_file( 1 );
		open_std_file( 2 );
	
	} else {

		/* This is the checkpointing-only startup */

		do_remote_syscalls = 0;

		/* Need to store the cwd in the file table */
		scm = SetSyscalls( SYS_LOCAL|SYS_MAPPED );
		chdir( getwd(0) );
		SetSyscalls( scm );

		_condor_prestart( SYS_LOCAL );

		dprintf( D_ALWAYS | D_NOHEADER , "User Job - %s\n", CondorVersion() );
		dprintf( D_ALWAYS | D_NOHEADER , "User Job - %s\n", CondorPlatform() );

		init_image_with_file_name( ckpt_file );

		if( should_restart ) {
			if( warning ) {
				fprintf( stderr, "Condor: Will restart from %s\n",ckpt_file);
			}
			restart();
		} else {
			if ( warning ) {
				fprintf( stderr, "Condor: Will checkpoint to %s\n", ckpt_file );
				fprintf( stderr, "Condor: Remote system calls disabled.\n");
			} 

			SetSyscalls( SYS_LOCAL | SYS_MAPPED );
		}
	}

	InRestart = FALSE;

	#if defined(HPUX)
		exit(_start( argc, argv, envp ));
	#else
		exit( main( argc, argv, envp ));
	#endif
}

void
_condor_interp_cmd_stream( int fd )
{
	BOOLEAN	at_end = FALSE;
	FILE	*fp = fdopen( fd, "r" );
	char	buf[1024];
	int		argc;
	char	*argv[256];
	int		scm;

	while( fgets(buf,sizeof(buf),fp) ) {
		_condor_scan_cmd( buf, &argc, argv );
		switch( _condor_do_cmd(argc,argv) ) {
		  case OK:
			break;
		  case NOT_OK:
			dprintf( D_ALWAYS, "?\n" );
			break;
		  case END:
			return;
		}
	}
	_condor_error_retry("EOF on command stream");
}

static void
_condor_scan_cmd( char *buf, int *argc, char *argv[] )
{
	int		i;

	argv[0] = strtok( buf, " \n" );
	if( argv[0] == NULL ) {
		*argc = 0;
		return;
	}

	for( i = 1; argv[i] = strtok(NULL," \n"); i++ )
		;
	*argc = i;
}


static enum result
_condor_do_cmd( int argc, char *argv[] )
{
	if( argc == 0 ) {
		return FALSE;
	}

	switch( _condor_find_cmd(argv[0]) ) {
	  case END_MARKER:
		return END;
	  case IWD:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_iwd( argv[1] );
	  case FD:
		assert( argc == 4 );
		return condor_fd( argv[1], argv[2], argv[3] );
	  case RESTART:
		if( argc != 1 ) {
			return FALSE;
		}
		return condor_restart();
	  case CKPT:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_ckpt( argv[1] );
	  case MIGRATE_TO:
		if( argc != 3 ) {
			return FALSE;
		}
		return condor_migrate_to( argv[1], argv[2] );
	  case MIGRATE_FROM:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_migrate_from( argv[1] );
	  case EXIT:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_exit( argv[1] );
	  default:
		return FALSE;
	}
}

static enum command
_condor_find_cmd( const char *str )
{
	COMMAND	*ptr;

	for( ptr = CmdTable; ptr->id != NO_COMMAND; ptr++ ) {
		if( strcmp(ptr->name,str) == MATCH ) {
			return ptr->id;
		}
	}
	return NO_COMMAND;
}

static BOOLEAN
condor_iwd( const char *path )
{
#if 0
	dprintf( D_ALWAYS, "condor_iwd: path = \"%s\"\n", path );
	delay();
#endif
	REMOTE_syscall( CONDOR_chdir, path );
	return TRUE;
}

static BOOLEAN
condor_fd( const char *num, const char *path, const char *open_mode )
{
	/* no longer used  - ignore */
	return TRUE;
}

static BOOLEAN
condor_ckpt( const char *path )
{
	dprintf( D_ALWAYS, "condor_ckpt: filename = \"%s\"\n", path );
	init_image_with_file_name( path );

	return TRUE;
}


static BOOLEAN
condor_restart()
{
	int		fd;
	size_t	n_bytes;

	dprintf( D_ALWAYS, "condor_restart:\n" );
	get_ckpt_name();
	restart();

		/* Can never get here - restart() jumps back into user code */
	return FALSE;
}

static BOOLEAN
condor_migrate_to( const char *host_name, const char *port_num )
{
	char 	*extra;
	long	port;

	port = strtol( port_num, &extra, 0 );
	if( extra[0] ) {
		return FALSE;
	}

	dprintf( D_FULLDEBUG,
		"condor_migrate_to: host = \"%s\", port = %d\n", host_name, port
	);
	return TRUE;
}

static BOOLEAN
condor_migrate_from( const char *fd_no )
{
	long	fd;
	char	*extra;

	fd = strtol( fd_no, &extra, 0 );
	if( extra[0] ) {
		return FALSE;
	}
	
	dprintf( D_FULLDEBUG, "condor_migrate_from: fd = %d\n", fd );
	return TRUE;
}

static BOOLEAN
condor_exit( const char *status )
{
	long	st;
	char	*extra;

	st = strtol( status, &extra, 0 );
	if( extra[0] ) {
		return FALSE;
	}
	
	dprintf( D_FULLDEBUG, "condor_exit: status = %d\n", st );
	return TRUE;
}


/*
  Open a stream for writing our checkpoint information.  Since we are in
  the "remote" startup file, this is the remote version.  We do it with
  a "pseudo system call" to the shadow.
*/
int
open_ckpt_file( const char *name, int flags, size_t n_bytes )
{
	char			file_name[ _POSIX_PATH_MAX ];
	int				status;

	if( !do_remote_syscalls ) {
		if( flags & O_WRONLY ) {
			return open( name, O_CREAT | O_TRUNC | O_WRONLY, 0664 );
		} else {
			return open( name, O_RDONLY );
		}
	} else {
		return open_file_stream( name, flags, &n_bytes );
	}
}

void
report_image_size( int kbytes )
{
	if( !do_remote_syscalls ) {
		return;
	} else {
		REMOTE_syscall( CONDOR_image_size, kbytes );
	}
}

/*
  After we have updated our image size and rusage, we ask the shadow
  for a bitmask which specifies checkpointing options, defined in
  condor_includes/condor_ckpt_mode.h.  A return of -1 or 0 signifies
  that all default values should be used.  Thus, if the shadow does not
  support this call, the job will checkpoint with default options.  The
  job sends the signal which triggered the checkpoint so the shadow
  knows if this is a periodic or vacate checkpoint.
*/
int
get_ckpt_mode( int sig )
{
	if( !do_remote_syscalls ) {
		return 0;
	} else {
		return REMOTE_syscall( CONDOR_get_ckpt_mode, sig );
	}
}

/*
  If the shadow tells us to checkpoint slowly in get_ckpt_mode(), we need
  to ask for a speed.  The return value is in KB/s.
*/
int
get_ckpt_speed()
{
	if( !do_remote_syscalls ) {
		return 0;
	} else {
		return REMOTE_syscall( CONDOR_get_ckpt_speed );
	}
}

/*
  dprintf uses _set_priv, but it is a noop in the user job, so we just
  include a dummy stub here to avoid bringing in more junk from the Condor
  libs.
*/
int
_set_priv()
{
	return 0;
}

#define UNIT 10000

#if defined(ALPHA)
#	define LIM (2000 * UNIT)
#elif defined(AIX32)
#	define LIM (225 * UNIT)
#elif defined(SPARC)
#	define LIM (260 * UNIT)
#elif defined(ULTRIX43)
#	define LIM (170 * UNIT)
#elif defined(HPPAR)
#	define LIM (260 * UNIT)
#elif defined(LINUX)
#	define LIM (200 * UNIT)
#elif defined(Solaris)
#	define LIM (260 * UNIT)
#elif defined(SGI)
#	define LIM (200 * UNIT)
#endif

#if 1
	delay()
	{
		int		i;

		for( i=0; i<LIM; i++ )
			;
	}
#else
	delay(){}
#endif
#define B_NET(x) (((long)(x)&IN_CLASSB_NET)>>IN_CLASSB_NSHIFT)
#define B_HOST(x) ((long)(x)&IN_CLASSB_HOST)
#define HI(x) (((long)(x)&0xff00)>>8)
#define LO(x) ((long)(x)&0xff)


void
display_ip_addr( unsigned int addr )
{
	int		net_part;
	int		host_part;

	if( IN_CLASSB(addr) ) {
		net_part = B_NET(addr);
		host_part = B_HOST(addr);
		dprintf( D_FULLDEBUG, "%d.%d", HI(B_NET(addr)), LO(B_NET(addr)) );
		dprintf( D_FULLDEBUG, ".%d.%d\n", HI(B_HOST(addr)), LO(B_HOST(addr)) );
	} else {
		dprintf( D_FULLDEBUG, "0x%x\n", addr );
	}
}

/*
  Open a standard file (0, 1, or 2), given its fd number.
*/
void
open_std_file( int fd )
{
	char	logical_name[ _POSIX_PATH_MAX ];
	int	result;
	int	new_fd;
	int	flags;

	result = REMOTE_syscall(CONDOR_get_std_file_info,fd,logical_name);
	if(result==0) {
		close(fd);
		if(fd==0) {
			flags = O_RDONLY;
		} else {
			flags = O_WRONLY;
		}
		new_fd = open(logical_name,flags,0);
		if(new_fd<0) {
			_condor_error_retry("Couldn't open standard file '%s'", logical_name );
		}
		if(new_fd!=fd) {
			dup2(fd,new_fd);
		}
	} else {
		_condor_error_fatal("Couldn't get info on standard file %d",fd );
	}
}

void
_condor_set_iwd()
{
	char	iwd[ _POSIX_PATH_MAX ];
	char	buf[ _POSIX_PATH_MAX + 50 ];
	int	scm;

	if( REMOTE_syscall(CONDOR_get_iwd,iwd) < 0 ) {
		_condor_error_fatal( "Can't determine initial working directory" );
	}
	scm = SetSyscalls( SYS_REMOTE|SYS_MAPPED );
	if( chdir(iwd) < 0 ) {
		_condor_error_retry( "Can't move to directory %s", iwd );
	}
	SetSyscalls(scm);
}

void
get_ckpt_name()
{
	char	ckpt_name[ _POSIX_PATH_MAX ];
	int		status;

	status = REMOTE_syscall( CONDOR_get_ckpt_name, ckpt_name );
	if( status < 0 ) {
		_condor_error_fatal("Can't get checkpoint file name!\n");
	}
	dprintf( D_ALWAYS, "Checkpoint file name is \"%s\"\n", ckpt_name );
	init_image_with_file_name( ckpt_name );
}

void
_condor_setup_dprintf()
{
	if( ! DebugFlags ) {
			// If it hasn't already been set, give a default, so we
			// still get dprintf() if we're running in Condor.
		DebugFlags = D_ALWAYS | D_NOHEADER;
	}

		// Now, initialize what FD we print to.  If we got to this
		// function, we want to use the socket back to the shadow. 
	_condor_DebugFD = CLIENT_LOG;

	_condor_dprintf_works = 1;
}
