/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#include "condor_common.h"

#include "except.h"
#include "debug.h"

extern int	errno;

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




