#ifndef FIX_STDIO_H
#define FIX_STDIO_H

#include <stdio.h>


/*
  For some reason the stdio.h on Ultrix 4.3 fails to provide a prototype
  for pclose() if _POSIX_SOURCE is defined - even though it does
  provide a prototype for popen().
*/
#if defined(ULTRIX43)

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)
int  pclose( FILE *__stream );
#else
int  pclose();
#endif

#if defined(__cplusplus)
}
#endif

#endif	/* ULTRIX43 */

#endif
