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

#ifdef  ultrix
#include <sys/mount.h>
#else ultrix
#include <mntent.h>
struct fs_data_req {
	dev_t	dev;
	char	*devname;
	char	*path;
};
struct fs_data {
	struct fs_data_req fd_req;
};
#define NOSTAT_MANY 0
#endif ultrix

char			*strdup(), *malloc();

#ifndef ultrix
/*
** The function getmnt() is Ultrix specific and returns a different structure
** than the similar BSD functions setmntent(3), getmntent(3), etc.  Here
** we simulate "getmnt()" by calling the BSD style library routines.
** N.B. we simulate getmnt() only to the extent needed specifically
** for this program.  This is NOT a generally correct simulation.
*/
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

	if( (tab=setmntent("/etc/mtab","r")) == NULL ) {
		perror( "setmntent" );
		exit( 1 );
	}

	for( i=0; ent=getmntent(tab); i++ ) {
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
#endif !ultrix
