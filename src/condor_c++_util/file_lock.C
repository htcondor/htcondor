/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
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
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "file_lock.h"

extern "C" int lock_file( int fd, LOCK_TYPE type, BOOLEAN do_block );

FileLock::FileLock( int f )
{
	fd = f;
	blocking = TRUE;
	state = UN_LOCK;
}

FileLock::~FileLock()
{
	if( state != UN_LOCK ) {
		release();
	}
}

void
FileLock::display()
{
	printf( "fd = %d\n", fd );
	printf( "blocking = %s\n", blocking ? "TRUE" : "FALSE" );
	switch( state ) {
	  case READ_LOCK:
		printf( "state = READ\n" );
		break;
	  case WRITE_LOCK:
		printf( "state = WRITE\n" );
		break;
	  case UN_LOCK:
		printf( "state = UNLOCKED\n" );
		break;
	}
}

BOOLEAN
FileLock::is_blocking()
{
	return blocking;
}

LOCK_TYPE
FileLock::get_state()
{
	return state;
}

void
FileLock::set_blocking( BOOLEAN val )
{
	blocking = val;
}

BOOLEAN
FileLock::obtain( LOCK_TYPE t )
{
	int		status = lock_file( fd, t, blocking );
	if( status == 0 ) {
		state = t;
	}
	return status == 0;
}

BOOLEAN
FileLock::release()
{
	return obtain( UN_LOCK );
}
