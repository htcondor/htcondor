/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 


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
#include "condor_sys_types.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "exit.h"
#include "condor_uid.h"
#include "basename.h"
#include "file_lock.h"
#if HAVE_BACKTRACE
#include "execinfo.h"
#endif
#include "util_lib_proto.h"		// for mkargv() proto
#include "condor_threads.h"
#include "log_rotate.h"
#include "dprintf_internal.h"

FILE *debug_lock(int debug_flags, const char *mode, int force_lock);
void debug_unlock(int debug_flags);

static FILE *debug_lock_it(struct DebugFileInfo* it, const char *mode, int force_lock, bool dont_panic);
static void debug_unlock_it(struct DebugFileInfo* it);
static FILE *open_debug_file(struct DebugFileInfo* it, const char flags[], bool dont_panic);
static void debug_close_file(struct DebugFileInfo* it);
static void debug_close_all_files(void);
static void debug_open_lock(void);
static void debug_close_lock(void);
static FILE *preserve_log_file(struct DebugFileInfo* it, bool dont_panic);

void _condor_dprintf_exit( int error_code, const char* msg );
void _condor_set_debug_flags( const char *strflags );
static void _condor_save_dprintf_line( int flags, const char* fmt, va_list args );
void _condor_dprintf_saved_lines( void );
struct saved_dprintf {
	int level;
	char* line;
	struct saved_dprintf* next;
};
static struct saved_dprintf* saved_list = NULL;
static struct saved_dprintf* saved_list_tail = NULL;

extern	DLL_IMPORT_MAGIC int		errno;
extern  int		DebugFlags;
extern param_functions *dprintf_param_funcs;

/*
   This is a global flag that tells us if we've successfully ran
   dprintf_config() or otherwise setup dprintf() to print where we
   want it to go.  We use it here so that if we call dprintf() before
   dprintf_config(), we save the messages into a special list and
   dump them all out once dprintf is configured.
*/
extern int _condor_dprintf_works;

int DebugShouldLockToAppend = 0;
static int DebugIsLocked = 0;

static int DebugLockDelay = 0; /* seconds spent waiting for lock */
static time_t DebugLockDelayPeriodStarted = 0;

/*
 * On Windows we have the ability to hold open the handle/FD to the
 * log file.  On Linux this is not enabled because of the hard global
 * limit to FDs.  Windows can open as many as you want short of running
 * out of physical resources like memory.
 */
std::vector<DebugFileInfo> * DebugLogs = NULL;
/*
 * This is last modification time of the main debug file as returned
 * by stat() before the current process has written anything to the
 * file. It is set in dprintf_config, which sets it to -errno if that
 * stat() fails.
 * DaemonCore uses this as an approximation of when the daemon
 * was last alive.
 */
time_t	DebugLastMod = 0;

/*
 * If LOGS_USE_TIMESTAMP is enabled, we will print out Unix timestamps
 * instead of the standard date format in all the log messages
 */
int		DebugUseTimestamps = 0;

/*
 * When true, don't exit even if we fail to open the debug output file.
 * Added so that on Win32 the kbdd (which is running as a user) won't quit 
 * if it does't have access to the directory where log files live.
 *
 */
int      DebugContinueOnOpenFailure = 0;

/*
** These arrays must be D_NUMLEVELS+1 in size since we can have a
** debug file for each level plus an additional catch-all debug file
** at index 0.
*/
char	*DebugLock = NULL;

int		(*DebugId)(char **buf,int *bufpos,int *buflen);
int		SetSyscalls(int mode);

int		LockFd = -1;

int		log_keep_open = 0;

static	int DprintfBroken = 0;
static	int DebugUnlockBroken = 0;
#if !defined(WIN32) && defined(HAVE_PTHREADS)
#include <pthread.h>
static pthread_mutex_t _condor_dprintf_critsec = 
						PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif
#ifdef WIN32
static CRITICAL_SECTION	*_condor_dprintf_critsec = NULL;
static int lock_or_mutex_file(int fd, LOCK_TYPE type, int do_block);
extern int vprintf_length(const char *format, va_list args);
static HANDLE debug_win32_mutex = NULL;
#endif
static int use_kernel_mutex = -1;
static int dprintf_count = 0;
/*
** Note: setting this to true will avoid blocking signal handlers from running
** while we are printing log messages.  It's probably a good idea to block
** them as they may get into trouble with manipulating the lock on the
** log file.  However blocking them will cause many implementations of
** dbx to hang.
*/
int InDBX = 0;

#define DPRINTF_ERR_MAX 255

#define FCLOSE_RETRY_MAX 10

DebugFileInfo::DebugFileInfo(const DebugFileInfo &debugFileInfo)
{
	this->debugFlags = debugFileInfo.debugFlags;
	this->logPath = std::string(debugFileInfo.logPath);
	this->maxLog = debugFileInfo.maxLog;
	this->maxLogNum = debugFileInfo.maxLogNum;
	this->debugFP = NULL;
}

DebugFileInfo::~DebugFileInfo()
{
	if(debugFP)
	{
		fclose(debugFP);
		debugFP = NULL;
	}
}

bool DebugFileInfo::MatchesFlags(int flags) const
{
	if ( ! flags) return true;
	if ( ! this->debugFlags) return (DebugFlags & flags) != 0;
	return (this->debugFlags & flags) != 0;
}

static char *formatTimeHeader(struct tm *tm) {
	static char timebuf[80];
	static char *timeFormat = 0;
	static int firstTime = 1;

	if (firstTime) {
		firstTime = 0;
		timeFormat = dprintf_param_funcs->param( "DEBUG_TIME_FORMAT" );
		if (!timeFormat) {
			timeFormat = strdup("%m/%d/%y %H:%M:%S ");
		} else {
			// Skip enclosing quotes
			char *p;
			if (*timeFormat == '"') {
				timeFormat++;
			}
			p = timeFormat;
			while (*p++) {
				if (*p == '"') *p = '\0';
			}
		}
	}
	strftime(timebuf, 80, timeFormat, tm);
	return timebuf;
}

/* _condor_dfprintf_va
 * This function is used internally by the dprintf system wherever
 * it wants to write directly to the open debug log.
 *
 * Since this function is called from a code path that is already
 * not reentrant, this function itself does not bother to try
 * to be reentrant.  Specifically, it uses static buffers.  If we
 * ever do reenter this function it is assumed that the program
 * will abort before returning to the prior call to this function.
 *
 * The caller of this function should not assume that args can be
 * used again on systems where va_copy is required.  If the caller
 * wishes to use args again, the caller should therefore pass in
 * a copy of the args.
 */
static void
_condor_dfprintf_va( int flags, int mask_flags, time_t clock_now, struct tm *tm, FILE *fp, const char* fmt, va_list args )
{
		// static buffer to avoid frequent memory allocation
	static char *buf = NULL;
	static int buflen = 0;

	int bufpos = 0;
	int rc = 0;
	int sprintf_errno = 0;
	int my_pid;
	int my_tid;
	int start_pos;
	FILE *local_fp;
	int fopen_rc = 1;

		/* Print the message with the time and a nice identifier */
	if( ((mask_flags|flags) & D_NOHEADER) == 0 ) {
		if ( DebugUseTimestamps ) {
				// Casting clock_now to int to get rid of compile
				// warning.  Probably format should be %ld, and
				// we should cast to long int, but I'm afraid of
				// changing the output format.  wenger 2009-02-24.
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(%d) ", (int)clock_now );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		} else {
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "%s", formatTimeHeader(tm));
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

		if ( (mask_flags|flags) & D_FDS ) {
			//Regardless of whether we're keeping the log file open our not, we open
			//the NULL file for the FD number.
			if( (local_fp=safe_fopen_wrapper_follow(NULL_FILE,"rN",0644)) == NULL )
			{
				local_fp = fp;
				fopen_rc = 0;
			}
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(fd:%d) ", fileno(local_fp) );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
			if(fopen_rc)
			{
				fopen_rc = fclose_wrapper(local_fp, FCLOSE_RETRY_MAX);
			}
		}

		if( (mask_flags|flags) & D_PID ) {
#ifdef WIN32
			my_pid = (int) GetCurrentProcessId();
#else
			my_pid = (int) getpid();
#endif
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(pid:%d) ", my_pid );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

			/* include tid if we are configured to use a thread pool */
		my_tid = CondorThreads_gettid();
		if ( my_tid > 0 ) {
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(tid:%d) ", my_tid );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

		if( DebugId ) {
			rc = (*DebugId)( &buf, &bufpos, &buflen );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}
	}

	if( sprintf_errno != 0 ) {
		_condor_dprintf_exit(sprintf_errno, "Error writing to debug header\n");	
	}

	rc = vsprintf_realloc( &buf, &bufpos, &buflen, fmt, args );

		/* printf returns < 0 on error */
	if (rc < 0) {
		_condor_dprintf_exit(errno, "Error writing to debug buffer\n");	
	}

		// We attempt to write the log record with one call to
		// write(), because then O_APPEND will ensure (on
		// compliant file systems) that writes from different
		// processes are not interleaved.  Since we are blocking
		// signals, we should not need to loop here on EINTR,
		// but we do anyway in case one of the exotic signals
		// that we are not blocking interrupts us.
	start_pos = 0;
	while( start_pos<bufpos ) {
		rc = write( fileno(fp),
					buf+start_pos,
					bufpos-start_pos );
		if( rc > 0 ) {
			start_pos += rc;
		}
		else if( errno != EINTR ) {
			_condor_dprintf_exit(errno, "Error writing debug log\n");	
		}
	}
}

/* _condor_dfprintf
 * This function is used internally by the dprintf system wherever
 * it wants to write directly to the open debug log.
 */
static void
_condor_dfprintf( FILE *fp, const char* fmt, ... )
{
	struct tm *tm=0;
	time_t clock_now;
    va_list args;

	memset((void*)&clock_now,0,sizeof(time_t)); // just to stop Purify UMR errors
	(void)time(  &clock_now );
	if ( ! DebugUseTimestamps ) {
		tm = localtime( &clock_now );
	}

    va_start( args, fmt );
	_condor_dfprintf_va(D_ALWAYS,D_ALWAYS|DebugFlags,clock_now,tm,fp,fmt,args);
    va_end( args );
}

int dprintf_getCount(void)
{
    return dprintf_count;
}

/*
** Print a nice log message, but only if "flags" are included in the
** current debugging flags.
*/
/* VARARGS1 */

// prototype
struct tm *localtime();

void
_condor_dprintf_va( int flags, const char* fmt, va_list args )
{
	struct tm *tm=0;
	time_t clock_now;
#if !defined(WIN32)
	sigset_t	mask, omask;
	mode_t		old_umask;
#endif
	int saved_errno;
	priv_state	priv;
	FILE *debug_file_ptr = NULL;
	std::vector<DebugFileInfo>::iterator it;

		/* DebugFP should be static initialized to stderr,
	 	   but stderr is not a constant on all systems. */

		/* If we hit some fatal error in dprintf, this flag is set.
		   If dprintf is broken and someone (like _EXCEPT_Cleanup)
		   trys to dprintf, we just return to avoid infinite loops. */
	if( DprintfBroken ) return;

		/* 
		   See if dprintf_config() has been called.  if not, save the
		   message into a list so we can dump them out all at once
		   when we've got a working log file.  we need to do this
		   before we check the debug flags since they won't be
		   initialized until we call dprintf_config().
		*/
	if( ! _condor_dprintf_works ) {
		_condor_save_dprintf_line( flags, fmt, args );
		return; 
	} 

		/* See if this is one of the messages we are logging */
	if( !(flags&DebugFlags) ) {
		return;
	}


#if !defined(WIN32) /* signals and umasks don't exist in WIN32 */

	/* Block any signal handlers which might try to print something */
	/* Note: do this BEFORE grabbing the _condor_dprintf_critsec mutex */
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

	/* We want dprintf to be thread safe.  For now, we achieve this
	 * with fairly coarse-grained mutex. On Unix, signals that may result
	 * in a call to dprintf() had better be blocked by now, or deadlock may 
	 * occur.
	 */
#ifdef WIN32
	if ( _condor_dprintf_critsec == NULL ) {
		_condor_dprintf_critsec = 
			(CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
		InitializeCriticalSection(_condor_dprintf_critsec);
	}
	EnterCriticalSection(_condor_dprintf_critsec);
#elif defined(HAVE_PTHREADS)
	/* On Win32 we always grab a mutex because we are always running
	 * with mutiple threads.  But on Unix, lets bother w/ mutexes if and only
	 * if we are running w/ threads.
	 */
	if ( CondorThreads_pool_size() ) {  /* will == 0 if no threads running */
		pthread_mutex_lock(&_condor_dprintf_critsec);
	}
#endif

	saved_errno = errno;


	/* log files owned by condor system acct */

		/* If we're in PRIV_USER_FINAL, there's a good chance we won't
		   be able to write to the log file.  We can't rely on Condor
		   code to refrain from calling dprintf() after switching to
		   PRIV_USER_FINAL.  So, we check here and simply don't try to
		   log anything when we're in PRIV_USER_FINAL, to avoid
		   exit(DPRINTF_ERROR). */
	if (get_priv() == PRIV_USER_FINAL) {
		/* Ensure to undo the signal blocking/umask code for unix and
			leave the critical section for windows. */
		goto cleanup;
	}

	{
		static int in_nonreentrant_part = 0;
		if( in_nonreentrant_part ) {
			/* Some of the following code messes with global state and
			 * does not expect to be called recursively.  Note that if
			 * we do get called recursively, the locking that happens
			 * before this point is expected to work correctly (avoid
			 * self-deadlock).
			 */
			goto cleanup;
		}
		in_nonreentrant_part = 1;

			/* avoid priv macros so we can bypass priv logging */
		priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

			/* Grab the time info only once, instead of inside the for
			   loop.  -Derek 9/14 */
		memset((void*)&clock_now,0,sizeof(time_t)); // just to stop Purify UMR errors
		(void)time(  &clock_now );
		if ( ! DebugUseTimestamps ) {
			tm = localtime( &clock_now );
		}
	
			/* print debug message to catch-all debug file plus files */
			/* registered for other debug levels */
		if(!DebugLogs->size())
		{
			debug_file_ptr = stderr;
#ifdef va_copy
			va_list copyargs;
			va_copy(copyargs, args);
			_condor_dfprintf_va(flags,DebugFlags,clock_now,tm,debug_file_ptr,fmt,copyargs);
			va_end(copyargs);
#else
			_condor_dfprintf_va(flags,DebugFlags,clock_now,tm,debug_file_ptr,fmt,args);
#endif
		}
		for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
		{
			int debugFlags = (*it).debugFlags;
			/*
			 * if debugFlags for the file is 0, print everything
			 * otherwise print only messages that match at least one of the flags.
			 * note: this means that D_ALWAYS will go only to slots where debugFlags == 0
			 */
			if (debugFlags && !(debugFlags & flags))
				continue;

			// for log files other than the first one, dont panic if we
			// fail to write to the file.
			bool dont_panic = (debugFlags != 0) || DebugContinueOnOpenFailure;

			/* Open and lock the log file */
			debug_file_ptr = debug_lock_it(&(*it), NULL, 0, dont_panic);
			if (debug_file_ptr) {
#ifdef va_copy
				va_list copyargs;
				va_copy(copyargs, args);
				_condor_dfprintf_va(flags,DebugFlags,clock_now,tm,debug_file_ptr,fmt,copyargs);
				va_end(copyargs);
#else
				_condor_dfprintf_va(flags,DebugFlags,clock_now,tm,debug_file_ptr,fmt,args);
#endif
			}

			debug_unlock_it(&(*it));
		}

			/* restore privileges */
		_set_priv(priv, __FILE__, __LINE__, 0);

        dprintf_count += 1;

		in_nonreentrant_part = 0;
	}

	cleanup:

	errno = saved_errno;

#if !defined(WIN32) // umasks don't exist in WIN32
		/* restore umask */
	(void)umask( old_umask );
#endif

	/* Release mutex.  Note: we MUST do this before we renable signals */
#ifdef WIN32
	LeaveCriticalSection(_condor_dprintf_critsec);
#elif defined(HAVE_PTHREADS)
	if ( CondorThreads_pool_size() ) {  /* will == 0 if no threads running */
		pthread_mutex_unlock(&_condor_dprintf_critsec);
	}
#endif

#if !defined(WIN32) // signals don't exist in WIN32
		/* Let them signal handlers go!! */
	(void) sigprocmask( SIG_SETMASK, &omask, 0 );
#endif
}

int
_condor_open_lock_file(const char *filename,int flags, mode_t perm)
{
	int	retry = 0;
	int save_errno = 0;
	priv_state	priv;
	char*		dirpath = NULL;
	int lock_fd;

	if( !filename ) {
		return -1;
	}

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);
	lock_fd = safe_open_wrapper_follow(filename,flags,perm);
	if( lock_fd < 0 ) {
		save_errno = errno;
		if( save_errno == ENOENT ) {
				/* 
				   No directory: Try to create the directory
				   itself, first as condor, then as root.  If
				   we created it as root, we need to try to
				   chown() it to condor.
				*/ 
			dirpath = condor_dirname( filename );
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
						   retry the safe_open_wrapper(). */
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
					   try the safe_open_wrapper() again */
				retry = 1;
			}
				/* At this point, we're done with this, so
				   don't leak it. */
			free( dirpath );
		}
		if( retry ) {
			lock_fd = safe_open_wrapper_follow(filename,flags,perm);
			if( lock_fd < 0 ) {
				save_errno = errno;
			}
		}
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	if( lock_fd < 0 ) {
		errno = save_errno;
	}
	return lock_fd;
}

/* debug_open_lock
 * - assumes correct priv state (PRIV_CONDOR) has already been set
 * - aborts the program on error
 */
static void
debug_open_lock(void)
{
	int save_errno;
	char msg_buf[DPRINTF_ERR_MAX];
	struct stat fstatus;
	time_t start_time,end_time;

	if ( use_kernel_mutex == -1 ) {
#ifdef WIN32
		// Use a mutex by default on Win32
		use_kernel_mutex = dprintf_param_funcs->param_boolean_int("FILE_LOCK_VIA_MUTEX", TRUE);
#else
		// Use file locking by default on Unix.  We should 
		// call param_boolean_int here, but since locking via
		// a mutex is not yet implemented on Unix, we will force it
		// to always be FALSE no matter what the config file says.
		// use_kernel_mutex = param_boolean_int("FILE_LOCK_VIA_MUTEX", FALSE);
		use_kernel_mutex = FALSE;
#endif
	}

		/* Acquire the lock */
	if( DebugLock ) {
		if( use_kernel_mutex == FALSE) {
			if (LockFd > 0 ) {
				fstat(LockFd, &fstatus);
				if (fstatus.st_nlink == 0){
					close(LockFd);
					LockFd = -1;
				}	
			}
			if (LockFd < 0) {
				LockFd = _condor_open_lock_file(DebugLock,O_CREAT|O_WRONLY,0660);
				if( LockFd < 0 ) {
					save_errno = errno;
					snprintf( msg_buf, sizeof(msg_buf), "Can't open \"%s\"\n", DebugLock );
					_condor_dprintf_exit( save_errno, msg_buf );
				} 
			}	
		}

		start_time = time(NULL);
		if( DebugLockDelayPeriodStarted == 0 ) {
			DebugLockDelayPeriodStarted = start_time;
		}

		errno = 0;
#ifdef WIN32
		if( lock_or_mutex_file(LockFd,WRITE_LOCK,TRUE) < 0 )
#else
		if( lock_file_plain(LockFd,WRITE_LOCK,TRUE) < 0 )
#endif
		{
			save_errno = errno;
			snprintf( msg_buf, sizeof(msg_buf), "Can't get exclusive lock on \"%s\", "
					 "LockFd: %d\n", DebugLock, LockFd );
			_condor_dprintf_exit( save_errno, msg_buf );
		}

		DebugIsLocked = 1;

			/* Update DebugLockDelay.  Ignore delays that are less than
			 * two seconds because the resolution is only 1s.
			 */
		end_time = time(NULL);
		if( end_time-start_time > 1 ) {
			DebugLockDelay += end_time-start_time;
		}
	}
}

void dprintf_reset_lock_delay(void) {
	DebugLockDelay = 0;
	DebugLockDelayPeriodStarted = 0;
}

double dprintf_get_lock_delay(void) {
	time_t now = time(NULL);
	if( now - DebugLockDelayPeriodStarted <= 0 ) {
		return 0;
	}
	return ((double)DebugLockDelay)/(now-DebugLockDelayPeriodStarted);
}

FILE *
debug_lock(int debug_flags, const char *mode, int force_lock)
{
	std::vector<DebugFileInfo>::iterator it;

	for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
	{
		/*
		 * debug_level is being treated by the caller as an INDEX into
		 * the DebugLogs vector, this it nuts, but it works as long as
		 * each file has a unique set of flags which is true for now...
		 */
		if(it->debugFlags != debug_flags)
			continue;

		// for log files other than the first one, dont panic if we
		// fail to write to the file.
		bool dont_panic = (it->debugFlags != 0) || DebugContinueOnOpenFailure;

		return debug_lock_it(&(*it), mode, force_lock, dont_panic);
	}

	return stderr;
}

static FILE *
debug_lock_it(struct DebugFileInfo* it, const char *mode, int force_lock, bool dont_panic)
{
	off_t		length = 0; // this gets assigned return value from lseek()
	priv_state	priv;
	int save_errno;
	char msg_buf[DPRINTF_ERR_MAX];
	int locked = 0;
	FILE *debug_file_ptr = it->debugFP;

	if ( mode == NULL ) {
		mode = "aN";
	}

	errno = 0;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	if(debug_file_ptr)
	{
		//Hypothetically if we never closed the file, we
		//should have never unlocked it either.  The best
		//way to handle this will need further thought.
		if( DebugShouldLockToAppend || force_lock )
			locked = 1;
	}
	else
	{
		if( DebugShouldLockToAppend || force_lock ) {
			debug_open_lock();
			locked = 1;
		}

		//open_debug_file will set DebugFPs[debug_level] so we do
		//not have to worry about it in this function, assuming
		//there are no further errors.
		debug_file_ptr = open_debug_file(it, mode, dont_panic);

		if( debug_file_ptr == NULL ) {
			
			save_errno = errno;
			if (dont_panic) {
				_set_priv(priv, __FILE__, __LINE__, 0);
				return NULL;
			}
#ifdef WIN32
			if (DebugContinueOnOpenFailure) {
				_set_priv(priv, __FILE__, __LINE__, 0);
				return NULL;
			}
#else
			if( errno == EMFILE ) {
				_condor_fd_panic( __LINE__, __FILE__ );
			}
#endif
			snprintf( msg_buf, sizeof(msg_buf), "Could not open DebugFile \"%s\"\n", 
				it->logPath.c_str() );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
	}

	if( (length=lseek(fileno(debug_file_ptr), 0, SEEK_END)) < 0 ) {
		if (dont_panic) {
			if(locked) debug_close_lock();
			debug_close_file(it);

			return NULL;
		}
		save_errno = errno;
		snprintf( msg_buf, sizeof(msg_buf), "Can't seek to end of DebugFP file\n" );
		_condor_dprintf_exit( save_errno, msg_buf );
	}

	if( it->maxLog && length > it->maxLog ) {
		if( !locked ) {
			/*
			 * We only need to redo everything if there is a lock defined
			 * for the log.
			 */

			if (debug_file_ptr) {
				int result = fflush( debug_file_ptr );
				if (result < 0) {
					DebugUnlockBroken = 1;
					_condor_dprintf_exit(errno, "Can't fflush debug log file\n");
				}
			}

			/*
			 * We need to be in PRIV_CONDOR for the code in these two
			 * functions, so since we are already in that privilege mode,
			 * we do not go back to the old priv state until we call the
			 * two functions.
			 */
			if(DebugLock)
			{
				debug_close_lock();
				debug_close_file(it);
				_set_priv(priv, __FILE__, __LINE__, 0);
				return debug_lock_it(it, mode, 1, dont_panic);
			}
		}

		// Casting length to int to get rid of compile warning.
		// Probably format should be %ld, and we should cast to
		// long int, but I'm afraid of changing the output format.
		// wenger 2009-02-24.
		_condor_dfprintf(debug_file_ptr, "MaxLog = %lld, length = %lld\n", (long long)it->maxLog, (long long)length);
		
		debug_file_ptr = preserve_log_file(it, dont_panic);
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	return debug_file_ptr;
}

static void 
debug_close_lock(void)
{
	int flock_errno;
	char msg_buf[DPRINTF_ERR_MAX];
	if(DebugUnlockBroken)
		return;

	if(DebugIsLocked)
	{
		errno = 0;
#ifdef WIN32
		if ( lock_or_mutex_file(LockFd,UN_LOCK,TRUE) < 0 )
#else
		if( lock_file_plain(LockFd,UN_LOCK,TRUE) < 0 )
#endif
		{
			flock_errno = errno;
			snprintf( msg_buf, sizeof(msg_buf), "Can't release exclusive lock on \"%s\", LockFd=%d\n", 
				DebugLock, LockFd );
			DebugUnlockBroken = 1;
			_condor_dprintf_exit( flock_errno, msg_buf );
		}
	}
}

static void 
debug_close_file(struct DebugFileInfo* it)
{
	FILE *debug_file_ptr = (*it).debugFP;

	if( debug_file_ptr ) {
		if (debug_file_ptr) {
			int close_result = fclose_wrapper( debug_file_ptr, FCLOSE_RETRY_MAX );
			if (close_result < 0) {
				DebugUnlockBroken = 1;
				_condor_dprintf_exit(errno, "Can't fclose debug log file\n");
			}
			(*it).debugFP = NULL;
		}
	}
}

static void 
debug_close_all_files()
{
	FILE *debug_file_ptr = NULL;
	std::vector<DebugFileInfo>::iterator it;

	for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
	{
		debug_file_ptr = (*it).debugFP;
		if(!debug_file_ptr)
			continue;
		int close_result = fclose_wrapper( debug_file_ptr, FCLOSE_RETRY_MAX );
		if (close_result < 0) {
			DebugUnlockBroken = 1;
			_condor_dprintf_exit(errno, "Can't fclose debug log file\n");
		}
		(*it).debugFP = NULL;
	}
}

void
debug_unlock(int debug_flags)
{
	std::vector<DebugFileInfo>::iterator it;
	for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
	{
		/*
		 * debug_level is being treated by the caller as an INDEX into
		 * the DebugLogs vector, this it nuts, but it works as long as
		 * each file has a unique set of flags which is true for now...
		 */
		if(it->debugFlags != debug_flags)
			continue;
		debug_unlock_it(&(*it));
		break;
	}
}

static void
debug_unlock_it(struct DebugFileInfo* it)
{
	priv_state priv;
	int result = 0;

	FILE *debug_file_ptr = (*it).debugFP;

	if(log_keep_open)
		return;

	if( DebugUnlockBroken ) {
		return;
	}

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	if (debug_file_ptr) {
		result = fflush( debug_file_ptr );
		if (result < 0) {
				DebugUnlockBroken = 1;
				_condor_dprintf_exit(errno, "Can't fflush debug log file\n");
		}

		debug_close_lock();
		debug_close_file(it);
	}

	_set_priv(priv, __FILE__, __LINE__, 0);
}


/*
** Copy the log file to a backup, then truncate the current one.
*/
static FILE *
preserve_log_file(struct DebugFileInfo* it, bool dont_panic)
{
	char		old[MAXPATHLEN + 4];
	priv_state	priv;
	int			still_in_old_file = FALSE;
	int			failed_to_rotate = FALSE;
	int			save_errno;
	int         rename_failed = 0;
	const char *timestamp;
	int			result;
	int			file_there = 0;
	FILE		*debug_file_ptr = (*it).debugFP;
	std::string		filePath = (*it).logPath;
#ifndef WIN32
	struct stat buf;
#endif
	char msg_buf[DPRINTF_ERR_MAX];


	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);
	(void)setBaseName(filePath.c_str());
	timestamp = createRotateFilename(NULL, it->maxLogNum);
	(void)sprintf( old, "%s.%s", filePath.c_str() , timestamp);
	_condor_dfprintf( debug_file_ptr, "Saving log file to \"%s\"\n", old );
	(void)fflush( debug_file_ptr );

	fclose_wrapper( debug_file_ptr, FCLOSE_RETRY_MAX );
	debug_file_ptr = NULL;
	(*it).debugFP = debug_file_ptr;

	result = rotateTimestamp(timestamp, it->maxLogNum);

#if defined(WIN32)
	if (result < 0) { // MoveFileEx and Copy failed
		failed_to_rotate = TRUE;
		debug_file_ptr = open_debug_file(it, "wN", dont_panic);
		if ( debug_file_ptr ==  NULL ) {
			still_in_old_file = TRUE;
		}
	}
#else

	if (result != 0) 
		rename_failed = 1;

	errno = 0;
	if (result != 0) {
		save_errno = result;
		if( save_errno == ENOENT && !DebugLock ) {
				/* This can happen if we are not using debug file locking,
				   and two processes try to rotate this log file at the
				   same time.  The other process must have already done
				   the rename but not created the new log file yet.
				*/
			rename_failed = 1;
		}
		else {
			snprintf( msg_buf, sizeof(msg_buf), "Can't rename(%s,%s)\n",
					  filePath.c_str(), old );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
	}
	
	/* double check the result of the rename
	   If we are not using locking, then it is possible for two processes
	   to rotate at the same time, in which case the following check
	   should be skipped, because it is expected that a new file may
	   have already been created by now. */

	if( DebugLock && DebugShouldLockToAppend ) {
		errno = 0;
		if (stat (filePath.c_str(), &buf) >= 0)
		{
			file_there = 1;
			save_errno = errno;
			snprintf( msg_buf, sizeof(msg_buf), "rename(%s) succeeded but file still exists!\n", 
					 filePath.c_str() );
			/* We should not exit here - file did rotate but something else created it newly. We
			 therefore won't grow without bounds, we "just" lost control over creating the file.
			 We should happily continue anyway and just put a log message into the system telling
			 about this incident.
			 */
		}
	}

#endif

	if (debug_file_ptr == NULL) {
		debug_file_ptr = open_debug_file(it, "aN", dont_panic);
	}

	if( debug_file_ptr == NULL ) {
		debug_file_ptr = stderr;

		save_errno = errno;
		snprintf( msg_buf, sizeof(msg_buf), "Can't open file for debug level %d\n",
				 it->debugFlags ); 
		_condor_dprintf_exit( save_errno, msg_buf );
	}

	if ( !still_in_old_file ) {
		_condor_dfprintf (debug_file_ptr, "Now in new log file %s\n", it->logPath.c_str());
	}

	// We may have a message left over from the succeeded rename after which the file
	// may have been recreated by another process. Tell user about it.
	if (file_there > 0) {
		_condor_dfprintf(debug_file_ptr, "WARNING: %s", msg_buf);
	}

	if ( failed_to_rotate || rename_failed ) {
		_condor_dfprintf(debug_file_ptr,"WARNING: Failed to rotate log into file %s!\n",old);
		if( rename_failed ) {
			_condor_dfprintf(debug_file_ptr,"Likely cause is that another Condor process rotated the file at the same time.\n");
		}
		else {
			_condor_dfprintf(debug_file_ptr,"       Perhaps someone is keeping log files open???");
		}
	}
	
	_set_priv(priv, __FILE__, __LINE__, 0);
	cleanUp(it->maxLogNum);
	(*it).debugFP = debug_file_ptr;

	return debug_file_ptr;
}


#if !defined(WIN32)
/*
** Can't open log or lock file becuase we are out of fd's.  Try to let
** somebody know what happened.
*/
void
_condor_fd_panic( int line, const char* file )
{
	int i;
	char msg_buf[DPRINTF_ERR_MAX];
	char panic_msg[DPRINTF_ERR_MAX];
	int save_errno;
	std::vector<DebugFileInfo>::iterator it;
	std::string filePath;
	bool fileExists = false;
	FILE* debug_file_ptr=0;

	_set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	snprintf( panic_msg, sizeof(panic_msg),
			 "**** PANIC -- OUT OF FILE DESCRIPTORS at line %d in %s",
			 line, file );

		/* Just to be extra paranoid, let's nuke a bunch of fds. */
	for ( i=0 ; i<50 ; i++ ) {
		(void)close( i );
	}

	for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
	{
		filePath = (*it).logPath;
		fileExists = true;
		break;
	}
	if( fileExists ) {
		debug_file_ptr = safe_fopen_wrapper_follow(filePath.c_str(), "a", 0644);
	}

	if( !debug_file_ptr ) {
		save_errno = errno;
		snprintf( msg_buf, sizeof(msg_buf), "Can't open \"%s\"\n%s\n", filePath.c_str(),
				 panic_msg ); 
		_condor_dprintf_exit( save_errno, msg_buf );
	}
		/* Seek to the end */
	(void)lseek( fileno(debug_file_ptr), 0, SEEK_END );
	fprintf( debug_file_ptr, "%s\n", panic_msg );
	(void)fflush( debug_file_ptr );

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

static FILE *
open_debug_file(struct DebugFileInfo* it, const char flags[], bool dont_panic)
{
	FILE		*fp;
	priv_state	priv;
	char msg_buf[DPRINTF_ERR_MAX];
	int save_errno;
	std::string filePath = (*it).logPath;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	/* Note: The log file shouldn't need to be group writeable anymore,
	   since PRIV_CONDOR changes euid now. */

	errno = 0;
	if( (fp=safe_fopen_wrapper_follow(filePath.c_str(),flags,0644)) == NULL ) {
		save_errno = errno;
#if !defined(WIN32)
		if( errno == EMFILE ) {
			_condor_fd_panic( __LINE__, __FILE__ );
		}
#endif
		if (fp == NULL) {
			fp = stderr;
		}
		_condor_dfprintf( fp, "Can't open \"%s\"\n", filePath.c_str() );
		if( ! dont_panic ) {
			snprintf( msg_buf, sizeof(msg_buf), "Can't open \"%s\"\n",
					 filePath.c_str() );

			if ( ! DebugContinueOnOpenFailure) {
			    _condor_dprintf_exit( save_errno, msg_buf );
			}
		}
		// fp is guaranteed to be NULL here.
	}

	_set_priv(priv, __FILE__, __LINE__, 0);
	it->debugFP = fp;

	return fp;
}

bool debug_check_it(struct DebugFileInfo& it, bool fTruncate, bool dont_panic)
{
	FILE *debug_file_fp;

	if( fTruncate ) {
		debug_file_fp = debug_lock_it(&it, "wN", 0, dont_panic);
	} else {
		debug_file_fp = debug_lock_it(&it, "aN", 0, dont_panic);
	}

	if (debug_file_fp) (void)debug_unlock_it(&it);
	return (debug_file_fp != NULL);
}

/* dprintf() hit some fatal error and is going to exit. */
void
_condor_dprintf_exit( int error_code, const char* msg )
{
	char* tmp;
	FILE* fail_fp;
	char buf[DPRINTF_ERR_MAX];
	char header[DPRINTF_ERR_MAX];
	char tail[DPRINTF_ERR_MAX];
	int wrote_warning = FALSE;
	struct tm *tm;
	time_t clock_now;
	std::vector<DebugFileInfo>::iterator it;

		/* We might land here with DprintfBroken true if our call to
		   dprintf_unlock() down below hits an error.  Since the
		   "error" that it hit might simply be that there was no lock,
		   we don't want to overwrite the original dprintf error
		   message with a new one, so skip most of the following if
		   DprintfBroken is already true.
		*/
	if( !DprintfBroken ) {
		(void)time( &clock_now );

		if ( DebugUseTimestamps ) {
				// Casting clock_now to int to get rid of compile warning.
				// Probably format should be %ld, and we should cast to long
				// int, but I'm afraid of changing the output format.
				// wenger 2009-02-24.
			snprintf( header, sizeof(header), "(%d) ", (int)clock_now );
		} else {
			tm = localtime( &clock_now );
			snprintf( header, sizeof(header), "%d/%d %02d:%02d:%02d ",
					  tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, 
					  tm->tm_min, tm->tm_sec );
		}
		snprintf( header, sizeof(header), "dprintf() had a fatal error in pid %d\n", (int)getpid() );
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

		tmp = dprintf_param_funcs->param( "LOG" );
		if( tmp ) {
			snprintf( buf, sizeof(buf), "%s/dprintf_failure.%s",
					  tmp, get_mySubSystemName() );
			fail_fp = safe_fopen_wrapper_follow( buf, "wN",0644 );
			if( fail_fp ) {
				fprintf( fail_fp, "%s", header );
				fprintf( fail_fp, "%s", msg );
				if( tail[0] ) {
					fprintf( fail_fp, "%s", tail );
				}
				fclose_wrapper( fail_fp, FCLOSE_RETRY_MAX );
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
		debug_close_lock();
		debug_close_all_files();
	}

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
set_debug_flags( const char *strflags )
{
	_condor_set_debug_flags( strflags );
}


time_t
dprintf_last_modification()
{
	return DebugLastMod;
}

void
dprintf_touch_log()
{
	std::vector<DebugFileInfo>::iterator it;
	if ( _condor_dprintf_works ) {
		if (!DebugLogs->empty()) {
			it = DebugLogs->begin();
#ifdef WIN32
			utime( it->logPath.c_str(), NULL );
#else
		/* The following updates the ctime without touching 
			the mtime of the file.  This way, we can differentiate
			a "heartbeat" touch from a append touch
		*/
			chmod( it->logPath.c_str(), 0644);
#endif
		}
	}
}

BOOLEAN dprintf_retry_errno( int value );

BOOLEAN dprintf_retry_errno( int value )
{
#ifdef WIN32
	return FALSE;
#else
	return value == EINTR;
#endif
}

/* This function calls fclose(), soaking up EINTRs up to maxRetries times.
   The motivation for this function is Gnats PR 937 (DAGMan crashes if
   straced).  Psilord investigated this and found that, because LIGO
   had their dagman.out files on NFS, stracing DAGMan could interrupt
   an fclose() on the dagman.out file.  So hopefully this will fix the
   problem...   wenger 2008-07-01.
 */
int
fclose_wrapper( FILE *stream, int maxRetries )
{

	int		result = 0;

	int		retryCount = 0;
	BOOLEAN	done = FALSE;

	ASSERT( maxRetries >= 0 );
	while ( !done ) {
		if ( ( result = fclose( stream ) ) != 0 ) {
			if ( dprintf_retry_errno( errno ) && retryCount < maxRetries ) {
				retryCount++;
			} else {
				fprintf( stderr, "fclose_wrapper() failed after %d retries; "
							"errno: %d (%s)\n",
							retryCount, errno, strerror( errno ) );
				done = TRUE;
			}
		} else {
			done = TRUE;
		}
	}

	return result;
}

int _condor_mkargv( int* argc, char* argv[], char* line );

// Why the heck is this in here, rather than in mkargv.c?  wenger 2009-02-24.
// prototype
int
mkargv( int* argc, char* argv[], char* line )
{
	return( _condor_mkargv(argc, argv, line) );
}

static void
_condor_save_dprintf_line( int flags, const char* fmt, va_list args )
{
	char* buf;
	struct saved_dprintf* new_node;
	int len;

		/* figure out how much space we need to store the string */
	len = vprintf_length( fmt, args )+1; /* add 1 for the null terminator */
	if( len <= 0 ) { 
		return;
	}
		/* make a buffer to hold it and print it there */
	buf = (char *)malloc( sizeof(char) * (len + 1) );
	if( ! buf ) {
		EXCEPT( "Out of memory!" );
	}
	vsnprintf( buf, len, fmt, args );

		/* finally, make a new node in our list and save the line */
	new_node = (struct saved_dprintf *)malloc( sizeof(struct saved_dprintf) );
	if( saved_list == NULL ) {
		saved_list = new_node;
	} else {
		saved_list_tail->next = new_node;
	}
	saved_list_tail = new_node;
	new_node->next = NULL;
	new_node->level = flags;
	new_node->line = buf;
}


void
_condor_dprintf_saved_lines( void )
{
	struct saved_dprintf* node;
	struct saved_dprintf* next;

	if( ! saved_list ) {
		return;
	}

	node = saved_list;
	while( node ) {
			/* 
			   print the line.  since we've already got the complete
			   string, including all the original args, we won't have
			   any optional args or a va_list.  we're just printing a
			   string literal.  however, we want to do it with a %s so
			   that the underlying vfprintf() code doesn't try to
			   interpret any format strings that might still exist in
			   the string literal, since that'd screw us up.  we
			   definitely want to use the real dprintf() code so we
			   get the locking, potentially different log files, all
			   that stuff handled for us automatically.
			*/
		dprintf( node->level, "%s", node->line );

			/* save the next node so we don't loose it */
		next = node->next;

			/* make sure we don't leak anything */
		free( node->line );
		free( node );

		node = next;
	}

		/* now that we deallocated everything, clear out our pointer
		   to the list so it's not dangling. */
	saved_list = NULL;
}

#ifdef WIN32
static int 
lock_or_mutex_file(int fd, LOCK_TYPE type, int do_block)
{
	int result = -1;
	char * filename = NULL;
	int filename_len;
	char *ptr = NULL;
	char mutex_name[MAX_PATH];

		// If we're trying to lock NUL, just return success early
	if (strcasecmp(DebugLock, "NUL") == 0) {
		return 0;
	}

	if ( use_kernel_mutex == FALSE ) {
			// use a filesystem lock
		return lock_file_plain(fd,type,do_block);
	}
	
		// If we made it here, we want to use a kernel mutex.
		//
		// We use a kernel mutex by default to fix a major shortcoming
		// with using Win32 file locking: file locking on Win32 is
		// non-deterministic.  Thus, we have observed processes
		// starving to get the lock.  The Win32 mutex object,
		// on the other hand, is FIFO --- thus starvation is avoided.


		// first, open a handle to the mutex if we haven't already
	if ( debug_win32_mutex == NULL && DebugLock ) {
			// Create the mutex name based upon the lock file
			// specified in the config file.  				
		char * filename = strdup(DebugLock);
		filename_len = strlen(filename);
			// Note: Win32 will not allow backslashes in the name, 
			// so get rid of em here.
		ptr = strchr(filename,'\\');
		while ( ptr ) {
			*ptr = '/';
			ptr = strchr(filename,'\\');
		}
			// The mutex name is case-sensitive, but the NTFS filesystem
			// is not.  So to avoid user confusion, strlwr.
		strlwr(filename);
			// Now, we pre-append "Global\" to the name so that it
			// works properly on systems running Terminal Services
		snprintf(mutex_name,MAX_PATH,"Global\\%s",filename);
		free(filename);
		filename = NULL;
			// Call CreateMutex - this will create the mutex if it does
			// not exist, or just open it if it already does.  Note that
			// the handle to the mutex is automatically closed by the
			// operating system when the process exits, and the mutex
			// object is automatically destroyed when there are no more
			// handles... go win32 kernel!  Thus, although we are not
			// explicitly closing any handles, nothing is being leaked.
			// Note: someday, to make BoundsChecker happy, we should
			// add a dprintf subsystem shutdown routine to nicely
			// deallocate this stuff instead of relying on the OS.
		debug_win32_mutex = CreateMutex(0,FALSE,mutex_name);
	}

		// now, if we have mutex, grab it or release it as needed
	if ( debug_win32_mutex ) {
		if ( type == UN_LOCK ) {
				// release mutex
			ReleaseMutex(debug_win32_mutex);
			result = 0;	// 0 means success
		} else {
				// grab mutex
				// block 10 secs if do_block is false, else block forever
			result = WaitForSingleObject(debug_win32_mutex, 
				do_block ? INFINITE : 10 * 1000);	// time in milliseconds
				// consider WAIT_ABANDONED as success so we do not EXCEPT
			if ( result==WAIT_OBJECT_0 || result==WAIT_ABANDONED ) {
				result = 0;
			} else {
				result = -1;
			}
		}

	}

	return result;
}
#endif  // of Win32

#ifndef WIN32
static int ParentLockFd = -1;

void
dprintf_before_shared_mem_clone() {
	ParentLockFd = LockFd;
}

void
dprintf_after_shared_mem_clone() {
	LockFd = ParentLockFd;
}

void
dprintf_init_fork_child( ) {
	if( LockFd >= 0 ) {
		close( LockFd );
		LockFd = -1;
	}
}

void
dprintf_wrapup_fork_child( ) {
		/* Child pledges not to call dprintf any more, so it is
		   safe to close the lock file.  If parent closes all
		   fds anyway, then this is redundant.
		*/
	if( LockFd >= 0 ) {
		close( LockFd );
		LockFd = -1;
	}
}

#if HAVE_BACKTRACE

static void
safe_async_simple_fwrite_fd(int fd,char const *msg,unsigned int *args,unsigned int num_args)
{
	unsigned int arg_index;
	unsigned int digit,arg;
	char intbuf[50];
	char *intbuf_pos;

	for(;*msg;msg++) {
		if( *msg != '%' ) {
			write(fd,msg,1);
		}
		else {
				// format is % followed by index of argument in args array
			arg_index = *(++msg)-'0';
			if( arg_index >= num_args || !*msg ) {
				write(fd," INVALID! ",10);
				break;
			}
			arg = args[arg_index];
			intbuf_pos=intbuf;
			do {
				digit = arg % 10;
				*(intbuf_pos++) = digit + '0';
				arg /= 10;  // integer division, shifts base-10 digits right
			} while( arg ); // terminate when no more non-zero digits

				// intbuf now contains the base-10 digits of arg
				// in order of least to most significant
			while( intbuf_pos-- > intbuf ) {
				write(fd,intbuf_pos,1);
			}
		}
	}
}

void
dprintf_dump_stack(void) {
	priv_state	orig_priv_state;
	int orig_euid;
	int orig_egid;
	int fd;
	void *trace[50];
	int trace_size;
	unsigned int args[3];

		/* In case we are dumping stack in the segfault handler, we
		   want this to be as simple as possible.  Calling malloc()
		   could be fatal, since the heap may be trashed.  Therefore,
		   we dispense with some of the formalities... */

	if (DprintfBroken || !_condor_dprintf_works || DebugLogs->empty()) {
			// Note that although this would appear to enable
			// backtrace printing to stderr before dprintf is
			// configured, the backtrace sighandler is only installed
			// when dprintf is configured, so we won't even get here
			// in that case.  Therefore, most command-line tools need
			// -debug to enable the backtrace.
		fd = 2;
	}
	else {
			// set_priv() is unsafe, because it may call into
			// the password cache, which may call unsafe functions
			// such as getpwuid() or initgroups() or malloc().
		orig_euid = geteuid();
		orig_egid = getegid();
		orig_priv_state = get_priv_state();
		if( orig_priv_state != PRIV_CONDOR ) {
				// To keep things simple, rather than trying to become
				// the correct condor id, just switch to our real
				// user id, which is probably either the same as
				// our effective id (no-op) or root.
			setegid(getgid());
			seteuid(getuid());
		}

		fd = safe_open_wrapper_follow(DebugLogs->begin()->logPath.c_str(),O_APPEND|O_WRONLY|O_CREAT,0644);

		if( orig_priv_state != PRIV_CONDOR ) {
			setegid(orig_egid);
			seteuid(orig_euid);
		}

		if( fd==-1 ) {
			fd=2;
		}
	}

	trace_size = backtrace(trace,50);

		// sprintf() and other convenient string-handling functions
		// are not officially async-signal safe, so use a crude replacement
	args[0] = (unsigned int)getpid();
	args[1] = (unsigned int)time(NULL);
	args[2] = (unsigned int)trace_size;
	safe_async_simple_fwrite_fd(fd,"Stack dump for process %0 at timestamp %1 (%2 frames)\n",args,3);

	backtrace_symbols_fd(trace,trace_size,fd);

	if (fd!=2) {
		close(fd);
	}
}
#endif

#endif

bool debug_open_fds(std::map<int,bool> &open_fds)
{
	bool found = false;
	std::vector<DebugFileInfo>::iterator it;

	for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
	{
		if(!it->debugFP)
			continue;
		open_fds.insert(std::pair<int,bool>(fileno(it->debugFP),true));
		found = true;
	}

	return found;
}

#if !defined(HAVE_BACKTRACE)
void
dprintf_dump_stack(void) {
		// this platform does not support backtrace()
}
#endif
