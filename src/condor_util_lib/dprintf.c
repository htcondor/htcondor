/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


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
#include <stdio.h>
#include <varargs.h>
#include <time.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "condor_sys.h"
#include "debug.h"
#include "clib.h"
#include "except.h"
#include "condor_uid.h"

#if defined(HPUX9) || defined(AIX32) || defined(LINUX) || defined (OSF1)
#	include <signal.h>
#endif

#if defined(HPUX9) || defined(Solaris)
#	include "fake_flock.h"
#endif

#if defined(Solaris)
#	include <sys/signal.h> 
#	include <sys/fcntl.h> 
#endif

FILE	*debug_lock();
FILE	*fdopen();

int open_debug_file( int flags );

extern int	errno;

int		DebugFlags = D_ALWAYS;
FILE	*DebugFP = stderr;
int		MaxLog;
char	*DebugFile;
char	*DebugLock;
int		(*DebugId)();

int		LockFd = -1;

static char _FileName_[] = __FILE__;

char *DebugFlagNames[] = {
	"D_ALWAYS", "D_TERMLOG", "D_SYSCALLS", "D_CKPT", "D_XDR", "D_MALLOC", 
	"D_NOHEADER", "D_LOAD", "D_EXPR", "D_PROC", "D_JOB", "D_MACHINE",
	"D_FULLDEBUG", "D_NFS", "D_UPDOWN", "D_AFS", "D_PREEMPT",
	"D_PROTOCOL", "D_PRIV", "D_TAPENET", "D_DAEMONCORE", "D_UNDEF21",
	"D_UNDEF22", "D_UNDEF23", "D_UNDEF24", "D_UNDEF25", "D_UNDEF26",
	"D_UNDEF27", "D_UNDEF28", "D_UNDEF29", "D_UNDEF30", "D_UNDEF31",
};

/*
**	Initialize the DebugFP to a specific file number.
*/
dprintf_init( fd )
int fd;
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
		exit( 1 );
	}
}

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
dprintf(va_alist)
va_dcl
{
	va_list pvar;
	int flags;
	char *fmt;
	struct tm *tm, *localtime();
	long *clock;
	int scm;
	sigset_t	mask, omask;
	int saved_errno;
	int	saved_flags;
	priv_state	priv;

	va_start(pvar);

	flags = va_arg(pvar, int);

		/* See if this is one of the messages we are logging */
	if( !(flags&DebugFlags) ) {
		goto VA_END;
	}

	saved_errno = errno;

	saved_flags = DebugFlags;       /* Limit recursive calls */
	DebugFlags = 0;



	scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );

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

		/* log files owned by condor system acct */
		/* avoid priv macros so we can bypass priv logging */
	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

		/* Open and lock the log file */
	(void)debug_lock();

		/* Print the message with the time and a nice identifier */
	if( ((saved_flags|flags) & D_NOHEADER) == 0 ) {
		(void)time(  (time_t *)&clock );
		tm = localtime( (time_t *)&clock );
		fprintf( DebugFP, "%d/%d %02d:%02d ", tm->tm_mon + 1, tm->tm_mday,
									tm->tm_hour, tm->tm_min );

		if( DebugId ) {
			(*DebugId)( DebugFP );
		}
	}

	fmt = va_arg(pvar, char *);

#if vax || (i386 && !LINUX && !defined(Solaris)) || bobcat || ibm032
	{
		int *argaddr = &va_arg(pvar, int);
		_doprnt( fmt, argaddr, DebugFP );
	}
#else 
	vfprintf( DebugFP, fmt, pvar );
#endif 

		/* Close and unlock the log file */
	debug_unlock();

		/* restore privileges */
	_set_priv(priv, __FILE__, __LINE__, 0);

		/* Let them signal handlers go!! */
	(void) sigprocmask( SIG_SETMASK, &omask, 0 );

	(void) SetSyscalls( scm );

	errno = saved_errno;
	DebugFlags = saved_flags;


VA_END:
	va_end(pvar);
}

FILE *
debug_lock()
{
	int			length;
	int			oumask;
	priv_state	priv;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

		/* Acquire the lock */
	if( DebugLock ) {
		if( LockFd < 0 ) {
			oumask = umask( 0 );
			LockFd = open(DebugLock,O_CREAT|O_WRONLY,0660);
			if( LockFd < 0 ) {
				if( errno == EMFILE ) {
					fd_panic( __LINE__, __FILE__ );
				}
				fprintf( DebugFP, "Can't open \"%s\"\n", DebugLock );
				exit( errno );
			}
			(void) umask( oumask );
		}

		if( flock(LockFd,LOCK_EX) < 0 ) {
			fprintf( DebugFP, "Can't get exclusive lock on \"%s\"\n",
							DebugLock);
			exit( errno );
		}
	}

	if( DebugFile ) {
		errno = 0;

		DebugFP = fdopen(open_debug_file(O_CREAT|O_WRONLY), "a");

		if( DebugFP == NULL ) {
			if( errno == EMFILE ) {
				fd_panic( __LINE__, __FILE__ );
			}
			fprintf(stderr, "Could not open DebugFile <%s>\n", DebugFile);
			exit( errno );
		}
			/* Seek to the end */
		if( (length=lseek(fileno(DebugFP),0,2)) < 0 ) {
			fprintf( DebugFP, "Can't seek to end of DebugFP file\n" );
			exit( errno );
		}

			/* If it's too big, preserve it and start a new one */
		if( MaxLog && length > MaxLog ) {
			fprintf( DebugFP, "MaxLog = %d, length = %d\n", MaxLog, length );
			preserve_log_file();
		}
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	return DebugFP;
}

debug_unlock()
{
	priv_state priv;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	(void)fflush( DebugFP );

	if( DebugLock ) {
			/* Don't forget to unlock the file */
		if( flock(LockFd,LOCK_UN) < 0 ) {
			fprintf(DebugFP,"Can't release exclusive lock on \"%s\"\n",
															DebugLock );
			exit( errno );
		}
	}

	if( DebugFile ) {
		(void)fclose( DebugFP );
		DebugFP = NULL;
	}

	_set_priv(priv, __FILE__, __LINE__, 0);
}


/*
** Copy the log file to a backup, then truncate the current one.
*/
preserve_log_file()
{
	char		old[MAXPATHLEN + 4];
	int			fd;
	priv_state	priv;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	(void)sprintf( old, "%s.old", DebugFile );
	fprintf( DebugFP, "Saving log file to \"%s\"\n", old );
	(void)fflush( DebugFP );

	/*
	** for some still unknown reason, newer versions of OSF/1 (Digital Unix)
    ** sometimes do not rotate the log correctly using 'rename'.  Changing
	** this to a unlink/link/unlink sequence seems to work.  May as well do
	** it for all platforms as there is no need for atomicity.  --RR
    */

	fclose( DebugFP );
	unlink (old);

	if (link (DebugFile, old) < 0)
		exit (__LINE__);

	if (unlink (DebugFile) < 0)
		exit (__LINE__);

	/* double check the result of the rename */
	{
		struct stat buf;
		if (stat (DebugFile, &buf) >= 0)
		{
			/* Debug file exists! */
			fprintf (DebugFP, "Double check on rename failed!\n");
			fprintf (DebugFP, "%s still exists\n", DebugFile);
			exit (__LINE__);
		}
	}

	fd = open_debug_file( O_CREAT | O_WRONLY );

#if 0
	(void)close( fileno(DebugFP) );
#	if defined(HPUX9)
	DebugFP->__fileL = fd & 0xf0;	/* Low byte of fd */
	DebugFP->__fileH = fd & 0x0f;	/* High byte of fd */
#	elif defined(ULTRIX43) || defined(IRIX331)
	((DebugFP)->_file) = fd;
#	else
	fileno(DebugFP) = fd;
#	endif
#else
	DebugFP = fdopen( fd, "a" );
	if (DebugFP == NULL) exit (__LINE__);
#endif
	fprintf (DebugFP, "Now in new log file %s\n", DebugFile);

	_set_priv(priv, __FILE__, __LINE__, 0);
}

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
	if( DebugFile ) {
		DebugFP = fopen(DebugFile, "a");
	}

	if( DebugFP == NULL ) {
		exit( EMFILE );
	}
		/* Seek to the end */
	(void)lseek( fileno(DebugFP), 0, 2 );

	fprintf( DebugFP,
	"**** PANIC -- OUT OF FILE DESCRIPTORS at line %d in %s\n", line, file );
	(void)fflush( DebugFP );
	exit( EMFILE );
}

	

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

int
open_debug_file(int flags)
{
	int			fd;
	int			oumask;
	priv_state	priv;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	/* The log file MUST be group writeable */
	oumask = umask( 0 );
	if( (fd=open(DebugFile,flags,0664)) < 0 ) {
		if( errno == EMFILE ) {
			fd_panic( __LINE__, __FILE__ );
		}
		if (DebugFP == 0) {
			DebugFP = stderr;
		}
		fprintf( DebugFP, "Can't open \"%s\"\n", DebugFile );
		fprintf( DebugFP, "errno = %d, euid = %d, egid = %d\n",
				 errno, geteuid(), getegid() );
		perror( "open" );
		abort();
		exit( errno );
	}
	(void) umask( oumask );

	_set_priv(priv, __FILE__, __LINE__, 0);

	return fd;
}
