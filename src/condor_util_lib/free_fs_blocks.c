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

#include <stdio.h>

#if defined(ULTRIX42) || defined(ULTRIX43)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/types.h>


#if defined(IRIX331)
#include <sys/statfs.h>
#elif !defined(OSF1)
#include <sys/vfs.h>
#endif


#endif

#include "debug.h"

#define LOTS_OF_FREE 50000

extern int	errno;

#if defined(ULTRIX42) || defined(ULTRIX43)
int
free_fs_blocks(filename)
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

	dprintf(D_FULLDEBUG, "number of kbytes available for filename(%s): %d\n", 
			filename, free_kbytes);

	return(free_kbytes);
}
#endif /* MIPS && ULTRIX */

#if defined(VAX) && defined(ULTRIX)
/* statfs() did not work on our VAX_ULTRIX machine.  use getmnt() instead. */
int
free_fs_blocks(filename)
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

#if (defined(I386) && defined(DYNIX)) || (defined(VAX) && defined(BSD43)) || (defined(MC68020) && defined(SUNOS41)) || (defined(IBM032) && defined(BSD43)) || (defined(MC68020) && defined(BSD43)) || (defined(SPARC) && defined(SUNOS41)) || (defined(R6000) && defined(AIX31)) || defined(AIX32) || defined(IRIX331) || (defined(SPARC) && defined(CMOS)) || defined(HPUX8)

#if defined(AIX31) || defined(AIX32)
#include <sys/statfs.h>
#endif

#ifdef IRIX331
#define f_bavail f_bfree
#endif

int
free_fs_blocks(filename)
char *filename;
{
	struct statfs statfsbuf;
	int free_kbytes;

#ifdef IRIX331
	if(statfs(filename, &statfsbuf, sizeof statfsbuf, 0) < 0) {
#else
	if(statfs(filename, &statfsbuf) < 0) {
#endif
		dprintf(D_ALWAYS, "free_fs_blocks: statfs(%s,0x%x) failed\n",
													filename, &statfsbuf );
		dprintf(D_ALWAYS, "errno = %d\n", errno );
		return(LOTS_OF_FREE);
	}

	/* Convert to kbyte blocks: available blks * blksize / 1k bytes. */
	free_kbytes = statfsbuf.f_bavail * statfsbuf.f_bsize / 1024;

	dprintf(D_FULLDEBUG, "number of kbytes available for filename(%s): %d\n", 
			filename, free_kbytes);

	return(free_kbytes);
}
#endif /* I386 && DYNIX */

