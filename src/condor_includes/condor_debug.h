/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
#define D_MALLOC		(1<<4)
#define D_LOAD			(1<<5)
#define D_EXPR			(1<<6)
#define D_PROC			(1<<7)
#define D_JOB			(1<<8)
#define D_MACHINE		(1<<9)
#define D_FULLDEBUG	 	(1<<10)
#define D_NFS			(1<<11)
#define D_UPDOWN        (1<<12)
#define D_AFS           (1<<13)
#define D_PREEMPT		(1<<14)
#define D_PROTOCOL		(1<<15)
#define D_PRIV			(1<<16)
#define D_SECURITY		(1<<17)
#define D_DAEMONCORE	(1<<18)
#define D_COMMAND		(1<<19)
#define D_BANDWIDTH		(1<<20)
#define D_NETWORK		(1<<21)
#define D_KEYBOARD		(1<<22)
#define D_PROCFAMILY	(1<<23)
#define D_IDLE			(1<<24)
#define D_MATCH			(1<<25)
#define D_ACCOUNTANT	(1<<26)
#define D_FAILURE	(1<<27)
#define D_FDS           (1<<(D_MAXFLAGS-3))
#define D_SECONDS		(1<<(D_MAXFLAGS-2))
#define D_NOHEADER		(1<<(D_MAXFLAGS-1))
#define D_ALL			(~(0) & (~(D_NOHEADER)))

#if defined(__cplusplus)
extern "C" {
#endif

extern int DebugFlags;	/* Bits to look for in dprintf */
extern int Termlog;		/* Are we logging to a terminal? */
extern int (*DebugId)(FILE *);		/* set header message */

void dprintf ( int flags, char *fmt, ... );
void dprintf_config( char *subsys, int logfd );
void _condor_dprintf_va ( int flags, char* fmt, va_list args );
int _condor_open_lock_file(const char *filename,int flags, mode_t perm);
void _EXCEPT_ ( char *fmt, ... );
void Suicide();
void set_debug_flags( char *strflags );
void _condor_fd_panic( int line, char *file );

/*
**	Definition of exception macro
*/
#define EXCEPT \
	_EXCEPT_Line = __LINE__; \
	_EXCEPT_File = __FILE__; \
	_EXCEPT_Errno = errno; \
	_EXCEPT_

/*
**	Important external variables in libc
*/
extern DLL_IMPORT_MAGIC int		errno;
#if !( defined(LINUX) && defined(GLIBC) || defined(Darwin) )
extern DLL_IMPORT_MAGIC int		sys_nerr;
extern DLL_IMPORT_MAGIC char		*sys_errlist[];
#endif

extern int	_EXCEPT_Line;			/* Line number of the exception    */
extern char	*_EXCEPT_File;			/* File name of the exception      */
extern int	_EXCEPT_Errno;			/* errno from most recent system call */
extern int (*_EXCEPT_Cleanup)(int,int,char*);	/* Function to call to clean up (or NULL) */
extern void _EXCEPT_(char*, ...);

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
