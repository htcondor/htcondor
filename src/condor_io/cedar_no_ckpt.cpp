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
#include "condor_config.h"
#include "ccb_client.h"
#include "condor_sinful.h"
#include "shared_port_client.h"
#include "condor_netdb.h"
#include "internet.h"
#include "ipv6_hostname.h"
#include "condor_fsync.h"
#include "dc_transfer_queue.h"
#include "limit_directory_access.h"

#ifdef WIN32
#include <mswsock.h>	// For TransmitFile()
#endif

const unsigned int PUT_FILE_EOM_NUM = 666;

// This special file descriptor number must not be a valid fd number.
// It is used to make get_file() consume transferred data without writing it.
const int GET_FILE_NULL_FD = -10;

const size_t OLD_FILE_BUF_SZ = 65536;
const size_t AES_FILE_BUF_SZ = 262144;

int
ReliSock::get_file( filesize_t *size, const char *destination,
					bool flush_buffers, bool append, filesize_t max_bytes,
					DCTransferQueue *xfer_q)
{
	int fd;
	int result;
	int flags = O_WRONLY | _O_BINARY | _O_SEQUENTIAL | O_LARGEFILE;

	if ( append ) {
		flags |= O_APPEND;
	}
	else {
		flags |= O_CREAT | O_TRUNC;
	}

	if (allow_shadow_access(destination)) {
		// Open the file
		errno = 0;
		fd = ::safe_open_wrapper_follow(destination, flags, 0600);
	}
	else {
		fd = -1;
		errno = EACCES;
	}

	// Handle open failure; it's bad....
	if ( fd < 0 )
	{
		int saved_errno = errno;
#ifndef WIN32 /* Unix */
		if ( errno == EMFILE ) {
			_condor_fd_panic( __LINE__, __FILE__ ); /* This calls dprintf_exit! */
		}
#endif
		dprintf(D_ALWAYS,
				"get_file(): Failed to open file %s, errno = %d: %s.\n",
				destination, saved_errno, strerror(saved_errno) );

			// In order to remain in a well-defined state on the wire
			// protocol, read and throw away the file data.
			// We're not going to write the data, so don't try to append.
		result = get_file( size, GET_FILE_NULL_FD, flush_buffers, /*append*/ false, max_bytes, xfer_q );
		if( result<0 ) {
				// Failure to read (and throw away) data indicates that
				// we are in an undefined state on the wire protocol
				// now, so return that type of failure, rather than
				// the well-defined failure code for OPEN_FAILED.
			return result;
		}

		errno = saved_errno;
		return GET_FILE_OPEN_FAILED;
	} 

	dprintf( D_FULLDEBUG,
			 "get_file(): going to write to filename %s\n",
			 destination);

	result = get_file( size, fd,flush_buffers, append, max_bytes, xfer_q);

	if(::close(fd)!=0) {
		dprintf(D_ALWAYS,
				"ReliSock: get_file: close failed, errno = %d (%s)\n",
				errno, strerror(errno));
		result = -1;
	}

	if(result<0) {
		if (unlink(destination) < 0) {
			dprintf(D_FULLDEBUG, "get_file(): failed to unlink file %s errno = %d: %s.\n", 
			        destination, errno, strerror(errno));
		}
	}
	
	return result;
}

MSC_DISABLE_WARNING(6262) // function uses 64k of stack
int
ReliSock::get_file( filesize_t *size, int fd,
					bool flush_buffers, bool append, filesize_t max_bytes,
					DCTransferQueue *xfer_q)
{
	filesize_t filesize, bytes_to_receive;
	unsigned int eom_num;
	filesize_t total = 0;
	int retval = 0;
	int saved_errno = 0;
	bool buffered = get_encryption() && get_crypto_state()->m_keyInfo.getProtocol() == CONDOR_AESGCM;
	size_t buf_sz = OLD_FILE_BUF_SZ;

		// NOTE: the caller may pass fd=GET_FILE_NULL_FD, in which
		// case we just read but do not write the data.

	// Read the filesize from the other end of the wire
	// If we're operating in buffered mode, also read the buffer size
	// (each buffer-sized chunk will be sent in a seperate CEDAR message).
	if ( !get(filesize) || !(buffered ? get(buf_sz) : 1) || !end_of_message() ) {
		dprintf(D_ALWAYS, 
				"Failed to receive filesize in ReliSock::get_file\n");
		return -1;
	}
	bytes_to_receive = filesize;
	if ( append ) {
		lseek( fd, 0, SEEK_END );
	}

	std::unique_ptr<char[]> buf(new char[buf_sz]);

	// Log what's going on
	dprintf( D_FULLDEBUG,
			 "get_file: Receiving " FILESIZE_T_FORMAT " bytes\n",
			 bytes_to_receive );

		/*
		  the code used to check for filesize == -1 here, but that's
		  totally wrong.  we're storing the size as an unsigned int,
		  so we can't check it against a negative value.  furthermore,
		  ReliSock::put_file() never puts a -1 on the wire for the
		  size.  that's legacy code from the pseudo_put_file_stream()
		  RSC in the syscall library.  this code isn't like that.
		*/

	// Now, read it all in & save it
	while( total < bytes_to_receive ) {
		struct timeval t1,t2;
		if( xfer_q ) {
			condor_gettimestamp(t1);
		}

		int	iosize =
			(int) MIN( (filesize_t) buf_sz, bytes_to_receive - total );
		int	nbytes;
		if( buffered ) {
			nbytes = get_bytes( buf.get(), iosize );
			if( nbytes > 0 && !end_of_message() ) {
				nbytes = 0;
			}
		} else {
			nbytes = get_bytes_nobuffer( buf.get(), iosize, 0 );
		}

		if( xfer_q ) {
			condor_gettimestamp(t2);
			xfer_q->AddUsecNetRead(timersub_usec(t2, t1));
		}

		if ( nbytes <= 0 ) {
			break;
		}

		if( fd == GET_FILE_NULL_FD ) {
				// Do not write the data, because we are just
				// fast-forwarding and throwing it away, due to errors
				// already encountered.
			total += nbytes;
			continue;
		}

		int rval;
		int written;
		for( written=0; written<nbytes; ) {
			rval = ::write( fd, &buf[written], (nbytes-written) );
			if( rval < 0 ) {
				saved_errno = errno;
				dprintf( D_ALWAYS,
						 "ReliSock::get_file: write() returned %d: %s "
						 "(errno=%d)\n", rval, strerror(errno), errno );


					// Continue reading data, but throw it all away.
					// In this way, we keep the wire protocol in a
					// well defined state.
				fd = GET_FILE_NULL_FD;
				retval = GET_FILE_WRITE_FAILED;
				written = nbytes;
				break;
			} else if( rval == 0 ) {
					/*
					  write() shouldn't really return 0 at all.
					  apparently it can do so if we asked it to write
					  0 bytes (which we're not going to do) or if the
					  file is closed (which we're also not going to
					  do).  so, for now, if we see it, we want to just
					  break out of this loop.  in the future, we might
					  do more fancy stuff to handle this case, but
					  we're probably never going to see this anyway.
					*/
				dprintf( D_ALWAYS,
						 "ReliSock::get_file: write() returned 0: "
						 "wrote %d out of %d bytes (errno=%d %s)\n",
						 written, nbytes, errno, strerror(errno) );
				break;
			} else {
				written += rval;
			}
		}
		if( xfer_q ) {
			condor_gettimestamp(t1);
				// reuse t2 above as start time for file write
			xfer_q->AddUsecFileWrite(timersub_usec(t1, t2));
			xfer_q->AddBytesReceived(written);
			xfer_q->ConsiderSendingReport(t1.tv_sec);
		}

		total += written;
		if( max_bytes >= 0 && total > max_bytes ) {

				// Since breaking off here leaves the protocol in an
				// undefined state, further communication about what
				// went wrong is not possible.  We do not want to
				// consume all remaining bytes to keep our peer happy,
				// because max_bytes may have been specified to limit
				// network usage.  Therefore, it is best to avoid ever
				// getting here.  For example, in FileTransfer, the
				// sender is expected to be nice and never send more
				// than the receiver's limit; we only get here if the
				// sender is behaving unexpectedly.

			dprintf( D_ALWAYS, "get_file: aborting after downloading %ld of %ld bytes, because max transfer size is exceeded.\n",
					 (long int)total,
					 (long int)bytes_to_receive);
			return GET_FILE_MAX_BYTES_EXCEEDED;
		}
	}

	// Our caller may treat get_file() as the end of a CEDAR message
	// and call end_of_message immediately afterwards. This call will
	// keep that from failing.
	if (buffered && !prepare_for_nobuffering(stream_decode)) {
		dprintf( D_ALWAYS, "get_file: prepare_for_nobuffering() failed!\n" );
		return -1;
	}

	if ( filesize == 0 ) {
		if ( !get(eom_num) || eom_num != PUT_FILE_EOM_NUM ) {
			dprintf( D_ALWAYS, "get_file: Zero-length file check failed!\n" );
			return -1;
		}			
	}

	if (flush_buffers && fd != GET_FILE_NULL_FD ) {
		if (condor_fdatasync(fd) < 0) {
			dprintf(D_ALWAYS, "get_file(): ERROR on fsync: %d\n", errno);
			return -1;
		}
	}

	if( fd == GET_FILE_NULL_FD ) {
		dprintf( D_ALWAYS,
				 "get_file(): consumed " FILESIZE_T_FORMAT
				 " bytes of file transmission\n",
				 total );
	}
	else {
		dprintf( D_FULLDEBUG,
				 "get_file: wrote " FILESIZE_T_FORMAT " bytes to file\n",
				 total );
	}

	if ( total < filesize ) {
		dprintf( D_ALWAYS,
				 "get_file(): ERROR: received " FILESIZE_T_FORMAT " bytes, "
				 "expected " FILESIZE_T_FORMAT "!\n",
				 total, filesize);
		return -1;
	}

	*size = total;
	errno = saved_errno;
	return retval;
}
MSC_RESTORE_WARNING(6262) // function uses 64k of stack

int
ReliSock::put_empty_file( filesize_t *size )
{
	bool buffered = get_encryption() && get_crypto_state()->m_keyInfo.getProtocol() == CONDOR_AESGCM;
	*size = 0;
	// the put(1) here is required because the other size is expecting us
	// to send the size of messages we are going to use.  however, we're
	// send zero bytes total so we just need to send any int at all, which
	// then gets ignored on the other side.
	if(!put(*size) || !(buffered ? put(1) : 1) || !end_of_message()) {
		dprintf(D_ALWAYS,"ReliSock: put_file: failed to send dummy file size\n");
		return -1;
	}
	put(PUT_FILE_EOM_NUM); //end the zero-length file
	return 0;
}

int
ReliSock::put_file( filesize_t *size, const char *source, filesize_t offset, filesize_t max_bytes, DCTransferQueue *xfer_q)
{
	int fd;
	int result;

	// Open the file, handle failure
	if (allow_shadow_access(source)) {
		errno = 0;
		fd = safe_open_wrapper_follow(source, O_RDONLY | O_LARGEFILE | _O_BINARY | _O_SEQUENTIAL, 0);
	}
	else {
		fd = -1;
		errno = EACCES;
	}

	if ( fd < 0 )
	{
		dprintf(D_ALWAYS,
				"ReliSock: put_file: Failed to open file %s, errno = %d.\n",
				source, errno);
			// Give the receiver an empty file so that this message is
			// complete.  The receiver _must_ detect failure through
			// some additional communication that is not part of
			// the put_file() message.

		int rc = put_empty_file( size );
		if(rc < 0) {
			return rc;
		}

		return PUT_FILE_OPEN_FAILED;
	}

	dprintf( D_FULLDEBUG,
			 "put_file: going to send from filename %s\n", source);

	result = put_file( size, fd, offset, max_bytes, xfer_q );

	if (::close(fd) < 0) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: close failed, errno = %d (%s)\n",
				errno, strerror(errno));
		return -1;
	}

	return result;
}

MSC_DISABLE_WARNING(6262) // function uses 64k of stack
int
ReliSock::put_file( filesize_t *size, int fd, filesize_t offset, filesize_t max_bytes, DCTransferQueue *xfer_q )
{
	filesize_t	filesize;
	filesize_t	total = 0;
	bool buffered = get_encryption() && get_crypto_state()->m_keyInfo.getProtocol() == CONDOR_AESGCM;
	const size_t buf_sz = buffered ? AES_FILE_BUF_SZ : OLD_FILE_BUF_SZ;

	StatInfo filestat( fd );
	if ( filestat.Error() ) {
		int		staterr = filestat.Errno( );
		dprintf(D_ALWAYS, "ReliSock: put_file: StatBuf failed: %d %s\n",
				staterr, strerror( staterr ) );
		return -1;
	}

	if ( filestat.IsDirectory() ) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: Failed because directories are not supported.\n" );
			// Give the receiver an empty file so that this message is
			// complete.  The receiver _must_ detect failure through
			// some additional communication that is not part of
			// the put_file() message.

		int rc = put_empty_file( size );
		if(rc < 0) {
			return rc;
		}

#ifdef EISDIR
		errno = EISDIR;
#else
		errno = EINVAL;
#endif
		return PUT_FILE_OPEN_FAILED;
	}

	filesize = filestat.GetFileSize( );
	dprintf( D_FULLDEBUG,
			 "put_file: Found file size " FILESIZE_T_FORMAT "\n",
			 filesize );
	
	if ( offset > filesize ) {
		dprintf( D_ALWAYS,
				 "ReliSock::put_file: offset " FILESIZE_T_FORMAT
				 " is larger than file " FILESIZE_T_FORMAT "!\n",
				 offset, filesize );
	}
	filesize_t	bytes_to_send = filesize - offset;
	bool max_bytes_exceeded = false;
	if( max_bytes >= 0 && bytes_to_send > max_bytes ) {
		bytes_to_send = max_bytes;
		max_bytes_exceeded = true;
	}

	// Send the file size to the receiver
	// If we're operating in buffered mode, also send the buffer size
	// (each buffer-sized chunk will be sent in a seperate CEDAR message).
	if ( !put(bytes_to_send) || !(buffered ? put(buf_sz) : 1) || !end_of_message() ) {
		dprintf(D_ALWAYS, "ReliSock: put_file: Failed to send filesize.\n");
		return -1;
	}

	if ( offset ) {
		lseek( fd, offset, SEEK_SET );
	}

	// Log what's going on
	dprintf(D_FULLDEBUG,
			"put_file: sending " FILESIZE_T_FORMAT " bytes\n", bytes_to_send );

	// If the file has a non-zero size, send it
	if ( bytes_to_send > 0 ) {

#if defined(WIN32)
		// On Win32, if we don't need encryption, use the super-efficient Win32
		// TransmitFile system call. Also, TransmitFile does not support
		// file sizes over 2GB, so we avoid that case as well.
		if (  (!get_encryption()) &&
			  (0 == offset) &&
			  (bytes_to_send < INT_MAX)  ) {

			// First drain outgoing buffers
			if ( !prepare_for_nobuffering(stream_encode) ) {
				dprintf(D_ALWAYS,
						"ReliSock: put_file: failed to drain buffers!\n");
				return -1;
			}

			struct timeval t1;
			condor_gettimestamp(t1);

			// Now transmit file using special optimized Winsock call
			bool transmit_success = TransmitFile(_sock,(HANDLE)_get_osfhandle(fd),bytes_to_send,0,NULL,NULL,0) != FALSE;

			if( xfer_q ) {
				struct timeval t2;
				condor_gettimestamp(t2);
					// We don't know how much of the time was spent reading
					// from disk vs. writing to the network, so we just report
					// it all as network i/o time.
				xfer_q->AddUsecNetWrite(timersub_usec(t2, t1));
			}

			if( !transmit_success ) {
				dprintf(D_ALWAYS,
						"ReliSock: put_file: TransmitFile() failed, errno=%d\n",
						GetLastError() );
				return -1;
			} else {
				// Note that it's been sent, so that we don't try to below
				total = bytes_to_send;
				if( xfer_q ) {
					xfer_q->AddBytesSent(bytes_to_send);
					xfer_q->ConsiderSendingReport();
				}
			}
		}
#endif

		std::unique_ptr<char[]> buf(new char[buf_sz]);
		int nbytes, nrd;

		// On Unix, always send the file using put_bytes_nobuffer().
		// Note that on Win32, we use this method as well if encryption 
		// is required.
		while (total < bytes_to_send) {
			struct timeval t1;
			struct timeval t2;
			if( xfer_q ) {
				condor_gettimestamp(t1);
			}

			// Be very careful about where the cast to size_t happens; see gt#4150
			nrd = ::read(fd, buf.get(), (size_t)((bytes_to_send-total) < (int)buf_sz ? bytes_to_send-total : buf_sz));

			if( xfer_q ) {
				condor_gettimestamp(t2);
				xfer_q->AddUsecFileRead(timersub_usec(t2, t1));
			}

			if( nrd <= 0) {
				break;
			}
			if( buffered ) {
				nbytes = put_bytes(buf.get(), nrd);
				if( nbytes > 0 && !end_of_message() ) {
					nbytes = 0;
				}
			} else {
				nbytes = put_bytes_nobuffer(buf.get(), nrd, 0);
			}
			if (nbytes < nrd) {
					// put_bytes_nobuffer() does the appropriate
					// looping for us already, the only way this could
					// return less than we asked for is if it returned
					// -1 on failure.
				ASSERT( nbytes <= 0 );
				dprintf( D_ALWAYS, "ReliSock::put_file: failed to put %d "
						 "bytes (put_bytes_nobuffer() returned %d)\n",
						 nrd, nbytes );
				return -1;
			}
			if( xfer_q ) {
					// reuse t2 from above to mark the start of the
					// network op and t1 to mark the end
				condor_gettimestamp(t1);
				xfer_q->AddUsecNetWrite(timersub_usec(t1, t2));
				xfer_q->AddBytesSent(nbytes);
				xfer_q->ConsiderSendingReport(t1.tv_sec);
			}
			total += nbytes;
		}
	
	} // end of if filesize > 0

	// Our caller may treat put_file() as the end of a CEDAR message
	// and call end_of_message immediately afterwards. This call will
	// keep that from failing.
	if (buffered && !prepare_for_nobuffering(stream_encode)) {
		dprintf( D_ALWAYS, "put_file: prepare_for_nobuffering() failed!\n" );
		return -1;
	}

	if ( bytes_to_send == 0 ) {
		put(PUT_FILE_EOM_NUM);
	}

	dprintf(D_FULLDEBUG,
			"ReliSock: put_file: sent " FILESIZE_T_FORMAT " bytes\n", total);

	if (total < bytes_to_send) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: only sent " FILESIZE_T_FORMAT 
				" bytes out of " FILESIZE_T_FORMAT "\n",
				total, filesize);
		return -1;
	}

	if ( max_bytes_exceeded ) {
		dprintf(D_ALWAYS,
				"ReliSock: put_file: only sent " FILESIZE_T_FORMAT 
				" bytes out of " FILESIZE_T_FORMAT
				" because maximum upload bytes was exceeded.\n",
				total, filesize);
		*size = bytes_to_send;
			// Nothing we have done has told the receiver that we hit
			// the max_bytes limit.  The receiver _must_ detect
			// failure through some additional communication that is
			// not part of the put_file() message.
		return PUT_FILE_MAX_BYTES_EXCEEDED;
	}

	*size = filesize;
	return 0;
}
MSC_RESTORE_WARNING(6262) // function uses 64k of stack

int
ReliSock::get_file_with_permissions( filesize_t *size, 
									 const char *destination,
									 bool flush_buffers,
									 filesize_t max_bytes,
									 DCTransferQueue *xfer_q)
{
	int result;
	condor_mode_t file_mode = __mode_t_dummy_value;

	// Read the permissions
	this->decode();
	if ( this->code( file_mode ) == FALSE ||
		 this->end_of_message() == FALSE ) {
		dprintf( D_ALWAYS, "ReliSock::get_file_with_permissions(): "
				 "Failed to read permissions from peer\n" );
		return -1;
	}

	if (file_mode == MISSING_FILE_PERMISSIONS) {
		// pull file metadata down to keep stream n'sync
		return get_file( size, GET_FILE_NULL_FD, flush_buffers, /*append*/ false, max_bytes, xfer_q );
	}
	result = get_file( size, destination, flush_buffers, false, max_bytes, xfer_q );

	if ( result < 0 ) {
		return result;
	}

	if( destination && !strcmp(destination,NULL_FILE) ) {
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
ReliSock::put_file_with_permissions( filesize_t *size, const char *source, filesize_t max_bytes, DCTransferQueue *xfer_q )
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
		file_mode = MISSING_FILE_PERMISSIONS;
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

	result = put_file( size, source, 0, max_bytes, xfer_q );

	return result;
}

ReliSock::x509_delegation_result
ReliSock::get_x509_delegation( const char *destination,
                               bool flush_buffers, void **state_ptr )
{
	void *st;
	int in_encode_mode;

		// store if we are in encode or decode mode
	in_encode_mode = is_encode();

	if ( !prepare_for_nobuffering( stream_unknown ) ||
		 !end_of_message() ) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): failed to "
				 "flush buffers\n" );
		return delegation_error;
	}

	int rc =  x509_receive_delegation( destination, relisock_gsi_get, (void *) this,
	                                                relisock_gsi_put, (void *) this,
	                                                &st );
	if (rc == -1) {
		dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): "
				 "delegation failed: %s\n", x509_error_string() );
		return delegation_error;
	}
		// NOTE: if we provide a state pointer, x509_receive_delegation *must* either fail or continue.
	else if (rc == 0) {
		dprintf(D_ALWAYS, "Programmer error: x509_receive_delegation completed unexpectedy.\n");
		return delegation_error;
	}

		// restore stream mode (either encode or decode)
	if ( in_encode_mode && is_decode() ) {
		encode();
	} else if ( !in_encode_mode && is_encode() ) { 
		decode();
	}

	if (state_ptr) {
		*state_ptr = st;
		return delegation_continue;
	}

	return get_x509_delegation_finish( destination, flush_buffers, st );
}

ReliSock::x509_delegation_result
ReliSock::get_x509_delegation_finish( const char *destination, bool flush_buffers, void *state_ptr )
{
                // store if we are in encode or decode mode
        int in_encode_mode = is_encode();

        if ( x509_receive_delegation_finish( relisock_gsi_get, (void *) this, state_ptr ) != 0 ) {
                dprintf( D_ALWAYS, "ReliSock::get_x509_delegation_finish(): "
                                 "delegation failed to complete: %s\n", x509_error_string() );
                return delegation_error;
        }

	if ( flush_buffers ) {
		int rc = 0;
		int fd = safe_open_wrapper_follow( destination, O_WRONLY, 0 );
		if ( fd < 0 ) {
			rc = fd;
		} else {
			rc = condor_fdatasync( fd, destination );
			::close( fd );
		}
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "ReliSock::get_x509_delegation(): open/fsync "
					 "failed, errno=%d (%s)\n", errno,
					 strerror( errno ) );
		}
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
		return delegation_error;
	}

	return delegation_ok;
}

int
ReliSock::put_x509_delegation( filesize_t *size, const char *source, time_t expiration_time, time_t *result_expiration_time )
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

	if ( x509_send_delegation( source, expiration_time, result_expiration_time, relisock_gsi_get, (void *) this,
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

// These variables hold the size of the last data block handled by each
// respective function. They are part of a hacky workaround for a GSI bug.
size_t relisock_gsi_get_last_size = 0;
size_t relisock_gsi_put_last_size = 0;

int relisock_gsi_get(void *arg, void **bufp, size_t *sizep)
{
    /* globus code which calls this function expects 0/-1 return vals */
    
    ReliSock * sock = (ReliSock*) arg;
    size_t stat;
    
    sock->decode();
    
    //read size of data to read
    stat = sock->code( *sizep );
	if ( stat == FALSE ) {
		*sizep = 0;
	}

	if( *sizep == 0 ) {
			// We avoid calling malloc(0) here, because the zero-length
			// buffer is not being freed by globus.
		*bufp = NULL;
	}
	else {
		*bufp = malloc( *sizep );
		if ( !*bufp ) {
			dprintf( D_ALWAYS, "malloc failure relisock_gsi_get\n" );
			stat = FALSE;
		}

			//if successfully read size and malloced, read data
		if ( stat ) {
			stat = sock->code_bytes( *bufp, *sizep );
		}
	}
    
    sock->end_of_message();
    
    //check to ensure comms were successful
    if ( stat == FALSE ) {
        dprintf( D_ALWAYS, "relisock_gsi_get (read from socket) failure\n" );
        *sizep = 0;
        free( *bufp );
        *bufp = NULL;
        relisock_gsi_get_last_size = 0;
        return -1;
    }
    relisock_gsi_get_last_size = *sizep;
    return 0;
}

int relisock_gsi_put(void *arg,  void *buf, size_t size)
{
    //param is just a ReliSock*
    ReliSock *sock = (ReliSock *) arg;
    int stat;
    
    sock->encode();
    
    //send size of data to send
    stat = sock->put( size );
    
    
    //if successful, send the data
    if ( stat ) {
        // don't call code_bytes() on a zero-length buffer
        if ( size != 0 && !(stat = sock->code_bytes( buf, ((int) size )) ) ) {
            dprintf( D_ALWAYS, "failure sending data (%lu bytes) over sock\n",(unsigned long)size);
        }
    }
    else {
        dprintf( D_ALWAYS, "failure sending size (%lu) over sock\n", (unsigned long)size );
    }
    
    sock->end_of_message();
    
    //ensure data send was successful
    if ( stat == FALSE) {
        dprintf( D_ALWAYS, "relisock_gsi_put (write to socket) failure\n" );
        relisock_gsi_put_last_size = 0;
        return -1;
    }
    relisock_gsi_put_last_size = size;
    return 0;
}


int Sock::special_connect(char const *host,int /*port*/,bool nonblocking,CondorError * errorStack)
{
	if( !host || *host != '<' ) {
		return CEDAR_ENOCCB;
	}

	Sinful sinful(host);
	if( !sinful.valid() ) {
		return CEDAR_ENOCCB;
	}

	char const *shared_port_id = sinful.getSharedPortID();
	if( shared_port_id ) {
			// If the port of the SharedPortServer is 0, that means
			// we do not know the address of the SharedPortServer.
			// This happens, for example when Create_Process passes
			// the parent's address to a child or the child's address
			// to the parent and the SharedPortServer does not exist yet.
			// So do a local connection bypassing SharedPortServer,
			// if we are on the same machine.

			// Another case where we want to bypass connecting to
			// SharedPortServer is if we are the shared port server,
			// because this causes us to hang.

			// We could additionally bypass using the shared port
			// server if we use the same shared port server as the
			// target daemon and we are on the same machine.  However,
			// this causes us to use an additional ephemeral port than
			// if we connect to the shared port, so in case that is
			// important, use the shared port server instead.

		bool no_shared_port_server =
			sinful.getPort() && strcmp(sinful.getPort(),"0")==0;

		bool same_host = false;
		// TODO: Picking IPv4 arbitrarily.
		//   We should do a better job of detecting whether sinful
		//   points to a local interface.
		std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();
		if( sinful.getHost() && strcmp(my_ip.c_str(),sinful.getHost())==0 ) {
			same_host = true;
		}

		bool i_am_shared_port_server = false;
		if( daemonCore ) {
			char const *daemon_addr = daemonCore->publicNetworkIpAddr();
			if( daemon_addr ) {
				Sinful my_sinful(daemon_addr);
				if( my_sinful.getHost() && sinful.getHost() &&
					strcmp(my_sinful.getHost(),sinful.getHost())==0 &&
					my_sinful.getPort() && sinful.getPort() &&
					strcmp(my_sinful.getPort(),sinful.getPort())==0 &&
					(!my_sinful.getSharedPortID() ||
					 strcmp(my_sinful.getSharedPortID(),shared_port_id)==0) )
				{
					i_am_shared_port_server = true;
					dprintf(D_FULLDEBUG,"Bypassing connection to shared port server %s, because that is me.\n",daemon_addr);
				}
			}
		}
		if( (no_shared_port_server && same_host) || i_am_shared_port_server ) {

			if( no_shared_port_server && same_host ) {
				dprintf(D_FULLDEBUG,"Bypassing connection to shared port server, because its address is not yet established; passing socket directly to %s.\n",host);
			}

			// do_shared_port_local_connect() calls connect_socketpair(), which
			// normally uses loopback addresses.  However, the loopback address
			// may not be in the ALLOW list.  Instead, we need to use the
			// address we would use to contact the shared port daemon.
			const char * sharedPortIP = sinful.getHost();
			// Presently, for either same_host or i_am_shared_port_server to
			// be true, this must be as well.
			ASSERT( sharedPortIP );

			return do_shared_port_local_connect( shared_port_id, nonblocking, sharedPortIP );
		}
	}

		// Set shared port id even if it is null so we clear whatever may
		// already be there.  If it is not null, then this information
		// is saved here and used later after we have connected.
	setTargetSharedPortID( shared_port_id );

	char const *ccb_contact = sinful.getCCBContact();
	if( !ccb_contact || !*ccb_contact ) {
		return CEDAR_ENOCCB;
	}

	return do_reverse_connect(ccb_contact,nonblocking, errorStack);
}

int
SafeSock::do_reverse_connect(char const *,bool,CondorError *)
{
	dprintf(D_ALWAYS,
			"CCBClient: WARNING: UDP not supported by CCB."
			"  Will therefore try to send packet directly to %s.\n",
			peer_description());

	return CEDAR_ENOCCB;
}

int
ReliSock::do_reverse_connect(char const *ccb_contact,bool nonblocking,CondorError * errorStack)
{
	ASSERT( !m_ccb_client.get() ); // only one reverse connect at a time!

	//
	// Since we can't change the CCB server without also changing the CCB
	// client (that is, without breaking backwards compatibility), we have
	// to determine if the server sent us ... a string we can't use.  Joy.
	//

	m_ccb_client =
		new CCBClient( ccb_contact, (ReliSock *)this );

	if( !m_ccb_client->ReverseConnect(errorStack, nonblocking) ) {
		dprintf(D_ALWAYS,"Failed to reverse connect to %s via CCB.\n",
				peer_description());
		return 0;
	}
	if( nonblocking ) {
		return CEDAR_EWOULDBLOCK;
	}

	m_ccb_client = NULL; // in blocking case, we are done with ccb client
	return 1;
}

int
SafeSock::do_shared_port_local_connect( char const *, bool, char const * )
{
	dprintf(D_ALWAYS,
			"SharedPortClient: WARNING: UDP not supported."
			"  Failing to connect to %s.\n",
			peer_description());

	return 0;
}

int
ReliSock::do_shared_port_local_connect( char const *shared_port_id, bool nonblocking, char const *sharedPortIP )
{
		// Without going through SharedPortServer, we want to connect
		// to a daemon that is local to this machine and which is set up
		// to use the local SharedPortServer.  We do this by creating
		// a connected socket pair and then passing one of those sockets
		// to the target daemon over its named socket (or whatever mechanism
		// this OS supports).

	SharedPortClient shared_port_client;
	ReliSock sock_to_pass;
	std::string orig_connect_addr = get_connect_addr() ? get_connect_addr() : "";
	if( !connect_socketpair(sock_to_pass, sharedPortIP) ) {
		dprintf(D_ALWAYS,
				"Failed to connect to loopback socket, so failing to connect via local shared port access to %s.\n",
				peer_description());
		return 0;
	}

#if defined(DARWIN)
	//
	// See GT#7866.  Summary: removing the blocking acknowledgement of the
	// socket hand-off (#7502), if the master sleeps for 100ms instead of
	// re-entering the event loop (check-in [60471]), then -- only on
	// MacOS X and only for the shared port daemon -- the socket on which
	// the childalive message was sent will arrive at the master with no
	// data to read.  If other daemons are forced to use this function,
	// and attempt to contact the master while it's sleeping, they also fail.
	//
	// We have not been able to further analyze this problem, but dup()ing
	// the (newly-created) socket here works around the problem.
	//
	static int liveness_hack = -1;
	if( liveness_hack != -1 ) { ::close(liveness_hack); }
	liveness_hack = dup( sock_to_pass.get_file_desc() );
#endif

		// restore the original connect address, which got overwritten
		// in connect_socketpair()
	set_connect_addr(orig_connect_addr.c_str());

	char const *request_by = "";
	// A nonblocking call here causes a segfault, so don't do that.
	if( !shared_port_client.PassSocket(&sock_to_pass,shared_port_id,request_by, false) ) {
		return 0;
	}

	if( nonblocking ) {
			// We must pretend that we are not yet connected so that callers
			// who want a non-blocking connect will get the expected behavior
			// from Register_Socket() (register for write rather than read).
		_state = sock_connect_pending;
		return CEDAR_EWOULDBLOCK;
	}

	enter_connected_state();
	return 1;
}

void
ReliSock::cancel_reverse_connect() {
	ASSERT( m_ccb_client.get() );
	m_ccb_client->CancelReverseConnect();
}

bool
ReliSock::sendTargetSharedPortID()
{
	char const *shared_port_id = getTargetSharedPortID();
	if( !shared_port_id ) {
		return true;
	}
	SharedPortClient shared_port;
	return shared_port.sendSharedPortID(shared_port_id,this);
}

char const *
Sock::get_sinful_public() const
{
		// In case TCP_FORWARDING_HOST changes, do not cache it.
	std::string tcp_forwarding_host;
	param(tcp_forwarding_host,"TCP_FORWARDING_HOST");
	if (!tcp_forwarding_host.empty()) {
		condor_sockaddr addr;
		
		if (!addr.from_ip_string(tcp_forwarding_host)) {
			std::vector<condor_sockaddr> addrs = resolve_hostname(tcp_forwarding_host);
			if (addrs.empty()) {
				dprintf(D_ALWAYS,
					"failed to resolve address of TCP_FORWARDING_HOST=%s\n",
					tcp_forwarding_host.c_str());
				return NULL;
			}
			addr = addrs.front();
		}
		addr.set_port(get_port());
		_sinful_public_buf = addr.to_sinful().c_str();

		std::string alias;
		if( param(alias,"HOST_ALIAS") ) {
			Sinful s(_sinful_public_buf.c_str());
			s.setAlias(alias.c_str());
			_sinful_public_buf = s.getSinful();
		}

		return _sinful_public_buf.c_str();
	}

	return get_sinful();
}
