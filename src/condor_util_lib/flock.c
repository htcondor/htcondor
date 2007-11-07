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

/*
** Compatibility routine for systems which utilize various forms of the
** fcntl() call for this purpose.  Note that semantics are a bit different
** then the bsd type flock() in that a write lock (exclusive lock) can
** only be applied if the file is open for writing, and a read lock
** (shared lock) can only be applied if the file is open for reading.
*/

/* Note, you may not call dprintf in here be because it will go into an 
	infinite loop as it tries to flock() the file multiple times through each
	dprintf invocation 
*/

int
flock( int fd, int op )
{
	struct flock	f;
	int				cmd;
	int				status;

	if( (op & LOCK_NB) == LOCK_NB ) {
		cmd = F_SETLK;		/* non-blocking */
	} else {
		cmd = F_SETLKW;		/* blocking */
	}

	f.l_start = 0;			/* flock only supports locking whole files */
	f.l_len = 0;
	f.l_whence = SEEK_SET;
	f.l_pid = 0;

	if( op & LOCK_SH ) {			/* shared */
		f.l_type = F_RDLCK;
	} else if (op & LOCK_EX) {		/* exclusive */
		f.l_type = F_WRLCK;
	} else if (op & LOCK_UN ) {		/* unlock */
		f.l_type = F_UNLCK;
	} else {
		errno = EINVAL;
		return -1;
	}

	status =  fcntl( fd, cmd, &f );
	return status;
}




