#ifndef FIX_UNISTD_H
#define FIX_UNISTD_H

/**********************************************************************
** Stuff that needs to be hidden or defined before unistd.h is
** included on various platforms. 
**********************************************************************/

#if defined(SUNOS41)
#	define read _hide_read
#	define write _hide_write
#endif

#if defined(AIX32)
#	define execv __hide_execv
#endif

#if defined(IRIX53) && !defined(IRIX62)
struct timeval;
#endif

#if defined(IRIX62)
#if !defined(_SYS_SELECT_H)
typedef struct fd_set fd_set;
#endif
#endif /* IRIX62 */

#if defined(LINUX)
#	define idle _hide_idle
#	define __USE_BSD
#endif

/**********************************************************************
** Actually include the file
**********************************************************************/
#include <unistd.h>

/**********************************************************************
** Clean-up
**********************************************************************/
#if defined(SUNOS41)
#	undef read
#	undef write
#endif

#if defined(AIX32)
#	undef execv
	int execv(const char *path, char *const argv[]);
#endif

#if defined(LINUX)
#   undef idle
#endif

/**********************************************************************
** Things that should be defined in unistd.h that for whatever reason
** aren't defined on various platforms
**********************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

#if defined(SUNOS41) 
	typedef unsigned long ssize_t;
	ssize_t read( int, void *, size_t );
	ssize_t write( int, const void *, size_t );
#endif

#if defined(SUNOS41) || defined(OSF1)
	int symlink( const char *, const char * );
	void *sbrk( ssize_t );
	int gethostname( char *, int );
#endif

#if defined(Solaris)
	int gethostname( char *, int );
#endif


#if defined(__cplusplus)
}
#endif

#endif FIX_UNISTD_H
