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


#ifndef CONDOR_DEBUG_H
#define CONDOR_DEBUG_H

#if !defined(__STDC__) && !defined(__cplusplus)
#define const
#endif

/*
**	Definitions for flags to pass to dprintf
**  Note: this is a little confusing, since the flags specify both
**  debug levels and dprintf options (i.e., D_NOHEADER).  The debug
**  level flags use the lower order bits while the option flag(s)
**  use the higher order bit(s).  Note that D_MAXFLAGS is 32 so we
**  can store the debug level as a integer bitmask.  When adding a
**  debug flag, be sure to update D_NUMLEVELS.  Since we start
**  counting levels at 0, D_NUMLEVELS should be one greater than the
**  highest level.
*/
#define D_NUMLEVELS		28
#define D_MAXFLAGS 		32
#define D_ALWAYS 		(1<<0)
#define D_SYSCALLS		(1<<1)
#define D_CKPT			(1<<2)
#define D_HOSTNAME		(1<<3)
#define D_UNUSED1		(1<<4)
#define D_LOAD			(1<<5)
#define D_EXPR			(1<<6)
#define D_PROC			(1<<7)
#define D_JOB			(1<<8)
#define D_MACHINE		(1<<9)
#define D_FULLDEBUG	 	(1<<10)
#define D_NFS			(1<<11)
#define D_CONFIG        (1<<12)
#define D_UNUSED2       (1<<13)
#define D_PREEMPT		(1<<14)
#define D_PROTOCOL		(1<<15)
#define D_PRIV			(1<<16)
#define D_SECURITY		(1<<17)
#define D_DAEMONCORE	(1<<18)
#define D_COMMAND		(1<<19)
#define D_MATCH			(1<<20)
#define D_NETWORK		(1<<21)
#define D_KEYBOARD		(1<<22)
#define D_PROCFAMILY	(1<<23)
#define D_IDLE			(1<<24)
#define D_THREADS		(1<<25)
#define D_ACCOUNTANT	(1<<26)
#define D_FAILURE	(1<<27)
/* 
   the rest of these aren't debug levels, but are format-modifying
   flags to change the appearance of the dprintf line
*/ 
#define D_PID           (1<<28)
#define D_FDS           (1<<29)
#define D_UNUSED3       (1<<30)
#define D_NOHEADER      (1<<31)
#define D_ALL           (~(0) & (~(D_NOHEADER)))

#if defined(__cplusplus)
extern "C" {
#endif

extern int DebugFlags;	/* Bits to look for in dprintf */
extern int Termlog;		/* Are we logging to a terminal? */
extern int (*DebugId)(FILE *);		/* set header message */

void dprintf ( int flags, const char *fmt, ... ) CHECK_PRINTF_FORMAT(2,3);

void dprintf_config( const char *subsys );
void _condor_dprintf_va ( int flags, const char* fmt, va_list args );
int _condor_open_lock_file(const char *filename,int flags, mode_t perm);
void _EXCEPT_ ( const char *fmt, ... ) CHECK_PRINTF_FORMAT(1,2);
void Suicide(void);
void set_debug_flags( const char *strflags );
void _condor_fd_panic( int line, char *file );
void _condor_set_debug_flags( const char *strflags );

/* must call this before clone(CLONE_VM|CLONE_VFORK) */
void dprintf_before_shared_mem_clone( void );

/* must call this after clone(CLONE_VM|CLONE_VFORK) returns */
void dprintf_after_shared_mem_clone( void );

/* must call this upon entering child of fork() if child calls dprintf */
void dprintf_init_fork_child( void );

/* call this when done with dprintf in child of fork()
 * This is not necessary if child is just going to exit.  It just
 * ensures that nothing gets inherited by exec().
 */
void dprintf_wrapup_fork_child( void );

void dprintf_dump_stack(void);

time_t dprintf_last_modification(void);
void dprintf_touch_log(void);

/* wrapper for fclose() that soaks up EINTRs up to maxRetries number of times.
 */
int fclose_wrapper( FILE *stream, int maxRetries );

/*
**	Definition of exception macro
*/
#define EXCEPT \
	_EXCEPT_Line = __LINE__, \
	_EXCEPT_File = __FILE__,				\
	_EXCEPT_Errno = errno,					\
	_EXCEPT_

/*
**	Important external variables in libc
*/
#if !( defined(LINUX) && defined(GLIBC) || defined(Darwin) || defined(CONDOR_FREEBSD) )
extern DLL_IMPORT_MAGIC int		errno;
extern DLL_IMPORT_MAGIC int		sys_nerr;
#if _MSC_VER < 1400 /* VC++ 2005 version */
extern DLL_IMPORT_MAGIC char	*sys_errlist[];
#endif
#endif

extern int	_EXCEPT_Line;			/* Line number of the exception    */
extern const char	*_EXCEPT_File;		/* File name of the exception      */
extern int	_EXCEPT_Errno;			/* errno from most recent system call */
extern int (*_EXCEPT_Cleanup)(int,int,char*);	/* Function to call to clean up (or NULL) */
extern void _EXCEPT_(const char*, ...) CHECK_PRINTF_FORMAT(1,2);

#if defined(__cplusplus)
}
#endif

#ifndef CONDOR_ASSERT
#define CONDOR_ASSERT(cond) \
	if( !(cond) ) { EXCEPT("Assertion ERROR on (%s)",#cond); }
#endif /* CONDOR_ASSERT */

#ifndef ASSERT
#	define ASSERT(cond) CONDOR_ASSERT(cond)
#endif /* ASSERT */

#ifndef TRACE
#define TRACE \
	fprintf( stderr, "TRACE at line %d in file \"%s\"\n",  __LINE__, __FILE__ );
#endif /* TRACE */

#ifdef MALLOC_DEBUG
#define MALLOC(size) mymalloc(__FILE__,__LINE__,size)
#define CALLOC(nelem,size) mycalloc(__FILE__,__LINE__,nelem,size)
#define REALLOC(ptr,size) myrealloc(__FILE__,__LINE__,ptr,size)
#define FREE(ptr) myfree(__FILE__,__LINE__,ptr)
char    *mymalloc(), *myrealloc(), *mycalloc();
#else
#define MALLOC(size) malloc(size)
#define CALLOC(nelem,size) calloc(nelem,size)
#define REALLOC(ptr,size) realloc(ptr,size)
#define FREE(ptr) free(ptr)
#endif /* MALLOC_DEBUG */

#define D_RUSAGE( flags, ptr ) { \
        dprintf( flags, "(ptr)->ru_utime = %d.%06d\n", (ptr)->ru_utime.tv_sec,\
        (ptr)->ru_utime.tv_usec ); \
        dprintf( flags, "(ptr)->ru_stime = %d.%06d\n", (ptr)->ru_stime.tv_sec,\
        (ptr)->ru_stime.tv_usec ); \
}

#endif /* CONDOR_DEBUG_H */

/* 
 * On Win32, define assert() to really do an Condor ASSERT(), which does EXCEPT.
 * NOTE THIS MUST BE PLACED AFTER THE #endif TO CONDOR_DEBUG_H because
 * this redefinition needs to be done more than once (because the WinNT header
 * files could redefine assert more than once... sigh).
*/
#ifdef WIN32
#	ifdef assert
#		undef assert
#	endif
#	ifdef ASSERT
#		undef ASSERT
#	endif
#	define ASSERT(cond) CONDOR_ASSERT(cond)
#	define assert(cond) CONDOR_ASSERT(cond)
#endif	/* of ifdef WIN32 */
