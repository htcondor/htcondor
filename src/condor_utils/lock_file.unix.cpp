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
#include "condor_random_num.h"

/* Declare some static variables here that are initialized by lock_file() and
 * used by lock_file_plain(). We do this song and dance because we want to 
 * initialize these variables by invoking various Condor library functions,
 * like our randomization functions, and invoking those functions from
 * lock_file_plain() is not permitted due to deadlock and/or threading reasons
 */
static unsigned int _lock_file_usleep_time = 0;
static unsigned int _lock_file_num_retries = 0;

/*
  Lock a file. This version is used for all unices. The windows version
  is in lock_file.WIN32.c. We use fcntl() here. lock_file_plain() doesn't
  call any other Condor code and is thus suitable for use by the dprintf
  code. lock_file() may call dprintf() and param(). C++ code should use
  the wrapper class FileLock instead of these functions.
*/

int
lock_file_plain( int fd, LOCK_TYPE type, bool do_block )
{
	struct flock	f;
	int				cmd;
	int rc, saved_errno;
	unsigned int num_retries = 0;

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

		/* Call fcntl */
	rc =  fcntl(fd,cmd,&f);
	saved_errno = errno;

		/* Deal with EINTR by retrying if in non-blocking mode */
	while ( !do_block && rc < 0  && saved_errno == EINTR ) 
	{
		rc =  fcntl(fd,cmd,&f);
		saved_errno = errno;
	}

		/* Deal w/ temporary errors by retrying if in blocking mode */
	while ( do_block && rc < 0 && num_retries < _lock_file_num_retries ) 
	{
		struct timeval timer;
		timer.tv_sec = 0;
		timer.tv_usec = _lock_file_usleep_time;
		switch (saved_errno) {
				// for these errors, just retry the system call 
				// immediately, don't increment number of retries.
			case EINTR:
				break;

				// for these errors, retry the system call a limited
				// number of times, and after waiting a 
				// fraction of a second.
			case ENOLCK:
			case EACCES:
			case EAGAIN:
				num_retries++;
				// do a platform independent usleep via select()
				select(0,NULL,NULL,NULL,&timer);
				break;

				// anything else is not an errno indicative
				// of a temporary condition, so break out of the 
				// while loop.
			default:
				num_retries = _lock_file_num_retries;
				continue;
		}
		
		rc =  fcntl(fd,cmd,&f);
		saved_errno = errno;
	}

	if ( rc < 0 ) {
		errno = saved_errno;
		return -1;
	} else {
		return 0;
	}
}

int
lock_file( int fd, LOCK_TYPE type, bool do_block )
{
	int rc;
	int saved_errno;
	static bool initialized = false;

	if ( !initialized ) {
		initialized = true;
		char *subsys = param("SUBSYSTEM");
		if ( subsys && strcmp(subsys,"SCHEDD")==0 ) {
			// If we are the schedd, retry early and often.
			// usleep time averages to 1/20 of a second, and
			// keep trying on average for 20 seconds.
			_lock_file_usleep_time = get_random_uint_insecure() % 100000;
			_lock_file_num_retries = 20 * 20;
		} else {
			// If we are not the schedd (eg we are the shadow), we
			// can be less agressive. usleep an average of a second, try
			// for 5 minutes.
			_lock_file_usleep_time = get_random_uint_insecure() % 2000000;
			_lock_file_num_retries = 60 * 5;
		}
		if (subsys) free(subsys);
	}

	rc = lock_file_plain( fd, type, do_block );
	saved_errno = errno;

	/* now, fcntl should work accross nfs.  but, due to bugs in some
	   implementations (*cough* linux *cough*) it sometimes fails with
	   errno 37 (ENOLCK).  if this happens we check the config to see
	   if we should report this as an error.   -zmiller  07/15/02
	   */
	if ( rc == -1 && saved_errno == ENOLCK ) {
		if (param_boolean_crufty("IGNORE_NFS_LOCK_ERRORS", false)) {
			dprintf ( D_FULLDEBUG, "Ignoring error ENOLCK on fd %i\n", fd );
			return 0;
		}
	}

	if ( rc == -1 )
	{
		dprintf( D_ALWAYS, "lock_file returning ERROR, errno=%d (%s)\n",
				saved_errno, strerror(saved_errno) );
#if 0  // in v7.6.0, we cannot risk EXCEPTing, but do so in v7.7.0
		if (saved_errno == EDEADLK || saved_errno == EFAULT) {
			EXCEPT("lock_file failed with errno %d, should never happen!",saved_errno);
		}
#endif
		errno = saved_errno;
	}

	return rc;
}
