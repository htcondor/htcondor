#ifndef FIX_UNISTD_H
#define FIX_UNISTD_H

#include <unistd.h>


/*
  For some reason the g++ include files on Ultrix 4.3 fail to provide
  these prototypes, even though the Ultrix 4.2 versions did...
  Once again, OSF1 also chokes, unless _AES_SOURCE(?) is defined JCP
*/
#if defined(ULTRIX43) || defined(SUNOS41) || defined(OSF1)

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(SUNOS41) || defined(ULTRIX43)
	typedef unsigned long ssize_t;
#endif

#if defined(__STDC__) || defined(__cplusplus)
	int symlink( const char *, const char * );
	void *sbrk( ssize_t );
	int gethostname( char *, int );
#else
	int symlink();
	char *sbrk();
	int gethostname();
#endif

#if defined(__cplusplus)
}
#endif

#endif	/* ULTRIX43 */

#endif
