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
#include "condor_sys.h"
#include "condor_debug.h"
#include "clib.h"
#include "except.h"
#include "condor_uid.h"


FILE *debug_lock(int debug_level);
FILE *open_debug_file( int debug_level, char flags[] );
void debug_unlock(int debug_level);
void preserve_log_file(int debug_level);
void dprintf_exit();

extern int	errno;

int		DebugFlags = D_ALWAYS;
FILE	*DebugFP = stderr;

/*
** These arrays must be D_NUMLEVELS+1 in size since we can have a
** debug file for each level plus an additional catch-all debug file
** at index 0.
*/
int		MaxLog[D_NUMLEVELS+1] = { 0 };
char	*DebugFile[D_NUMLEVELS+1] = { NULL };
char	*DebugLock[D_NUMLEVELS+1] = { NULL };

int		(*DebugId)(FILE *);
int		SetSyscalls(int mode);

#if defined(WIN32)
HANDLE	LockHandle = INVALID_HANDLE_VALUE;
#else
int		LockFd = -1;
#endif

static	int DprintfBroken = 0;
static	int DebugUnlockBroken = 0;

static char _FileName_[] = __FILE__;

char *DebugFlagNames[] = {
	"D_ALWAYS", "D_SYSCALLS", "D_CKPT", "D_XDR", "D_MALLOC", "D_LOAD",
	"D_EXPR", "D_PROC", "D_JOB", "D_MACHINE", "D_FULLDEBUG", "D_NFS",
	"D_UPDOWN", "D_AFS", "D_PREEMPT", "D_PROTOCOL",	"D_PRIV",
	"D_TAPENET", "D_DAEMONCORE", "D_COMMAND", "D_BANDWIDTH", "D_NETWORK",
	"D_KEYBOARD", "D_PROCFAMILY", "D_UNDEF24", "D_UNDEF25", "D_UNDEF26",
	"D_UNDEF27", "D_UNDEF28", "D_FDS", "D_SECONDS", "D_NOHEADER",
};

#if !defined(WIN32)	// Need to port this to WIN32.  It is used when logging to a socket.
/*
**	Initialize the DebugFP to a specific file number.
*/
void
dprintf_init( int fd )
{
	FILE *fp;
	int		tmp_errno;

	errno = 0;
	fp = fdopen( fd, "a" );
	tmp_errno = errno;

	if( fp != NULL ) {
		DebugFP = fp;
	} else {
		fprintf(stderr, "dprintf_init: failed to fdopen(%d)\n", fd );
		dprintf_exit();
	}
}
#endif

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
	int scm;
#if !defined(WIN32)
	sigset_t	mask, omask;
	mode_t		old_umask;
#endif
	int saved_errno;
	int	saved_flags;
	priv_state	priv;
	int debug_level;


		/* If we hit some fatal error in dprintf, this flag is set.
		   If dprintf is broken and someone (like _EXCEPT_Cleanup)
		   trys to dprintf, we just return to avoid infinite loops. */
	if( DprintfBroken ) return;

		/* See if this is one of the messages we are logging */
	if( !(flags&DebugFlags) ) {
		return;
	}

	saved_errno = errno;

	saved_flags = DebugFlags;       /* Limit recursive calls */
	DebugFlags = 0;

	/*
	When talking to the debug fd, you are talking to an fd 
	that is not visible to the user.  It is not entered in the
	virtual file table, and, hence, you should be in unmapped mode.
	*/

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

#if !defined(WIN32) /* signals and umasks don't exist in WIN32 */

	/* Block any signal handlers which might try to print something */
	sigfillset( &mask );
	sigdelset( &mask, SIGABRT );
	sigdelset( &mask, SIGBUS );
	sigdelset( &mask, SIGFPE );
	sigdelset( &mask, SIGILL );
	sigdelset( &mask, SIGQUIT );
	sigdelset( &mask, SIGSEGV );
	sigdelset( &mask, SIGTRAP );
	sigdelset( &mask, SIGCHLD );
	sigprocmask( SIG_BLOCK, &mask, &omask );

		/* Make sure our umask is reasonable, in case we're the shadow
		   and the remote job has tried to set its umask or
		   something.  -Derek Wright 6/11/98 */
	old_umask = umask( 022 );

#endif

	/* log files owned by condor system acct */
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

			/* Print the message with the time and a nice identifier */
			if( ((saved_flags|flags) & D_NOHEADER) == 0 ) {
				if( (saved_flags|flags) & D_SECONDS ) {
					fprintf( DebugFP, "%d/%d %02d:%02d:%02d ", 
							 tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, 
							 tm->tm_min, tm->tm_sec );
				} else {
					fprintf( DebugFP, "%d/%d %02d:%02d ", tm->tm_mon + 1,
						 tm->tm_mday, tm->tm_hour, tm->tm_min );
				}

				if ( (saved_flags|flags) & D_FDS ) {
					fprintf ( DebugFP, "(fd:%d) ", fileno(DebugFP) );
				}

				if( DebugId ) {
					(*DebugId)( DebugFP );
				}
			}

			vfprintf( DebugFP, fmt, args );

			/* Close and unlock the log file */
			debug_unlock(debug_level);

		}
	}

		/* restore privileges */
	_set_priv(priv, __FILE__, __LINE__, 0);

#if !defined(WIN32) // signals and umasks don't exist in WIN32

		/* restore umask */
	(void)umask( old_umask );

		/* Let them signal handlers go!! */
	(void) sigprocmask( SIG_SETMASK, &omask, 0 );

#endif

	(void) SetSyscalls( scm );

	errno = saved_errno;
	DebugFlags = saved_flags;
}


void
dprintf(int flags, char* fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, fmt, args );
    va_end( args );
}


FILE *
debug_lock(int debug_level)
{
	int			length;
	priv_state	priv;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

		/* Acquire the lock */
	if( DebugLock[debug_level] ) {
#if defined(WIN32)
		if( LockHandle == INVALID_HANDLE_VALUE ) {
			/* open file with exclusive access (no sharing) */
			/* what happens when we fail to get the exclusive lock? */
			LockHandle = CreateFile(DebugLock[debug_level], GENERIC_WRITE,
									0, 0, OPEN_ALWAYS,
									FILE_ATTRIBUTE_NORMAL, 0);
			if( LockHandle == INVALID_HANDLE_VALUE ) {
				fprintf( DebugFP, "Can't open \"%s\", errno = %d\n",
						 DebugLock[debug_level], errno );
				dprintf_exit();
			}
		}
#else
		if( LockFd < 0 ) {
			LockFd = open(DebugLock[debug_level],O_CREAT|O_WRONLY,0660);
			if( LockFd < 0 ) {
					/* 
					   if( errno == ENOENT ) { ...

					   We should try creating the parent directory,
					   both as condor, and as root.  If we can only
					   create it as root, we should set the ownership
					   back to condor when we're done.
					*/
				fprintf( stderr, "Can't open \"%s\", errno: %d (%s)\n",
						 DebugLock[debug_level], errno, strerror(errno) );
				dprintf_exit();

			}
		}

		if( flock(LockFd,LOCK_EX) < 0 ) {
			fprintf( stderr, "Can't get exclusive lock on \"%s\"\n",
					 DebugLock);
			dprintf_exit();
		}
#endif
	}

	if( DebugFile[debug_level] ) {
		errno = 0;

		DebugFP = open_debug_file(debug_level, "a");

		if( DebugFP == NULL ) {
#if !defined(WIN32)
			if( errno == EMFILE ) {
				fd_panic( __LINE__, __FILE__ );
			}
#endif
			fprintf(stderr, "Could not open DebugFile <%s>\n", DebugFile);
			dprintf_exit();
		}
			/* Seek to the end */
		if( (length=lseek(fileno(DebugFP),0,2)) < 0 ) {
			fprintf( DebugFP, "Can't seek to end of DebugFP file\n" );
			dprintf_exit();
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

	if( DebugUnlockBroken ) {
		return;
	}

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	(void)fflush( DebugFP );

	if( DebugLock[debug_level] ) {
			/* Don't forget to unlock the file */
#if defined(WIN32)
		if( CloseHandle(LockHandle) == 0) {
			fprintf( DebugFP, "Can't release lock on \"%s\", errno = %d\n",
					 DebugLock[debug_level], GetLastError() );
			DebugUnlockBroken = 1;
		}
#else
		if( flock(LockFd,LOCK_UN) < 0 ) {
			fprintf( DebugFP,"Can't release exclusive lock on \"%s\"\n",
					 DebugLock[debug_level] );
			DebugUnlockBroken = 1;
			dprintf_exit();
		}
#endif
	}

	if( DebugFile[debug_level] ) {
		(void)fclose( DebugFP );
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
				still_in_old_file == TRUE;
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

	if (link (DebugFile[debug_level], old) < 0)
		dprintf_exit();

	if (unlink (DebugFile[debug_level]) < 0)
		dprintf_exit();

	/* double check the result of the rename */
	{
		struct stat buf;
		if (stat (DebugFile[debug_level], &buf) >= 0)
		{
			/* Debug file exists! */
			fprintf (DebugFP, "Double check on rename failed!\n");
			fprintf (DebugFP, "%s still exists\n", DebugFile[debug_level]);
			dprintf_exit();
		}
	}

#endif

	if (DebugFP == NULL) {
		DebugFP = open_debug_file(debug_level, "a");
	}

	if (DebugFP == NULL) dprintf_exit();

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
fd_panic( line, file )
int		line;
char	*file;
{
	priv_state	priv;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	(void)close( 0 );
	if( DebugFile[0] ) {
		DebugFP = fopen(DebugFile[0], "a");
	}

	if( DebugFP == NULL ) {
		dprintf_exit();
	}
		/* Seek to the end */
	(void)lseek( fileno(DebugFP), 0, 2 );

	fprintf( DebugFP,
	"**** PANIC -- OUT OF FILE DESCRIPTORS at line %d in %s\n", line, file );
	(void)fflush( DebugFP );
	dprintf_exit();
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

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	/* Note: The log file shouldn't need to be group writeable anymore,
	   since PRIV_CONDOR changes euid now. */

#if !defined(WIN32)
	errno = 0;
#endif
	if( (fp=fopen(DebugFile[debug_level],flags)) == NULL ) {
#if !defined(WIN32)
		if( errno == EMFILE ) {
			fd_panic( __LINE__, __FILE__ );
		}
#endif
		if (DebugFP == 0) {
			DebugFP = stderr;
		}
		fprintf( DebugFP, "Can't open \"%s\"\n", DebugFile[debug_level] );
#if !defined(WIN32)
		fprintf( DebugFP, "errno = %d, euid = %d, egid = %d\n",
				 errno, geteuid(), getegid() );
#endif
		dprintf_exit();
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	return fp;
}

/* dprintf() hit some fatal error and is going to exit. */
void
dprintf_exit()
{
		/* First, set a flag so we know not to try to keep using
		   dprintf during the rest of this */
	DprintfBroken = 1;

		/* Don't forget to unlock the log file, if possible! */
	debug_unlock(0);

		/* If _EXCEPT_Cleanup is set for cleaning up during EXCEPT(),
		   we call that here, as well. */
	if( _EXCEPT_Cleanup ) {
		(*_EXCEPT_Cleanup)();
	}

		/* Actually exit now */
	fflush (stderr);
	exit(1);
}
