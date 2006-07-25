/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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

	// _locking() in blocking mode will try 10 times to get
	// the lock on the file, after which it gives up and fails
	// with errno==EDEADLOCK. Since we want to block indefinitely
	// for this lock, we loop until we get the lock.
	do {
		result = _locking(fd, mode, 4L);
	} while ((result < 0) && errno == EDEADLOCK); 

#if 0  // leave this out because lock_file called from dprintf,
	   // so we cannot dprintf or EXCEPT in lock_file !
	if ( result < 0 && ( errno != EACCES && errno != EDEADLOCK ) ) {
		EXCEPT("Programmer error in lock_file()");
	}
#endif

	lseek(fd, SEEK_SET, original_pos);

	return result;
}
