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


/* 
   This file contains implementations of functions we need in the
   regular libcedar.a that we do NOT want to link into standard
   universe jobs with libcondorsyscall.a.  Any functions implemented
   here need to be added to condor_syscall_lib/cedar_no_ckpt_stubs.C
   that calls EXCEPT() or whatever is appropriate.  This way, we can
   add functionality to CEDAR that we need/use outside of the syscall
   library without causing trouble inside the standard universe code.
*/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "directory.h"


int
ReliSock::get_file_with_permissions( filesize_t *size, 
									 const char *destination,
									 bool flush_buffers )
{
	int result;
	condor_mode_t file_mode;

	// Read the permissions
	this->decode();
	if ( this->code( file_mode ) == FALSE ||
		 this->end_of_message() == FALSE ) {
		dprintf( D_ALWAYS, "ReliSock::get_file_with_permissions(): "
				 "Failed to read permissions from peer\n" );
		return -1;
	}

	result = get_file( size, destination, flush_buffers );

	if ( result < 0 ) {
		return result;
	}

		// If the other side told us to ignore its permissions, then we're
		// done.
	if ( file_mode == NULL_FILE_PERMISSIONS ) {
		dprintf( D_FULLDEBUG, "ReliSock::get_file_with_permissions(): "
				 "received null permissions from peer, not setting\n" );
		return result;
	}

		// We don't know how unix permissions translate to windows, so
		// ignore whatever permissions we received if we're on windows.
#ifndef WIN32
	dprintf( D_FULLDEBUG, "ReliSock::get_file_with_permissions(): "
			 "going to set permissions %o\n", file_mode );

	// chmod the file
	errno = 0;
	result = ::chmod( destination, (mode_t)file_mode );
	if ( result < 0 ) {
		dprintf( D_ALWAYS, "ReliSock::get_file_with_permissions(): "
				 "Failed to chmod file '%s': %s (errno: %d)\n",
				 destination, strerror(errno), errno );
		return -1;
	}
#endif

	return result;
}


int
ReliSock::put_file_with_permissions( filesize_t *size, const char *source )
{
	int result;
	condor_mode_t file_mode;

#ifndef WIN32
	// Stat the file
	StatInfo stat_info( source );

	if ( stat_info.Error() ) {
		dprintf( D_ALWAYS, "ReliSock::put_file_with_permissions(): "
				 "Failed to stat file '%s': %s (errno: %d, si_error: %d)\n",
				 source, strerror(stat_info.Errno()), stat_info.Errno(),
				 stat_info.Error() );
		return -1;
	}
	file_mode = (condor_mode_t)stat_info.GetMode();
#else
		// We don't know what unix permissions a windows file should have,
		// so tell the other side to ignore permissions from us (act like
		// get/put_file() ).
	file_mode = NULL_FILE_PERMISSIONS;
#endif

	dprintf( D_FULLDEBUG, "ReliSock::put_file_with_permissions(): "
			 "going to send permissions %o\n", file_mode );

	// Send the permissions
	this->encode();
	if ( this->code( file_mode ) == FALSE ||
		 this->end_of_message() == FALSE ) {
		dprintf( D_ALWAYS, "ReliSock:;put_file_with_permissions(): "
				 "Failed to send permissions\n" );
		return -1;
	}

	result = put_file( size, source );

	return result;
}

