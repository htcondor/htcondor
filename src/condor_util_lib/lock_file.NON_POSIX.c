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
#include "condor_config.h"
#include "condor_debug.h"
#include "file_lock.h"

/*
  Lock a file. This version is used for all unices. The windows version
  is in lock_file.WIN32.c. We use fcntl() here. lock_file_plain() doesn't
  call any other Condor code and is thus suitable for use by the dprintf
  code. lock_file() may call dprintf() and param(). C++ code should use
  the wrapper class FileLock instead of these functions.
*/

int
lock_file_plain( int fd, LOCK_TYPE type, int do_block )
{
	struct flock	f;
	int				cmd;

	if( do_block ) {
		cmd = F_SETLKW;		/* blocking */
	} else {
		cmd = F_SETLK;		/* non-blocking */
	}

		/* These parameters say we want to lock the whole file */
	f.l_whence = SEEK_SET;	/* l_start refers to beginning of file */
	f.l_start = 0;			/* start locking at byte 0 */
	f.l_len = 0;			/* continue locking to the end of the file */
	f.l_pid = 0;			/* don't care about this one */

	switch( type ) {
	  case READ_LOCK:
		f.l_type = F_RDLCK;
		break;
	  case WRITE_LOCK:
		f.l_type = F_WRLCK;
		break;
	  case UN_LOCK:
		f.l_type = F_UNLCK;
		break;
      default:
			  /* unknown lock type, set errno and fail immediately */
		errno = EINVAL;
		return -1;
	}

		/* be signal safe */
	while( fcntl(fd,cmd,&f) < 0 ) {
		switch (errno) {
			case EINTR:
				// this just means we were interrupted, not
				// necessarily that we failed.
				break;
			default:
				return -1;
		}
	}
	return 0;
}

int
lock_file( int fd, LOCK_TYPE type, int do_block )
{
	int rc;

	rc = lock_file_plain( fd, type, do_block );

	/* now, fcntl should work accross nfs.  but, due to bugs in some
	   implementations (*cough* linux *cough*) it sometimes fails with
	   errno 37 (ENOLCK).  if this happens we check the config to see
	   if we should report this as an error.   -zmiller  07/15/02
	   */
	if ( rc == -1 && errno == ENOLCK ) {
		char* p = param("IGNORE_NFS_LOCK_ERRORS");
		char  val = 'N';

		if (p) {
			val = p[0];
			free (p);
		}

		if (val == 'Y' || val == 'y' || val == 'T' || val == 't') {
				// pretend there was no error.
			dprintf ( D_FULLDEBUG, "Ignoring error ENOLCK on fd %i\n", fd );
			return 0;
		} else {
			errno = ENOLCK;
			return rc;
		}
	}

	return rc;
}
