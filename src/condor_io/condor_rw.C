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


#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"
#include "selector.h"

/*
 * Returns true if the given error number indicates
 * a temporary condition that may immediately be retried.
 */

static int errno_is_temporary( int e )
{
#ifdef WIN32
	if( e==WSAEWOULDBLOCK ) {
		return 1;
	} else {
		return 0;
	}
#else
	if( e==EAGAIN || e==EWOULDBLOCK || e==EINTR ) {
		return 1;
	} else {
		return 0;
	}
#endif
}

/* Generic read/write wrappers for condor.  These function emulate the 
 * read/write system calls under unix except that they are portable, use
 * a timeout, and make sure that all data is read or written.
 * Returns < 0 on failure.  -1 = general error, -2 = peer closed socket.
 */
int
condor_read( SOCKET fd, char *buf, int sz, int timeout, int flags )
{
	Selector selector;
	int nr = 0, nro;
	unsigned int start_time, cur_time;
	char sinbuf[SINFUL_STRING_BUF_SIZE];
	
	/* PRE Conditions. */
	ASSERT(fd >= 0);     /* Need valid file descriptor */
	ASSERT(buf != NULL); /* Need real memory to put data into */
	ASSERT(sz > 0);      /* Need legit size on buffer */

	selector.add_fd( fd, Selector::IO_READ );
	
	if ( timeout > 0 ) {
		start_time = time(NULL);
		cur_time = start_time;
	}

	while( nr < sz ) {

		if( timeout > 0 ) {

			if( cur_time == 0 ) {
				cur_time = time(NULL);
			}

			// If it hasn't yet been longer then we said we would wait...
			if( start_time + timeout > cur_time ) {
				selector.set_timeout( (start_time + timeout) - cur_time );
			} else {
				dprintf( D_ALWAYS, 
						 "condor_read(): timeout reading %d bytes from %s.\n",
						 sz,
						 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
				return -1;
			}
			
			cur_time = 0;

			if( DebugFlags & D_FULLDEBUG && DebugFlags & D_NETWORK ) {
				dprintf(D_FULLDEBUG, "condor_read(): fd=%d\n", fd);
			}
			selector.execute();
			if( DebugFlags & D_FULLDEBUG && DebugFlags & D_NETWORK ) {
				dprintf(D_FULLDEBUG, "condor_read(): select returned %d\n", 
						selector.select_retval());
			}

			if ( selector.timed_out() ) {
				dprintf( D_ALWAYS, 
						 "condor_read(): timeout reading %d bytes from %s.\n",
						 sz,
						 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
				return -1;

			} else if ( selector.signalled() ) {
				continue;
			} else if ( !selector.has_ready() ) {
				int the_error;
#ifdef WIN32
				the_error = WSAGetLastError();
#else
				the_error = errno;
#endif
				dprintf( D_ALWAYS, "condor_read(): select() "
						 "returns %d, assuming failure reading %d bytes from %s (errno=%d).\n",
						 selector.select_retval(),
						 sz,
						 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source"),
						 the_error );
				return -1;
			}
		}

		nro = recv(fd, &buf[nr], sz - nr, flags);
		if( nro <= 0 ) {

				// If timeout > 0, and we made it here, then
				// we know we were woken by select().  Now, if
				// select() wakes up on a read fd, and then recv() 
				// subsequently returns 0, that means that the
				// socket has been closed by our peer.
				// If timeout == 0, then recv() should have 
				// blocked until 1 or more bytes arrived.
				// Thus no matter what, if nro==0, then the
				// socket must be closed.
			if ( nro == 0 ) {
				dprintf( D_FULLDEBUG, "condor_read(): "
						 "Socket closed when trying to read %d bytes from %s\n",
						 sz,
						 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
				return -2;
	}

			int the_error;
#ifdef WIN32
			the_error = WSAGetLastError();
#else
			the_error = errno;
#endif
			if ( errno_is_temporary(the_error) ) {
				dprintf( D_FULLDEBUG, "condor_read(): "
				         "recv() returned temporary error %d,"
				         "still trying to read from %s\n",
				         the_error,
				         sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
				continue;
			}

			dprintf( D_ALWAYS, "condor_read(): recv() returned %d, "
					 "errno = %d, assuming failure reading %d bytes from %s.\n",
					 nro, the_error, sz,
					 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
			return -1;
		}
		nr += nro;
	}
	
/* Post Conditions */
	ASSERT( nr == sz );  // we should have read *ALL* the data
	return nr;
}


int
condor_write( SOCKET fd, char *buf, int sz, int timeout, int flags )
{
	Selector selector;
	int nw = 0, nwo = 0;
	unsigned int start_time = 0, cur_time = 0;
	char tmpbuf[1];
	int nro;
	bool select_for_read = true;
	bool needs_select = true;
	char sinbuf[SINFUL_STRING_BUF_SIZE];

	/* Pre-conditions. */
	ASSERT(sz > 0);      /* Can't write buffers that are have no data */
	ASSERT(fd >= 0);     /* Need valid file descriptor */
	ASSERT(buf != NULL); /* Need valid buffer to write */

	selector.add_fd( fd, Selector::IO_READ );
	selector.add_fd( fd, Selector::IO_WRITE );
	selector.add_fd( fd, Selector::IO_EXCEPT );
	
	if(timeout > 0) {
		start_time = time(NULL);
		cur_time = start_time;
	}

	while( nw < sz ) {

		needs_select = true;

		if( timeout > 0 ) {
			while( needs_select ) {
				if( cur_time == 0 ) {
					cur_time = time(NULL);
				}

				if( start_time + timeout > cur_time ) {
					selector.set_timeout( (start_time + timeout) - cur_time );
				} else {
					dprintf( D_ALWAYS, "condor_write(): "
							 "timed out writing %d bytes to %s\n",
							 sz,
							 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
					return -1;
				}
			
				cur_time = 0;

					// The write and except sets are added at the top of
					// this function, since we always want to select on
					// them.
				if( select_for_read ) {
						// Also, put it in the read fds, so we'll wake
						// up if the socket is closed
					selector.add_fd( fd, Selector::IO_READ );
				} else {
					selector.delete_fd( fd, Selector::IO_READ );
				}
				selector.execute();

					// unless we decide we need to select() again, we
					// want to break out of our while() loop now that
					// we've actually performed a select()
				needs_select = false;

				if ( selector.timed_out() ) {
					dprintf( D_ALWAYS, "condor_write(): "
							 "timed out writing %d bytes to %s\n",
							 sz,
							 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
					return -1;
				
				} else if ( selector.signalled() ) {
					needs_select = true;
					continue;
				} else if ( selector.has_ready() ) {
					if ( selector.fd_ready( fd, Selector::IO_READ ) ) {
							// see if the socket was closed
						nro = recv(fd, tmpbuf, 1, MSG_PEEK);
						if( nro == -1 ) {
							int the_error;
#ifdef WIN32
							the_error = WSAGetLastError();
#else
							the_error = errno;
#endif
							if(errno_is_temporary( the_error )) {
								continue;
							}

							dprintf( D_ALWAYS, "condor_write(): "
									 "Socket closed when trying "
									 "to write %d bytes to %s, fd is %d, "
									 "errno=%d\n",
									 sz,
									 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source"),
									 fd, the_error );
							return -1;
						}

						if( ! nro ) {
							dprintf( D_ALWAYS, "condor_write(): "
									 "Socket closed when trying "
									 "to write %d bytes to %s, fd is %d\n",
									 sz,
									 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source"),
									 fd );
							return -1;
						}

							/*
							  otherwise, there's real data to consume
							  on the read side, and we don't want to
							  put our fd in the readfds anymore or
							  select() will never block.  also, we
							  need to re-do the select()
							*/
						needs_select = true;
						select_for_read = false;
					}
				} else {
					dprintf( D_ALWAYS, "condor_write(): select() "
							 "returns %d, assuming failure "
							 "writing %d bytes to %s.\n",
							 selector.select_retval(),
							 sz,
							 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
					return -1;
				}
			}
		}
		
		nwo = send(fd, &buf[nw], sz - nw, flags);

		if( nwo <= 0 ) {
			int the_error;
#ifdef WIN32
			the_error = WSAGetLastError();
#else
			the_error = errno;
#endif
			if ( errno_is_temporary(the_error) ) {
				dprintf( D_FULLDEBUG, "condor_write(): "
				         "send() returned temporary error %d,"
						 "still trying to write %d bytes to %s\n", the_error,
						 sz,
						 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source") );
				continue;
			}

#ifdef WIN32
				// alas, apparently there's no version of strerror()
				// that works on the codes you get back from
				// WSAGetLastError(), so we need a seperate dprintf()
				// that doesn't include a %s for error string.
			dprintf( D_ALWAYS, "condor_write(): send() %d bytes to %s "
					 "returned %d, "
					 "timeout=%d, errno=%d.  Assuming failure.\n",
					 sz,
					 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source" ),
					 nwo, timeout, the_error );
#else
			dprintf( D_ALWAYS, "condor_write(): send() %d bytes to %s "
					 "returned %d, "
					 "timeout=%d, errno=%d (%s).  Assuming failure.\n",
					 sz,
					 sock_peer_to_string(fd, sinbuf, sizeof(sinbuf), "unknown source"),
					 nwo, timeout, the_error, strerror(the_error) );
#endif
			return -1;
		}
		
		nw += nwo;
	}

	/* POST conditions. */
	ASSERT( nw == sz ); /* Make sure that we wrote everything */
	return nw;
}


