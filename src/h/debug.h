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
#define D_MAXFLAGS	32

#define D_ALL		(~D_NOHEADER)

/*
**	Important external variables...
*/

extern int errno;

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
char	*mymalloc(), *myrealloc(), *mycalloc();
#else
#define MALLOC(size) malloc(size)
#define CALLOC(nelem,size) calloc(nelem,size)
#define REALLOC(ptr,size) realloc(ptr,size)
#define FREE(ptr) free(ptr)
#endif

#if 0
#if !defined(AIX32) && !defined(OSF1)
char	*malloc(), *realloc(), *calloc();
#endif
#endif

#if defined(AIX31) || defined(AIX32)
#define BIN_MAIL "/usr/bin/mail"
#else
#define BIN_MAIL "/bin/mail"
#endif

#define D_RUSAGE( flags, ptr ) { \
	dprintf( flags, "(ptr)->ru_utime = %d.%06d\n", (ptr)->ru_utime.tv_sec, \
											(ptr)->ru_utime.tv_usec ); \
	dprintf( flags, "(ptr)->ru_stime = %d.%06d\n", (ptr)->ru_stime.tv_sec, \
											(ptr)->ru_stime.tv_usec ); \
}
