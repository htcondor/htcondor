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
** Author:  Rick Rasmussen
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*
** free_fs_blocks: reports the number of kbytes free for the filesystem where
** given filename resides.
*/

#if defined(WIN32)

#include "condor_common.h"
#include <limits.h>

int
free_fs_blocks(char *filename)
{
	ULARGE_INTEGER FreeBytesAvailableToCaller, TotalNumberOfBytes, TotalNumberOfFreeBytes;
	if (GetDiskFreeSpaceEx(filename, &FreeBytesAvailableToCaller, &TotalNumberOfBytes, 
		&TotalNumberOfFreeBytes) == 0) {
		return -1;
	} else {
		if (FreeBytesAvailableToCaller.HighPart > 0) {
			return INT_MAX;
		} else {
			return (int)(FreeBytesAvailableToCaller.LowPart/1024);
		}
	}
}

#else

#include <stdio.h>
#include <string.h>

#if defined(ULTRIX42) || defined(ULTRIX43)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#else	/* ULTRIX */
#include <sys/types.h>


#if defined(IRIX331) || defined(IRIX53)
#include <sys/statfs.h>
#elif !defined(OSF1)
#include <sys/vfs.h>
#endif


#endif	/* ULTRIX */

#include "debug.h"

#define TRUE 1
#define FALSE 0

#define LOTS_OF_FREE 50000

extern int	errno;
char	*param();
static char	*Afs_Cache_Name;


#define FS_PROGRAM "/usr/afsws/bin/fs"
#define FS_COMMAND "getcacheparms"
#define FS_OUTPUT_FORMAT "\nAFS using %d of the cache's available %d"

#if defined(__STDC__)
FILE * my_popen( char * cmd, char * mode );
int my_pclose( FILE *fp );
#else
FILE * my_popen();
int my_pclose();
#endif

/*
  How much disk space we need to reserve for the AFS cache.  Answer is
  in kilobytes.
*/
int
reserve_for_afs_cache()
{
	int		answer;
	FILE	*fp;
	char	cmd[512];
	int		cache_size, cache_in_use;
	static int	do_it = -1;
	char	*str;

		/* See if we're configured to deal with an AFS cache */
	if( do_it < 0 ) {
		str = param( "RESERVE_AFS_CACHE" );
		if( str && (*str == 'T' || *str == 't')) {
			do_it = TRUE;
		} else {
			do_it = FALSE;
		}
		if( str ) {
			free( str );
		}
	}

		/* If we're not configured to deal with AFS cache, just return 0 */
	if( !do_it ) {
		return 0;
	}

		/* Run an AFS utility program to learn how big the cache is and
		   how much is in use. */
	dprintf( D_FULLDEBUG, "Checking AFS cache parameters\n" );
	sprintf( cmd, "%s %s", FS_PROGRAM, FS_COMMAND );
	fp = my_popen( cmd, "r" );
	fscanf( fp, FS_OUTPUT_FORMAT, &cache_in_use, &cache_size );
	my_pclose( fp );
	dprintf( D_FULLDEBUG, "cache_in_use = %d, cache_size = %d\n",
		cache_in_use,
		cache_size
	);
	answer = cache_size - cache_in_use;

		/* The cache may be temporarily over size, in this case AFS will
		   clean up itself, so we don't have to allow for that. */
	if( answer < 0 ) {
		answer = 0;
	}

	dprintf( D_FULLDEBUG, "Reserving %d kbytes for AFS cache\n", answer );
	return answer;
}

/*
  How much disk space we need to reserve for the regular file system.
  Answer is in kilobytes.
*/
int
reserve_for_fs()
{
	static int	answer = -1;
	char	*str;

	if( answer < 0 ) {
		dprintf( D_ALWAYS, "Looking up RESERVED_DISK parameter\n" );
		str = param( "RESERVED_DISK" );
		if( str ) {
			answer = atoi( str ) * 1024;	/* Parameter is in meg */
			free( str );
		}
		if( answer < 0 ) {
			answer = 0;
		}
		dprintf( D_ALWAYS, "Reserving %d kbytes for file system\n", answer );
	}
	dprintf( D_FULLDEBUG, "Reserving %d kbytes for file system\n", answer );
	return answer;
}


#if defined(__STDC__)
int raw_free_fs_blocks( char *filename);
#else
int raw_free_fs_blocks();
#endif

/*
  Return number of kbytes condor may play with in the named file
  system.  System administrators may reserve space which condor should
  leave alone.  This could be either a static amount of space, or an
  amount of space based on the current size of the AFS cache (or
  both).

  Note: This code assumes that if we have an AFS cache, that cache is
  on the filesystem we are asking about.  This is not true in general,
  but it is the case at the developer's home site.  A more correct
  implementation would check whether the filesystem being asked about
  is the one containing the cache.  This is messy to do if the
  filesystem name we are given isn't a full pathname, e.g.
  free_fs_blocks("."), which is why it isn't implemented here. -- mike
*/
int
free_fs_blocks(filename)
char *filename;
{
	int		answer;
	answer =  raw_free_fs_blocks(filename)
			- reserve_for_afs_cache()
			- reserve_for_fs();
	return answer < 0 ? 0 : answer;
}

#if defined(ULTRIX42) || defined(ULTRIX43)
int
raw_free_fs_blocks(filename)
char *filename;
{
	struct fs_data statfsbuf;
	struct fs_data_req *fs;
	int free_kbytes;

	if(statfs(filename, &statfsbuf) < 0) {
		dprintf(D_ALWAYS, "free_fs_blocks: statfs(%s,0x%x) failed\n",
													filename, &statfsbuf );
		return(LOTS_OF_FREE);
	}

	fs = &statfsbuf.fd_req;

	/* bfreen is in kbytes */
	free_kbytes = fs->bfreen;

	dprintf(D_FULLDEBUG, "number of kbytes available for (%s): %d\n", 
			filename, free_kbytes);

	return(free_kbytes);
}
#endif /* MIPS && ULTRIX */

#if defined(VAX) && defined(ULTRIX)
/* statfs() did not work on our VAX_ULTRIX machine.  use getmnt() instead. */
int
raw_free_fs_blocks(filename)
char *filename;
{
	struct fs_data mntdata;
	int start;
	struct fs_data_req *fs_req;
	int free_kbytes;

	if( getmnt(&start, &mntdata, sizeof(mntdata), NOSTAT_ONE, filename) < 0) {
		dprintf(D_ALWAYS, "free_fs_blocks(): getmnt failed");
		return(LOTS_OF_FREE);
	}
	
	fs_req = &mntdata.fd_req;

	/* bfreen is in kbytes. */
	free_kbytes = fs_req->bfreen;

	dprintf(D_FULLDEBUG, "number of kbytes available for filename: %d\n", 
			free_kbytes);

	return(free_kbytes);
}
#endif /* VAX && ULTRIX */

#if (defined(I386) && defined(DYNIX)) || (defined(I386) && defined(LINUX)) || (defined(VAX) && defined(BSD43)) || (defined(MC68020) && defined(SUNOS41)) || (defined(IBM032) && defined(BSD43)) || (defined(MC68020) && defined(BSD43)) || (defined(SPARC) && defined(SUNOS41)) || (defined(R6000) && defined(AIX31)) || defined(AIX32) || defined(IRIX331) || (defined(SPARC) && defined(CMOS)) || defined(HPUX9) || defined(OSF1) || defined(Solaris) || defined(IRIX53)

#if defined(AIX31) || defined(AIX32) || defined(IRIX53)
#include <sys/statfs.h>
#elif defined(Solaris)
#include <sys/statvfs.h>
#elif defined(OSF1)
#include <sys/mount.h>
#endif


#if defined(IRIX331) || defined(Solaris) || defined(IRIX53)
#define f_bavail f_bfree
#endif

int
raw_free_fs_blocks(filename)
char *filename;
{
#if defined(Solaris)
	struct statvfs statfsbuf;
#else
	struct statfs statfsbuf;
#endif
	unsigned long free_kbytes;

#if defined(IRIX331) || defined(IRIX53)
	if(statfs(filename, &statfsbuf, sizeof statfsbuf, 0) < 0) {
#elif defined(OSF1)
	if(statfs(filename, &statfsbuf, sizeof statfsbuf) < 0) {
#elif defined(Solaris)
	if(statvfs(filename, &statfsbuf) < 0) {
#else
	if(statfs(filename, &statfsbuf) < 0) {
#endif
		dprintf(D_ALWAYS, "free_fs_blocks: statfs(%s,0x%x) failed\n",
													filename, &statfsbuf );
		dprintf(D_ALWAYS, "errno = %d\n", errno );
		return(LOTS_OF_FREE);
	}

	/* Convert to kbyte blocks: available blks * blksize / 1k bytes. */
	/* fix the overflow problem. weiru */
	free_kbytes = (unsigned long)statfsbuf.f_bavail * (unsigned long)statfsbuf.f_bsize / 1024;
	if(free_kbytes > 0x7fffffff)
	{
		dprintf(D_ALWAYS, "Too much free disk space, return LOTS_OF_FREE\n");
		return(LOTS_OF_FREE);
	}

	dprintf(D_FULLDEBUG, "number of kbytes available for (%s): %d\n", 
			filename, free_kbytes);

	return(free_kbytes);
}
#endif /* I386 && DYNIX */

#endif /* !WIN32 */
