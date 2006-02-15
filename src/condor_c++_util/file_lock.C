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

 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "file_lock.h"
#include <stdio.h>

extern "C" int lock_file( int fd, LOCK_TYPE type, BOOLEAN do_block );

FileLock::FileLock( int f, FILE *fp ) : fd(f), fp(fp)
{
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
// lock_file uses lseeks in order to lock the first 4 bytes of the file on NT
// It DOES properly reset the lseek version of the file position, but that is
// not the same (in some very inconsistent and weird ways) as the fseek one,
// so if the user has given us a FILE *, we need to make sure we don't ruin
// their current position.  The lesson here is don't use fseeks and lseeks
// interchangeably...

	long lPosBeforeLock;
	if (fp) // if the user has a FILE * as well as an fd
	{
		// save their FILE*-based current position
		lPosBeforeLock = ftell(fp); 
	}
	
	int		status = 0;
	status = lock_file( fd, t, blocking );
	
	if (fp)
	{
		// restore their FILE*-position
		fseek(fp, lPosBeforeLock, SEEK_SET); 	
	}

	if( status == 0 ) {
		state = t;
	}
	if ( status != 0 ) {
		dprintf( D_ALWAYS, "FileLock::obtain(%d) failed - errno %d (%s)\n",
	                t, errno, strerror(errno) );
	}
	return status == 0;
}

BOOLEAN
FileLock::release()
{
	return obtain( UN_LOCK );
}
