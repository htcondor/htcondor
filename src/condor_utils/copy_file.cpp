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


#include "condor_common.h"
#include "condor_debug.h"

extern "C"
{
int copy_file(const char *old_filename, const char *new_filename);
int hardlink_or_copy_file(const char *old_filename, const char *new_filename);

int
copy_file(const char *old_filename, const char *new_filename)
{
#if defined(WIN32)
   		// overwrite destination path
   BOOL retval;
   retval = CopyFile(old_filename, new_filename, FALSE);

   if (retval == 0) {
   		// failure
	  dprintf(D_ALWAYS, "CopyFile() failed with error=%li\n",
			  GetLastError());
	  return -1; 
   }
   else { return 0; } // success

#else
	int rc;
	int num_bytes;
	int in_fd = -1;
	int out_fd = -1;
	int new_file_created = 0;
	char buff[1024];
	struct stat fs;
	mode_t old_umask;

	old_umask = umask(0);

	rc = stat( old_filename, &fs );
	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "stat(%s) failed with errno %d\n",
				 old_filename, errno );
		goto copy_file_err;
	}
	fs.st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;

	in_fd = safe_open_wrapper( old_filename, O_RDONLY | O_LARGEFILE, 0644 );
	if ( in_fd < 0 ) {
		dprintf( D_ALWAYS, "safe_open_wrapper(%s, O_RDONLY|O_LARGEFILE) failed with errno %d\n",
				 old_filename, errno );
		goto copy_file_err;
	}

	out_fd = safe_open_wrapper( new_filename, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, fs.st_mode );
	if ( out_fd < 0 ) {
		dprintf( D_ALWAYS, "safe_open_wrapper(%s, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, %d) failed with errno %d\n",
				 new_filename, fs.st_mode, errno );
		goto copy_file_err;
	}

	new_file_created = 1;

	errno = 0;
	rc = read( in_fd, buff, sizeof(buff) );
	while ( rc > 0 ) {
		num_bytes = rc;
		rc = write( out_fd, buff, num_bytes );
		if ( rc < num_bytes ) {
			dprintf( D_ALWAYS, "write(%d) to file %s return %d, errno %d\n",
					 num_bytes, new_filename, rc, errno );
			goto copy_file_err;
		}
		rc = read( in_fd, buff, sizeof(buff) );
	}

	if ( rc < 0 ) {
		dprintf( D_ALWAYS, "read() from file %s failed with errno %d\n",
				 old_filename, errno );
		goto copy_file_err;
	}

	close (in_fd);
	close (out_fd);

	umask( old_umask );

	return 0;

 copy_file_err:
	if ( in_fd != -1 ) {
		close( in_fd );
	}
	if ( out_fd != -1 ) {
		close( out_fd );
	}
	if ( new_file_created ) {
		unlink( new_filename );
	}
	umask( old_umask );
	return -1;
#endif
}

int
hardlink_or_copy_file(const char *old_filename, const char *new_filename)
{
#if defined(WIN32)
	/* There are no hardlinks under Windows, so just copy the file. */
	return copy_file(old_filename,new_filename);
#else
	if(link(old_filename,new_filename) == -1) {
		if(errno == EEXIST) {
			//We definitely do not want to call copy_file() if the target
			//file already exists, because the target might be a hard link
			//to old_filename, and copy_file() will open and truncate it.

			if(remove(new_filename) == -1) {
				dprintf(D_ALWAYS,"Failed to remove %s (errno %d), so cannot create hard link from %s\n",new_filename,errno,old_filename);
				return -1;
			}
			if(link(old_filename,new_filename) == 0) {
				return 0;
			}
			if(errno == EEXIST) {
				dprintf(D_ALWAYS,"Removed %s, but hard linking from %s still fails with errno %d\n",new_filename,old_filename,errno);
				return -1;
			}
		}

		// Hardlink may fail for a number of reasons, some of which
		// can be solved by doing a plain old copy instead.  No harm
		// in trying.
		return copy_file(old_filename,new_filename);
	}
	return 0;
#endif
}
}