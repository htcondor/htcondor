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
#include <sys/locking.h>
#include "file_lock.h"
#include "fake_flock.h"

int
lock_file(int fd, LOCK_TYPE type, int do_block)
{
	int	mode, result;
	long original_pos, pos;

	original_pos = lseek(fd, SEEK_CUR, 0);
	if (original_pos < 0) return original_pos;
	pos = lseek(fd, SEEK_SET, 0);
	if (pos < 0) return pos;

	switch (type) {
	case READ_LOCK:
	case WRITE_LOCK:
		if (do_block)
			mode = _LK_LOCK;
		else
			mode = _LK_NBLCK;
		break;
	case UN_LOCK:
	case LOCK_UN:
		mode = _LK_UNLCK;
		break;
	}

	result = _locking(fd, mode, 4L);

#if 0  // leave this out because lock_file called from dprintf,
	   // so we cannot dprintf or EXCEPT in lock_file !
	if ( result < 0 && ( errno != EACCES && errno != EDEADLOCK ) ) {
		EXCEPT("Programmer error in lock_file()");
	}
#endif

	lseek(fd, SEEK_SET, original_pos);

	return result;
}
