/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_getmnt.h"


/*
 The function getmnt() is Ultrix specific and returns a different
 structure than the similar functions available on other UNIX
 variants.  We simulate "getmnt()" by calling the appropriate platform
 specific library routines.  N.B. we simulate getmnt() only to the
 extent needed specifically by condor.  This is NOT a generally correct
 simulation.
*/


#if defined(Darwin) || defined(CONDOR_FREEBSD)

#include <sys/stat.h>
#include <sys/mount.h>


int
getmnt( int * /*start*/, struct fs_data buf[], unsigned /*bufsize*/,
		int /*mode*/, char * /*path*/ )
{
	struct statfs	*data = NULL;
	struct stat	st_buf;
	int		n_entries;
	int		i;

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

#else

	/* BEGIN !ULTRIX, !AIX  version - use setmntent() and getmntent()  */

FILE			*setmntent();
struct mntent	*getmntent();

// I believe this is *only* used by std::universe
extern "C" int
getmnt( int* /*start*/, struct fs_data buf[],
		unsigned int bufsize, int /*mode*/, char* /*path*/ )
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
	for( i=0; (i < lim) && (ent=getmntent(tab)); i++ ) {
		if( stat(ent->mnt_dir,&st_buf) < 0 ) {
			buf[i].fd_req.dev = 0;
		} else {
			buf[i].fd_req.dev = st_buf.st_dev;
		}
		buf[i].fd_req.devname = strdup(ent->mnt_fsname);
		buf[i].fd_req.path = strdup(ent->mnt_dir);
	}

	endmntent(tab);
	return i;
}

	/* END !ULTRIX version */

#endif
