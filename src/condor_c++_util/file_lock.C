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

 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
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
	
	int		status = lock_file( fd, t, blocking );
	
	if (fp)
	{
		// restore their FILE*-position
		fseek(fp, lPosBeforeLock, SEEK_SET); 	
	}

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
