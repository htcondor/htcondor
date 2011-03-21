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
#include <sys/locking.h>
#include "file_lock.h"

int
lock_file_plain(int fd, LOCK_TYPE type, BOOLEAN do_block)
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

	lseek(fd, SEEK_SET, original_pos);

	return result;
}

int
lock_file(int fd, LOCK_TYPE type, BOOLEAN do_block)
{
	int result = lock_file_plain( fd, type, do_block );

	if ( result < 0 && ( errno != EACCES && errno != EDEADLOCK ) ) {
		EXCEPT("Programmer error in lock_file()");
	}

	return result;
}
