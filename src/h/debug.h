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
**	Definitions for flags to pass to dprintf
*/
#define D_ALWAYS	(1<<0)
#define D_TERMLOG	(1<<1)
#define D_SYSCALLS	(1<<2)
#define D_CKPT		(1<<3)
#define D_XDR		(1<<4)
#define D_MALLOC	(1<<5)
#define D_NOHEADER	(1<<6)
#define D_LOAD		(1<<7)
#define D_EXPR		(1<<8)
#define D_PROC		(1<<9)
#define D_JOB		(1<<10)
#define D_MACHINE	(1<<11)
#define D_FULLDEBUG	(1<<12)
#define D_NFS		(1<<13)
#define D_UPDOWN        (1<<14)
#define D_AFS           (1<<15)
#define D_PREEMPT	(1<<16)
#define D_PROTOCOL	(1<<17)
#define D_PRIV		(1<<18)
#define D_TAPENET	(1<<19)
#define D_DAEMONCORE (1<<20)
#define D_COMMAND 	(1<<20)
#define D_MAXFLAGS	32

#define D_ALL		(~D_NOHEADER)

#include <stdio.h>

/*
**	Important external variables...
*/

extern int	errno;
#if !( defined(LINUX) && defined(GLIBC) )
extern int	sys_nerr;
extern char	*sys_errlist[];
#endif

extern int DebugFlags;	/* Bits to look for in dprintf                       */
extern int MaxLog;		/* Maximum size of log file (if D_TRUNCATE is set)   */
extern char *DebugFile;	/* Name of the log file (or NULL)                    */
extern char *DebugLock;	/* Name of the lock file (or NULL)                   */
extern int (*DebugId)();/* Function to call to print special info (or NULL)  */
extern FILE *DebugFP;	/* The FILE to perform output to                     */

extern char *DebugFlagNames[];

#ifdef MALLOC_DEBUG
#define MALLOC(size) mymalloc(__FILE__,__LINE__,size)
#define CALLOC(nelem,size) mycalloc(__FILE__,__LINE__,nelem,size)
#define REALLOC(ptr,size) myrealloc(__FILE__,__LINE__,ptr,size)
#define FREE(ptr) myfree(__FILE__,__LINE__,ptr)
#if defined(__cplusplus)
extern "C" {
#endif
char	*mymalloc(), *myrealloc(), *mycalloc();
#if defined(__cplusplus)
}
#endif
#else
#define MALLOC(size) malloc(size)
#define CALLOC(nelem,size) calloc(nelem,size)
#define REALLOC(ptr,size) realloc(ptr,size)
#define FREE(ptr) free(ptr)
#endif

#define D_RUSAGE( flags, ptr ) { \
	dprintf( flags, "(ptr)->ru_utime = %d.%06d\n", (ptr)->ru_utime.tv_sec, \
											(ptr)->ru_utime.tv_usec ); \
	dprintf( flags, "(ptr)->ru_stime = %d.%06d\n", (ptr)->ru_stime.tv_sec, \
											(ptr)->ru_stime.tv_usec ); \
}
