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
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dir.h>

#include "condor_getmnt.h"

char			*strdup(), *malloc();

/*
 The function getmnt() is Ultrix specific and returns a different
 structure than the similar functions available on other UNIX
 variants.  We simulate "getmnt()" by calling the appropriate platform
 specific library routines.  N.B. we simulate getmnt() only to the
 extent needed specifically by condor.  This is NOT a generally correct
 simulation.
*/

#if defined(ULTRIX42) || defined(ULTRIX43)

	/* Nothing needed on ULTRIX systems - getmnt() is native*/

#elif defined(OSF1)

	/* BEGIN OSF1 version - use getmntinfo() */

#include <sys/stat.h>
#include <sys/mount.h>

getmnt( start, buf, bufsize, mode, path )
int				*start;
struct fs_data	buf[];
unsigned		bufsize;
int				mode;
char			*path;
{
	struct statfs	*data = NULL;
	struct stat	st_buf;
	int		n_entries;
	int		i;
	dev_t	dev;

	if( (n_entries=getmntinfo(&data,MNT_NOWAIT)) < 0 ) {
		perror( "getmntinfo" );
		exit( 1 );
	}

	for( i=0; i<n_entries; i++ ) {
		if( stat(data[i].f_mntonname,&st_buf) < 0 ) {
			buf[i].fd_req.dev = 0;
		} else {
			buf[i].fd_req.dev = st_buf.st_dev;
		}
		buf[i].fd_req.devname = data[i].f_mntonname;
		buf[i].fd_req.path = data[i].f_mntfromname;
	}
	return n_entries;
}

	/* END OSF1 version */

#else

	/* BEGIN !OSF1 and !ULTRIX version - use setmntent() and getmntent()  */

FILE			*setmntent();
struct mntent	*getmntent();

getmnt( start, buf, bufsize, mode, path )
int				*start;
struct fs_data	buf[];
unsigned		bufsize;
int				mode;
char			*path;
{
	FILE			*tab;
	struct mntent	*ent;
	struct stat		st_buf;
	int				i;
	int				lim;

	if( (tab=setmntent("/etc/mtab","r")) == NULL ) {
		perror( "setmntent" );
		exit( 1 );
	}

	lim = bufsize / sizeof(struct fs_data);
	for( i=0; i < lim && ent=getmntent(tab); i++ ) {
		if( stat(ent->mnt_dir,&st_buf) < 0 ) {
			buf[i].fd_req.dev = 0;
		} else {
			buf[i].fd_req.dev = st_buf.st_dev;
		}
		buf[i].fd_req.devname = strdup(ent->mnt_fsname);
		buf[i].fd_req.path = strdup(ent->mnt_dir); 
	}
	return i;
}

	/* END !OSF1 and !ULTRIX version */

#endif
