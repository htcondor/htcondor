/*
  N.B. Every file which includes this header will need to contain the
  following line:

	static char *_FileName_ = __FILE__;
*/

#ifndef DEBUG_H
#define DEBUG_H

extern int DebugFlags;	/* Bits to look for in dprintf                       */

#if !defined(__STDC__) && !defined(__cplusplus)
#define const
#endif

/*
**	Definitions for flags to pass to dprintf
*/
static const int D_ALWAYS		= (1<<0);
static const int D_TERMLOG		= (1<<1);
static const int D_SYSCALLS		= (1<<2);
static const int D_CKPT			= (1<<3);
static const int D_XDR			= (1<<4);
static const int D_MALLOC		= (1<<5);
static const int D_NOHEADER		= (1<<6);
static const int D_LOAD			= (1<<7);
static const int D_EXPR			= (1<<8);
static const int D_PROC			= (1<<9);
static const int D_JOB			= (1<<10);
static const int D_MACHINE		= (1<<11);
static const int D_FULLDEBUG	= (1<<12);
static const int D_NFS			= (1<<13);
static const int D_MAXFLAGS		= 32;
static const int D_ALL			= (~(1<<6));

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)
void dprintf_init ( int fd );
void dprintf ( int flags, char *fmt, ... );
void _EXCEPT_ ( char *fmt, ... );
#else
void config ();
char * param ();
void _EXCEPT_ ();
#endif

#if defined(__cplusplus)
}
#endif

/*
**	Definition of exception macro
*/
#define EXCEPT \
	_EXCEPT_Line = __LINE__; \
	_EXCEPT_File = _FileName_; \
	_EXCEPT_Errno = errno; \
	_EXCEPT_

/*
**	Important external variables
*/
extern int	errno;

extern int	_EXCEPT_Line;			/* Line number of the exception    */
extern char	*_EXCEPT_File;			/* File name of the exception      */
extern int	_EXCEPT_Errno;			/* errno from most recent system call */
extern int (*_EXCEPT_Cleanup)();	/* Function to call to clean up (or NULL) */

#endif

#ifndef ASSERT
#define ASSERT(cond) \
	if( !(cond) ) { EXCEPT("Assertion ERROR on (%s)",#cond); }
#endif

#ifndef TRACE
#define TRACE \
	fprintf( stderr, "TRACE at line %d in file \"%s\"\n",  __LINE__, __FILE__ );
#endif

#if defined(AIX31) || defined(AIX32)
#define BIN_MAIL "/usr/bin/mail"
#else
#define BIN_MAIL "/bin/mail"
#endif
