#ifndef FIX_STDIO_H
#define FIX_STDIO_H

#include <stdio.h>



#if defined(__cplusplus)
extern "C" {
#endif

/*
  For some reason the stdio.h on OSF1 fails to provide prototypes
  for popen() and pclose() if _POSIX_SOURCE is defined.
*/
#if defined(OSF1)
#if defined(__STDC__) || defined(__cplusplus)
	FILE *popen( char *, char * );
	int  pclose( FILE *__stream );
	extern void     setbuffer(FILE*, char*, int);
	extern void     setlinebuf(FILE*);
#else
	FILE *popen();
	int  pclose();
	extern void     setbuffer();
	extern void     setlinebuf();
#endif
#endif	/* OSF1 */

/*
  For some reason the stdio.h on Ultrix 4.3 fails to provide a prototype
  for pclose() if _POSIX_SOURCE is defined - even though it does
  provide a prototype for popen().
*/
#if defined(ULTRIX43) || defined(SUNOS41) || defined(ULTRIX42)
#if defined(__STDC__) || defined(__cplusplus)
	int  pclose( FILE *__stream );
#else
	int  pclose();
#endif
#endif	/* ULTRIX43 */

#if defined(__cplusplus)
}
#endif


#endif
