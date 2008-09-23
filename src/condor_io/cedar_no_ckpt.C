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
#include "globus_utils.h"
#include "condor_auth_x509.h"
#include "condor_config.h"


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

		// Now send an empty file in order to recover sanity on this
		// stream.
		file_mode = NULL_FILE_PERMISSIONS;
		this->encode();
		if( this->code( file_mode) == FALSE ||
			this->end_of_message() == FALSE ) {
			dprintf( D_ALWAYS, "ReliSock::put_file_with_permissions(): "
			         "Failed to send dummy permissions\n" );
			return -1;
		}
		int rc = put_empty_file( size );
		if(rc < 0) {
			return rc;
		}
		return PUT_FILE_OPEN_FAILED;
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
		dprintf( D_ALWAYS, "ReliSock::put_file_with_permissions(): "
				 "Failed to send permissions\n" );
		return -1;
	}

	result = put_file( size, source );

	return result;
}

int
ReliSock::get_x509_delegation( filesize_t *size, const char *destination,
							   bool flush_buffers )
{
	int in_encode_mode;

		// store if we are in encode or decode mode
	in_encode_mode = is_encode();

	if ( !prepare_for_nobuffering( stream_unknown ) ||
		 !end_of_message() ) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): failed to "
				 "flush buffers\n" );
		return -1;
	}

	if ( x509_receive_delegation( destination, relisock_gsi_get, (void *) this,
								  relisock_gsi_put, (void *) this ) != 0 ) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): "
				 "delegation failed: %s\n", x509_error_string() );
		return -1;
	}

		// restore stream mode (either encode or decode)
	if ( in_encode_mode && is_decode() ) {
		encode();
	} else if ( !in_encode_mode && is_encode() ) { 
		decode();
	}
	if ( !prepare_for_nobuffering( stream_unknown ) ) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): failed to "
				 "flush buffers afterwards\n" );
		return -1;
	}

	if ( flush_buffers ) {
		int rc = 0;
		int fd = safe_open_wrapper( destination, O_WRONLY, 0 );
		if ( fd < 0 ) {
			rc = fd;
		} else {
			rc = fsync( fd );
			::close( fd );
		}
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): open/fsync "
					 "failed, errno=%d (%s)\n", errno,
					 strerror( errno ) );
		}
	}

		// We should figure out how many bytes were sent
	*size = 0;

	return 0;
}

int
ReliSock::put_x509_delegation( filesize_t *size, const char *source )
{
	int in_encode_mode;

		// store if we are in encode or decode mode
	in_encode_mode = is_encode();

	if ( !prepare_for_nobuffering( stream_unknown ) ||
		 !end_of_message() ) {
		dprintf( D_ALWAYS, "ReliSock::put_x509_delegation(): failed to "
				 "flush buffers\n" );
		return -1;
	}

	if ( x509_send_delegation( source, relisock_gsi_get, (void *) this,
							   relisock_gsi_put, (void *) this ) != 0 ) {
		dprintf( D_ALWAYS, "ReliSock::put_x509_delegation(): delegation "
				 "failed: %s\n", x509_error_string() );
		return -1;
	}

		// restore stream mode (either encode or decode)
	if ( in_encode_mode && is_decode() ) {
		encode();
	} else if ( !in_encode_mode && is_encode() ) { 
		decode();
	}
	if ( !prepare_for_nobuffering( stream_unknown ) ) {
		dprintf( D_ALWAYS, "ReliSock::put_x509_delegation(): failed to "
				 "flush buffers afterwards\n" );
		return -1;
	}

		// We should figure out how many bytes were sent
	*size = 0;

	return 0;
}


int relisock_gsi_get(void *arg, void **bufp, size_t *sizep)
{
    /* globus code which calls this function expects 0/-1 return vals */
    
    ReliSock * sock = (ReliSock*) arg;
    size_t stat;
    
    sock->decode();
    
    //read size of data to read
    stat = sock->code( *((int *)sizep) );

	if( *((int *)sizep) == 0 ) {
			// We avoid calling malloc(0) here, because the zero-length
			// buffer is not being freed by globus.
		*bufp = NULL;
	}
	else {
		*bufp = malloc( *((int *)sizep) );
		if ( !*bufp ) {
			dprintf( D_ALWAYS, "malloc failure relisock_gsi_get\n" );
			stat = FALSE;
		}

			//if successfully read size and malloced, read data
		if ( stat ) {
			sock->code_bytes( *bufp, *((int *)sizep) );
		}
	}
    
    sock->end_of_message();
    
    //check to ensure comms were successful
    if ( stat == FALSE ) {
        dprintf( D_ALWAYS, "relisock_gsi_get (read from socket) failure\n" );
        return -1;
    }
    return 0;
}

int relisock_gsi_put(void *arg,  void *buf, size_t size)
{
    //param is just a ReliSock*
    ReliSock *sock = (ReliSock *) arg;
    int stat;
    
    sock->encode();
    
    //send size of data to send
    stat = sock->code( (int &)size );
    
    
    //if successful, send the data
    if ( stat ) {
        if ( !(stat = sock->code_bytes( buf, ((int) size )) ) ) {
            dprintf( D_ALWAYS, "failure sending data (%d bytes) over sock\n",size);
        }
    }
    else {
        dprintf( D_ALWAYS, "failure sending size (%d) over sock\n", size );
    }
    
    sock->end_of_message();
    
    //ensure data send was successful
    if ( stat == FALSE) {
        dprintf( D_ALWAYS, "relisock_gsi_put (write to socket) failure\n" );
        return -1;
    }
    return 0;
}

bool
Stream::prepare_crypto_for_secret_is_noop()
{
	CondorVersionInfo const *peer_ver = get_peer_version();
	if( !peer_ver || peer_ver->built_since_version(7,1,3) ) {
		if( param_boolean("ENCRYPT_SECRETS",true) ) {
			if( !get_encryption() ) {
				if( crypto_ ) {
						// do turn on encryption before sending secret
					return false;
				}
			}
		}
	}
	return true;
}

void
Stream::prepare_crypto_for_secret()
{
	m_crypto_state_before_secret = true;
	if( !prepare_crypto_for_secret_is_noop() ) {
		dprintf(D_NETWORK,"encrypting secret\n");
		m_crypto_state_before_secret = get_encryption(); // always false
		set_crypto_mode(true);
	}
}

void
Stream::restore_crypto_after_secret()
{
	if( !m_crypto_state_before_secret ) {
		set_crypto_mode(false); //restore crypto mode
	}
}
