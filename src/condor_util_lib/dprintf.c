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

 


/************************************************************************
**
**	Generic logging function.  Prints the message on DebugFP with a date if
**	any bits in "flags" are set in DebugFlags.  If locking is desired,
**	DebugLock should contain the name of the lock file.  If log length
**	management is desired, MaxLog should contain the maximum length of the
**	log in bytes.  (The log will be copied to "DebugFile.old", so MaxLog
**	should be half of the space you are willing to devote.  If both log 
**	length management and locking are desired, the lock file should not be
**	the same as the log file.  Along with the date, other identifying
**	information can be logged with the message by supplying the function
**	(*DebugId)() which takes DebugFP as an argument.
**
************************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "except.h"
#include "exit.h"
#include "condor_uid.h"
#include "basename.h"
#include "get_mysubsystem.h"

FILE *debug_lock(int debug_level);
FILE *open_debug_file( int debug_level, char flags[] );
void debug_unlock(int debug_level);
void preserve_log_file(int debug_level);
void _condor_dprintf_exit( int error_code, const char* msg );
void _condor_set_debug_flags( char *strflags );
int _condor_mkargv( int* argc, char* argv[], char* line );


extern	DLL_IMPORT_MAGIC int		errno;
extern	int		DebugFlags;


FILE	*DebugFP = 0;

/*
** These arrays must be D_NUMLEVELS+1 in size since we can have a
** debug file for each level plus an additional catch-all debug file
** at index 0.
*/
int		MaxLog[D_NUMLEVELS+1] = { 0 };
char	*DebugFile[D_NUMLEVELS+1] = { NULL };
char	*DebugLock = NULL;

int		(*DebugId)(FILE *);
int		SetSyscalls(int mode);

int		LockFd = -1;

static	int DprintfBroken = 0;
static	int DebugUnlockBroken = 0;
#ifdef WIN32
static CRITICAL_SECTION	*_condor_dprintf_critsec = NULL;
#endif

extern char *_condor_DebugFlagNames[];

/*
** Note: setting this to true will avoid blocking signal handlers from running
** while we are printing log messages.  It's probably a good idea to block
** them as they may get into trouble with manipulating the lock on the
** log file.  However blocking them will cause many implementations of
** dbx to hang.
*/
int InDBX = 0;

/*
** Print a nice log message, but only if "flags" are included in the
** current debugging flags.
*/
/* VARARGS1 */

void
_condor_dprintf_va( int flags, char* fmt, va_list args )
{
	struct tm *tm, *localtime();
	time_t clock;
#if !defined(WIN32)
	sigset_t	mask, omask;
	mode_t		old_umask;
#endif
	int saved_errno;
	int	saved_flags;
	priv_state	priv;
	int debug_level;

		/* DebugFP should be static initialized to stderr,
	 	   but stderr is not a constant on all systems. */
	if( !DebugFP ) DebugFP = stderr;

		/* If we hit some fatal error in dprintf, this flag is set.
		   If dprintf is broken and someone (like _EXCEPT_Cleanup)
		   trys to dprintf, we just return to avoid infinite loops. */
	if( DprintfBroken ) return;

		/* See if this is one of the messages we are logging */
	if( !(flags&DebugFlags) ) {
		return;
	}

#ifdef WIN32

	// DaemonCore Create_Thread creates a real kernel thread on Win32,
	// and dprintf is not thread-safe.  So, until such time that
	// dprint is made thread safe, on Win32 we restrict access to one
	// thread at a time via a critical section.  NOTE: we must enter
	// the critical section _after_ we test DprintfBroken above,
	// otherwise we could hang forever if _condor_dprintf_exit() is
	// called and an EXCEPT handler tries to use dprintf.
	if ( _condor_dprintf_critsec == NULL ) {
		_condor_dprintf_critsec = 
			(CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
		InitializeCriticalSection(_condor_dprintf_critsec);
	}
	EnterCriticalSection(_condor_dprintf_critsec);
#endif

#if !defined(WIN32) /* signals and umasks don't exist in WIN32 */

	/* Block any signal handlers which might try to print something */
	sigfillset( &mask );
	sigdelset( &mask, SIGABRT );
	sigdelset( &mask, SIGBUS );
	sigdelset( &mask, SIGFPE );
	sigdelset( &mask, SIGILL );
	sigdelset( &mask, SIGSEGV );
	sigdelset( &mask, SIGTRAP );
	sigprocmask( SIG_BLOCK, &mask, &omask );

		/* Make sure our umask is reasonable, in case we're the shadow
		   and the remote job has tried to set its umask or
		   something.  -Derek Wright 6/11/98 */
	old_umask = umask( 022 );

#endif

	saved_errno = errno;

	saved_flags = DebugFlags;       /* Limit recursive calls */
	DebugFlags = 0;


	/* log files owned by condor system acct */

		/* If we're in PRIV_USER_FINAL, there's a good chance we won't
		   be able to write to the log file.  We can't rely on Condor
		   code to refrain from calling dprintf() after switching to
		   PRIV_USER_FINAL.  So, we check here and simply don't try to
		   log anything when we're in PRIV_USER_FINAL, to avoid
		   exit(DPRINTF_ERROR). */
	if (get_priv() == PRIV_USER_FINAL) return;

		/* avoid priv macros so we can bypass priv logging */
	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

		/* Grab the time info only once, instead of inside the for
		   loop.  -Derek 9/14 */
	(void)time(  (time_t *)&clock );
	tm = localtime( (time_t *)&clock );

		/* print debug message to catch-all debug file plus files */
		/* registered for other debug levels */
	for (debug_level = 0; debug_level <= D_NUMLEVELS; debug_level++) {
		if ((debug_level == 0) ||
			(DebugFile[debug_level] && (flags&(1<<(debug_level-1))))) {

				/* Open and lock the log file */
			(void)debug_lock(debug_level);

			if (DebugFP) {

				/* Print the message with the time and a nice identifier */
				if( ((saved_flags|flags) & D_NOHEADER) == 0 ) {
					fprintf( DebugFP, "%d/%d %02d:%02d:%02d ", 
							 tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, 
							 tm->tm_min, tm->tm_sec );

					if ( (saved_flags|flags) & D_FDS ) {
						fprintf ( DebugFP, "(fd:%d) ", fileno(DebugFP) );
					}

					if( DebugId ) {
						(*DebugId)( DebugFP );
					}
				}

				vfprintf( DebugFP, fmt, args );

			}

			/* Close and unlock the log file */
			debug_unlock(debug_level);

		}
	}

		/* restore privileges */
	_set_priv(priv, __FILE__, __LINE__, 0);

	errno = saved_errno;
	DebugFlags = saved_flags;

#if !defined(WIN32) // signals and umasks don't exist in WIN32

		/* restore umask */
	(void)umask( old_umask );

		/* Let them signal handlers go!! */
	(void) sigprocmask( SIG_SETMASK, &omask, 0 );

#endif

#ifdef WIN32
	LeaveCriticalSection(_condor_dprintf_critsec);
#endif

}


FILE *
debug_lock(int debug_level)
{
	int			length, save_errno, retry = 0;
	priv_state	priv;
	char*		dirpath = NULL;
	char msg_buf[_POSIX_PATH_MAX];

	if ( DebugFP == NULL ) {
		DebugFP = stderr;
	}

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

		/* Acquire the lock */
	if( DebugLock ) {
		if( LockFd < 0 ) {
			LockFd = open(DebugLock,O_CREAT|O_WRONLY,0660);
			if( LockFd < 0 ) {
				save_errno = errno;
				if( save_errno == ENOENT ) {
						/* 
						   No directory: Try to create the directory
						   itself, first as condor, then as root.  If
						   we created it as root, we need to try to
						   chown() it to condor.
						*/ 
					dirpath = dirname( DebugLock );
					errno = 0;
					if( mkdir(dirpath, 0777) < 0 ) {
						if( errno == EACCES ) {
								/* Try as root */ 
							_set_priv(PRIV_ROOT, __FILE__, __LINE__, 0);
							if( mkdir(dirpath, 0777) < 0 ) {
								/* We failed, we're screwed */
								fprintf( stderr, "Can't create lock directory \"%s\", "
										 "errno: %d (%s)\n", dirpath, errno, 
 										 strerror(errno) );
							} else {
								/* It worked as root, so chown() the
								   new directory and set a flag so we
								   retry the open(). */
#ifndef WIN32
								chown( dirpath, get_condor_uid(),
									   get_condor_gid() );
#endif
								retry = 1;
							}
							_set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);
						} else {
								/* Some other error than access, give up */ 
							fprintf( stderr, "Can't create lock directory: \"%s\""
									 "errno: %d (%s)\n", dirpath, errno, 
									 strerror(errno) );							
						}
					} else {
							/* We succeeded in creating the directory,
							   try the open() again */
						retry = 1;
					}
						/* At this point, we're done with this, so
						   don't leak it. */
					free( dirpath );
				}
				if( retry ) {
					LockFd = open(DebugLock,O_CREAT|O_WRONLY,0660);
				}
				if( LockFd < 0 ) {
					sprintf( msg_buf, "Can't open \"%s\"\n", DebugLock );
					_condor_dprintf_exit( save_errno, msg_buf );
				}
			}
		}

		errno = 0;
#ifdef WIN32
		if( lock_file(LockFd,WRITE_LOCK,TRUE) < 0 ) 
#else
		if( flock(LockFd,LOCK_EX) < 0 ) 
#endif
		{
			save_errno = errno;
			sprintf( msg_buf, "Can't get exclusive lock on \"%s\", "
					 "LockFd: %d\n", DebugLock, LockFd );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
	}

	if( DebugFile[debug_level] ) {
		errno = 0;

		DebugFP = open_debug_file(debug_level, "a");

		if( DebugFP == NULL ) {
			if (debug_level > 0) return NULL;
#if !defined(WIN32)
			save_errno = errno;
			if( errno == EMFILE ) {
				_condor_fd_panic( __LINE__, __FILE__ );
			}
#endif
			sprintf( msg_buf, "Could not open DebugFile \"%s\"\n", 
					 DebugFile[debug_level] );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
			/* Seek to the end */
		if( (length=lseek(fileno(DebugFP),0,2)) < 0 ) {
			if (debug_level > 0) {
				fclose( DebugFP );
				DebugFP = NULL;
				return NULL;
			}
			save_errno = errno;
			sprintf( msg_buf, "Can't seek to end of DebugFP file\n" );
			_condor_dprintf_exit( save_errno, msg_buf );
		}

			/* If it's too big, preserve it and start a new one */
		if( MaxLog[debug_level] && length > MaxLog[debug_level] ) {
			fprintf( DebugFP, "MaxLog = %d, length = %d\n",
					 MaxLog[debug_level], length );
			preserve_log_file(debug_level);
		}
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	return DebugFP;
}

void
debug_unlock(int debug_level)
{
	priv_state priv;
	char msg_buf[_POSIX_PATH_MAX];
	int flock_errno = 0;

	if( DebugUnlockBroken ) {
		return;
	}

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	if (DebugFP) (void)fflush( DebugFP );

	if( DebugLock ) {
			/* Don't forget to unlock the file */
		errno = 0;
#if defined(WIN32)
		if ( lock_file(LockFd,UN_LOCK,TRUE) < 0 )
#else
		if( flock(LockFd,LOCK_UN) < 0 ) 
#endif
		{
			flock_errno = errno;
			sprintf( msg_buf, "Can't release exclusive lock on \"%s\"\n", 
					 DebugLock );
			DebugUnlockBroken = 1;
			_condor_dprintf_exit( flock_errno, msg_buf );
		}
	}

	if( DebugFile[debug_level] ) {
		if (DebugFP) (void)fclose( DebugFP );
		DebugFP = NULL;
	}

	_set_priv(priv, __FILE__, __LINE__, 0);
}


/*
** Copy the log file to a backup, then truncate the current one.
*/
void
preserve_log_file(int debug_level)
{
	char		old[MAXPATHLEN + 4];
	priv_state	priv;
	int			still_in_old_file = FALSE;
	int			failed_to_rotate = FALSE;
	int			save_errno;
	struct stat buf;
	char msg_buf[_POSIX_PATH_MAX];

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	(void)sprintf( old, "%s.old", DebugFile[debug_level] );
	fprintf( DebugFP, "Saving log file to \"%s\"\n", old );
	(void)fflush( DebugFP );

	fclose( DebugFP );
	DebugFP = NULL;

	unlink(old);

#if defined(WIN32)

	/* use rename on WIN32, since link isn't available */
	if (rename(DebugFile[debug_level], old) < 0) {
		/* the rename failed, perhaps one of the log files
		 * is currently open.  Sleep a half second and try again. */		 
		Sleep(500);
		unlink(old);
		if ( rename(DebugFile[debug_level],old) < 0) {
			/* Crap.  Some bonehead must be keeping one of the files
			 * open for an extended period.  Win32 will not permit an
			 * open file to be unlinked or renamed.  So, here we copy
			 * the file over (instead of renaming it) and then truncate
			 * our original. */

			if ( CopyFile(DebugFile[debug_level],old,FALSE) == 0 ) {
				/* Even our attempt to copy failed.  We're screwed. */
				failed_to_rotate = TRUE;
			}

			/* now truncate the original by reopening _not_ with append */
			DebugFP = open_debug_file(debug_level, "w");
			if ( DebugFP ==  NULL ) {
				still_in_old_file = TRUE;
			}
		}
	}

#else

	/* 
	** for some still unknown reason, newer versions of OSF/1 (Digital Unix)
    ** sometimes do not rotate the log correctly using 'rename'.  Changing
	** this to a unlink/link/unlink sequence seems to work.  May as well do
	** it for all platforms as there is no need for atomicity.  --RR
    */

	errno = 0;
	if( link(DebugFile[debug_level], old) < 0 ) {
		save_errno = errno;
		sprintf( msg_buf, "Can't link(%s,%s)\n",
				 DebugFile[debug_level], old );
		_condor_dprintf_exit( save_errno, msg_buf );
	}

	errno = 0;
	if( unlink(DebugFile[debug_level]) < 0 ) {
		save_errno = errno;
		sprintf( msg_buf, "Can't unlink(%s)\n", DebugFile[debug_level] ); 
		_condor_dprintf_exit( save_errno, msg_buf );
	}

	/* double check the result of the rename */
	{
		errno = 0;
		if (stat (DebugFile[debug_level], &buf) >= 0)
		{
			save_errno = errno;
			sprintf( msg_buf, "unlink(%s) succeeded but file still exists!", 
					 DebugFile[debug_level] );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
	}

#endif

	if (DebugFP == NULL) {
		DebugFP = open_debug_file(debug_level, "a");
	}

	if( DebugFP == NULL ) {
		save_errno = errno;
		sprintf( msg_buf, "Can't open file for debug level %d\n",
				 debug_level ); 
		_condor_dprintf_exit( save_errno, msg_buf );
	}

	if ( !still_in_old_file ) {
		fprintf (DebugFP, "Now in new log file %s\n", DebugFile[debug_level]);
	}

	if ( failed_to_rotate ) {
		fprintf(DebugFP,"ERROR: Failed to rotate log into file %s!\n",old);
		fprintf(DebugFP,"       Perhaps someone is keeping log files open???");
	}

	_set_priv(priv, __FILE__, __LINE__, 0);
}


#if !defined(WIN32)
/*
** Can't open log or lock file becuase we are out of fd's.  Try to let
** somebody know what happened.
*/
void
_condor_fd_panic( int line, char* file )
{
	priv_state	priv;
	int i;
	char msg_buf[_POSIX_PATH_MAX];
	char panic_msg[_POSIX_PATH_MAX];
	int save_errno;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	sprintf( panic_msg, 
			 "**** PANIC -- OUT OF FILE DESCRIPTORS at line %d in %s",
			 line, file );

		/* Just to be extra paranoid, let's nuke a bunch of fds. */
	for ( i=0 ; i<50 ; i++ ) {
		(void)close( i );
	}
	if( DebugFile[0] ) {
		DebugFP = fopen(DebugFile[0], "a");
	}

	if( DebugFP == NULL ) {
		save_errno = errno;
		sprintf( msg_buf, "Can't open \"%s\"\n%s\n", DebugFile[0],
				 panic_msg ); 
		_condor_dprintf_exit( save_errno, msg_buf );
	}
		/* Seek to the end */
	(void)lseek( fileno(DebugFP), 0, 2 );
	fprintf( DebugFP, "%s\n", panic_msg );
	(void)fflush( DebugFP );

	_condor_dprintf_exit( 0, panic_msg );
}
#endif
	

#ifdef NOTDEF
void tzset(){}
extern char	**environ;

char *
_getenv( name )
char	*name;
{
	char	**envp;
	char	*p1, *p2;

	for( envp = environ; *envp; envp++ ) {
		for( p1 = name, p2 = *envp; *p1 && *p2 && *p1 == *p2; p1++, p2++ )
			;
		if( *p1 == '\0' ) {
			return p2;
		}
	}
	return (char *)0;
}

char *
getenv( name )
char	*name;
{
	return _getenv(name);
}

sigset(){}
#endif

FILE *
open_debug_file(int debug_level, char flags[])
{
	FILE		*fp;
	priv_state	priv;
	char msg_buf[_POSIX_PATH_MAX];
	int save_errno;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	/* Note: The log file shouldn't need to be group writeable anymore,
	   since PRIV_CONDOR changes euid now. */

	errno = 0;
	if( (fp=fopen(DebugFile[debug_level],flags)) == NULL ) {
		save_errno = errno;
#if !defined(WIN32)
		if( errno == EMFILE ) {
			_condor_fd_panic( __LINE__, __FILE__ );
		}
#endif
		if (DebugFP == 0) {
			DebugFP = stderr;
		}
		fprintf( DebugFP, "Can't open \"%s\"\n", DebugFile[debug_level] );
		if( debug_level == 0 ) {
			sprintf( msg_buf, "Can't open \"%s\"\n",
					 DebugFile[debug_level] );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
		return NULL;
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	return fp;
}

/* dprintf() hit some fatal error and is going to exit. */
void
_condor_dprintf_exit( int error_code, const char* msg )
{
	char* tmp;
	FILE* fail_fp;
	char buf[_POSIX_PATH_MAX];
	char header[_POSIX_PATH_MAX];
	char tail[_POSIX_PATH_MAX];
	int wrote_warning = FALSE;
	struct tm *tm;
	time_t clock;

	(void)time( (time_t *)&clock );
	tm = localtime( (time_t *)&clock );

	sprintf( header, "%d/%d %02d:%02d:%02d "
			 "dprintf() had a fatal error in pid %d\n", 
			 tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, 
			 tm->tm_min, tm->tm_sec, (int)getpid() );
	tail[0] = '\0';
	if( error_code ) {
		sprintf( tail, "errno: %d (%s)\n", error_code,
				 strerror(error_code) );
	}
#ifndef WIN32			
	sprintf( buf, "euid: %d, ruid: %d\n", (int)geteuid(),
			 (int)getuid() );
	strcat( tail, buf );
#endif

	tmp = param( "LOG" );
	if( tmp ) {
		sprintf( buf, "%s/dprintf_failure.%s", tmp, get_mySubSystem() );
		fail_fp = fopen( buf, "w" );
		if( fail_fp ) {
			fprintf( fail_fp, "%s", header );
			fprintf( fail_fp, "%s", msg );
			if( tail[0] ) {
				fprintf( fail_fp, "%s", tail );
			}
			fclose( fail_fp );
			wrote_warning = TRUE;
		} 
		free( tmp );
	}
	if( ! wrote_warning ) {
		fprintf( stderr, "%s", header );
		fprintf( stderr, "%s", msg );
		if( tail[0] ) {
			fprintf( stderr, "%s", tail );
		}

	}

		/* First, set a flag so we know not to try to keep using
		   dprintf during the rest of this */
	DprintfBroken = 1;

		/* Don't forget to unlock the log file, if possible! */
	debug_unlock(0);

		/* If _EXCEPT_Cleanup is set for cleaning up during EXCEPT(),
		   we call that here, as well. */
	if( _EXCEPT_Cleanup ) {
		(*_EXCEPT_Cleanup)( __LINE__, errno, "dprintf hit fatal errors\n" );
	}

		/* Actually exit now */
	fflush (stderr);

	exit(DPRINTF_ERROR); 
}


/*
  We want these all to have _condor in front of them for inside the
  user job, but the rest of the Condor code just calls the regular
  versions.  So, we'll just call the "safe" version so we can share
  the code in both places. -Derek Wright 9/29/99
*/
void
set_debug_flags( char *strflags )
{
	_condor_set_debug_flags( strflags );
}


int
mkargv( int* argc, char* argv[], char* line )
{
	return( _condor_mkargv(argc, argv, line) );
}
