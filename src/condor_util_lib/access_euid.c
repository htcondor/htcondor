/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


#define _FILE_OFFSET_BITS 64
#include "condor_common.h"
#include "condor_debug.h"
#include <dirent.h>


static int access_euid_dir(char const *path,int mode,struct stat *statbuf)
{
	errno = 0;

	if ((R_OK == 0) || (mode & R_OK)) {
		DIR *d = opendir(path);
		if (d == NULL) {
			if( ! errno ) {
				dprintf( D_ALWAYS, "WARNING: opendir() failed, but "
						 "errno is still 0!  Beware of misleading "
						 "error messages\n" );
			}
			return -1;
		}
		closedir(d);
	}

	if ((W_OK == 0) || (mode & W_OK)) {
		int success = 0;
		int try;
		char *pathbuf = (char *)malloc(strlen(path) + 100);
		ASSERT( pathbuf );
		for(try=0;try<100;try++) {
			sprintf(pathbuf,"%s%caccess-test-%d-%d-%d",path,DIR_DELIM_CHAR,getpid(),(int)time(NULL),try);
			if( mkdir(pathbuf,0700) == 0 ) {
				rmdir( pathbuf );
				success = 1;
				break;
			}
			if( errno == EEXIST ) {
				continue;
			}
			break;
		}
		free( pathbuf );

		if( !success ) {
			if( errno == EEXIST ) {
				dprintf(D_ALWAYS, "Failed to test write access to %s, because too many access-test sub-directories exist.\n", path);
				return -1;
			}
			return -1;
		}
	}

	if ((X_OK == 0) || (mode & X_OK)) {
		struct stat st;
		if (!statbuf) {
			/* stats are expensive, so only do it if I have to */
			if (stat(path, &st) < 0) {
				if( ! errno ) {
					dprintf( D_ALWAYS, "WARNING: stat() failed, but "
							 "errno is still 0!  Beware of misleading "
							 "error messages\n" );
				}
				return -1;
			}
			statbuf = &st;
		}
		int bit = 0;
		if( statbuf->st_uid == geteuid() ) {
			bit |= S_IXUSR;
		}
		else if( statbuf->st_gid == getegid() ) {
			bit |= S_IXGRP;
		}
		else {
			bit |= S_IXOTH;
		}
		if (!(statbuf->st_mode & bit)) {
			errno = EACCES;
			return -1;
		}
	}

	return 0;
}

/* This function access a file with the access() interface, but with stat()
	semantics so that the access doesn't occur with the real uid, but the
	effective uid instead. */
int
access_euid(const char *path, int mode)
{
	FILE *f;
	struct stat buf;
	int already_stated = 0;

		/* clear errno before we begin, so we can ensure we set it to
		   the right thing.  if we return without touching it again,
		   we want it to be 0 (success).
		*/
	errno = 0;

	/* are the arguments valid? */
	if (path == NULL || (mode & ~(R_OK|W_OK|X_OK|F_OK)) != 0) {
		errno = EINVAL;
		return -1;
	}

	/* here we are going to sanity check the constants to make sure that we
		use them correctly if they happen to be a zero value(WHICH USUALLY F_OK
		IS). If it is zero, then by the semantics of the bitwise or of a zeroed
		value against a number, which is idempotent, it automatically needs to
		happen. */

	if ((F_OK == 0) || (mode & F_OK)) {
		if (stat(path, &buf) < 0) {
			if( ! errno ) {
					/* evil!  stat() failed but there's no errno! */
				dprintf( D_ALWAYS, "WARNING: stat() failed, but "
						 "errno is still 0!  Beware of misleading "
						 "error messages\n" );
			}
			return -1;
		}
		already_stated = 1;

		if( buf.st_mode & S_IFDIR ) {
			return access_euid_dir(path,mode,&buf);
		}
	}

	if ((R_OK == 0) || (mode & R_OK)) {
		f = safe_fopen_wrapper(path, "r", 0644);
		if (f == NULL) {
			if( errno == EISDIR ) {
				return access_euid_dir(path,mode,NULL);
			}
			if( ! errno ) {
				dprintf( D_ALWAYS, "WARNING: safe_fopen_wrapper() failed, but "
						 "errno is still 0!  Beware of misleading "
						 "error messages\n" );
			}
			return -1;
		}
		fclose(f);
	}

	if ((W_OK == 0) || (mode & W_OK)) {
		f = safe_fopen_wrapper(path, "a", 0644); /* don't truncate the file! */
		if (f == NULL) {
			if( errno == EISDIR ) {
				return access_euid_dir(path,mode,NULL);
			}
			if( ! errno ) {
				dprintf( D_ALWAYS, "WARNING: safe_fopen_wrapper() failed, but "
						 "errno is still 0!  Beware of misleading "
						 "error messages\n" );
			}
			return -1;
		}
		fclose(f);
	}

	if ((X_OK == 0) || (mode & X_OK)) {
		if (!already_stated) {
			/* stats are expensive, so only do it if I have to */
			if (stat(path, &buf) < 0) {
				if( ! errno ) {
					dprintf( D_ALWAYS, "WARNING: stat() failed, but "
							 "errno is still 0!  Beware of misleading "
							 "error messages\n" );
				}
				return -1;
			}
			if( buf.st_mode & S_IFDIR ) {
				return access_euid_dir(path,mode,&buf);
			}
		}
		if (!(buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
				/* since() stat worked, errno will still be 0.  that's
				   no good, since access_euid() is about to return
				   failure.  set errno here to the right thing. */
			errno = EACCES;
			return -1;
		}
	}

	return 0;
}


