/* 
** Copyright 1997 by the Condor Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Basney
**
** Purpose: Advisory file locking on Windows NT
*/ 

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

	lseek(fd, SEEK_SET, original_pos);

	return result;
}