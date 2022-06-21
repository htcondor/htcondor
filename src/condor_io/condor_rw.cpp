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
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"
#include "selector.h"
#include "condor_threads.h"
#include "condor_sockfunc.h"

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

static int errno_socket_dead( int e )
{
	switch( e ) {
#ifdef WIN32
	case WSAENETRESET:
	case WSAECONNABORTED:
	case WSAECONNRESET:
	case WSAESHUTDOWN:
	case WSAETIMEDOUT:
	case WSAECONNREFUSED:
#else
	case ECONNRESET:
	case ENOTCONN:
	case ETIMEDOUT:
#endif
		return 1;
	default:
		return 0;
	}
}

static char const *not_null_peer_description(char const *peer_description,SOCKET fd,char *sinbuf)
{
    if( peer_description ) {
        return peer_description;
    }

	condor_sockaddr addr;
	if (condor_getpeername(fd, addr) < 0) {
		return "disconnected socket";
	}

	addr.to_sinful(sinbuf, SINFUL_STRING_BUF_SIZE);
	return sinbuf;
}

/* Generic read/write wrappers for condor.  These function emulate-ish the 
 * read/write system calls under unix except that they are portable, use
 * a timeout, and make sure that all data is read or written.
 *
 * A few notes on the behavior differing from POSIX:
 * - These will never fail due to EINTR.
 * - If in non_blocking mode, there may be a short read or write returned.
 * - The corresponding POSIX functon returns 0 bytes read when the peer closed
 *   the socket; these return -2.
 * - If zero bytes were read/written in non-blocking mode, this will return 0.  This differs
 *   from POSIX.
 * - Providing a zero-sized argument to this function will cause the program to abort().
 *
 * Returns < 0 on failure.  -1 = general error, -2 = peer closed socket
 */
int
condor_read( char const *peer_description, SOCKET fd, char *buf, int sz, int timeout, int flags, bool non_blocking )
{
	Selector selector;
	int nr = 0, nro;
	unsigned int start_time=0, cur_time=0;
	char sinbuf[SINFUL_STRING_BUF_SIZE];

	if( IsDebugLevel(D_NETWORK) ) {
		dprintf(D_NETWORK,
				"condor_read(fd=%d %s,,size=%d,timeout=%d,flags=%d,non_blocking=%d)\n",
				fd,
				not_null_peer_description(peer_description,fd,sinbuf),
				sz,
				timeout,
				flags,
				non_blocking);
	}

	/* PRE Conditions. */
	ASSERT(fd >= 0);     /* Need valid file descriptor */
	ASSERT(buf != NULL); /* Need real memory to put data into */
	ASSERT(sz > 0);      /* Need legit size on buffer */

	if (non_blocking) {
#ifdef WIN32
		unsigned long mode = 1; // nonblocking mode
		if (ioctlsocket(fd, FIONBIO, &mode) < 0)
			return -1;
#else
		int fcntl_flags;
		if ( (fcntl_flags=fcntl(fd, F_GETFL)) < 0 )
			return -1;
			// set nonblocking mode
		if ( ((fcntl_flags & O_NONBLOCK) == 0) && fcntl(fd, F_SETFL, fcntl_flags | O_NONBLOCK) == -1 )
			return -1;
#endif
		nr = -2;
		while (nr == -2 || (nr == -1 && errno == EINTR)) {
			nr = recv(fd, buf, sz, flags);
		}

		if ( nr <= 0 ) {
			int the_error;
			char const *the_errorstr;
#ifdef WIN32
			the_error = WSAGetLastError();
			the_errorstr = "";
#else
			the_error = errno;
			the_errorstr = strerror(the_error);
#endif
			if ( nr == 0 && !(flags & MSG_PEEK)) {
				nr = -2;
				dprintf( D_FULLDEBUG, "condor_read(): "
					"Socket closed when trying to read %d bytes from %s in non-blocking mode\n",
					sz,
					not_null_peer_description(peer_description,fd,sinbuf) );
			} else if ( errno_socket_dead(the_error) ) {
				nr = -2;
				dprintf( D_ALWAYS, "condor_read(): "
					"Socket closed abnormally when trying to read %d bytes from %s in non-blocking mode, errno=%d %s\n",
					sz,
					not_null_peer_description(peer_description,fd,sinbuf),
					the_error, the_errorstr );
			} else if ( !errno_is_temporary(the_error) ) {
				dprintf( D_ALWAYS, "condor_read() failed: recv() %d bytes from %s "
					"returned %d, "     
					"timeout=%d, errno=%d %s.\n",
					sz,                 
					not_null_peer_description(peer_description,fd,sinbuf),
					nr, timeout, the_error, the_errorstr );
			}
			else
			{
				nr = 0;
			}
		}

#ifdef WIN32
		mode = 0; // reset blocking mode
		if (ioctlsocket(fd, FIONBIO, &mode) < 0)
			return -1;
#else
			// reset flags to prior value
		if ( ((fcntl_flags & O_NONBLOCK) == 0) && (fcntl(fd, F_SETFL, fcntl_flags) == -1) )
			return -1;
#endif
		return nr;
	}

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
						 not_null_peer_description(peer_description,fd,sinbuf) );
				return -1;
			}
			
			cur_time = 0;

			if( IsDebugVerbose( D_NETWORK ) ) {
				dprintf(D_NETWORK, "condor_read(): fd=%d\n", fd);
			}
			selector.execute();
			if( IsDebugVerbose( D_NETWORK ) ) {
				dprintf(D_NETWORK, "condor_read(): select returned %d\n", 
						selector.select_retval());
			}

			if ( selector.timed_out() ) {
				dprintf( D_ALWAYS, 
						 "condor_read(): timeout reading %d bytes from %s.\n",
						 sz,
						 not_null_peer_description(peer_description,fd,sinbuf) );
				return -1;

			} else if ( selector.signalled() ) {
				continue;
			} else if ( !selector.has_ready() ) {
				int the_error;
                char const *the_errorstr;
#ifdef WIN32
				the_error = WSAGetLastError();
                the_errorstr = "";
#else
				the_error = errno;
                the_errorstr = strerror(the_error);
#endif

				dprintf( D_ALWAYS, "condor_read() failed: select() "
						 "returns %d, reading %d bytes from %s (errno=%d %s).\n",
						 selector.select_retval(),
						 sz,
						 not_null_peer_description(peer_description,fd,sinbuf),
						 the_error, the_errorstr );
				return -1;
			}
		}
		
		start_thread_safe("recv");

		nro = recv(fd, &buf[nr], sz - nr, flags);
		// Save the error value before stop_thread_safe(), as that may
		// overwrite it.
		int the_error;
#ifdef WIN32
		the_error = WSAGetLastError();
#else
		the_error = errno;
#endif

		stop_thread_safe("recv");

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
						 not_null_peer_description(peer_description,fd,sinbuf) );
				return -2;
			}

            char const *the_errorstr;
#ifdef WIN32
            the_errorstr = "";
#else
            the_errorstr = strerror(the_error);
#endif
			if( the_error == ETIMEDOUT ) {
				if( timeout <= 0 ) {
					dprintf( D_ALWAYS,
							 "condor_read(): read timeout during blocking read from %s\n",
							 not_null_peer_description(peer_description,fd,sinbuf));
				}
				else {
					int lapse = (int)(time(NULL)-start_time);
					dprintf( D_ALWAYS,
							 "condor_read(): UNEXPECTED read timeout after %ds during non-blocking read from %s (desired timeout=%ds)\n",
							 lapse,
							 not_null_peer_description(peer_description,fd,sinbuf),
							 timeout);
				}
			}
			if ( errno_is_temporary(the_error) ) {
				dprintf( D_FULLDEBUG, "condor_read(): "
				         "recv() returned temporary error %d %s,"
				         "still trying to read from %s\n",
				         the_error,the_errorstr,
				         not_null_peer_description(peer_description,fd,sinbuf) );
				continue;
			} else if ( errno_socket_dead(the_error) ) {
				dprintf( D_ALWAYS, "condor_read(): "
					"Socket closed abnormally when trying to read %d bytes from %s, errno=%d %s\n",
					sz,
					not_null_peer_description(peer_description,fd,sinbuf),
					the_error, the_errorstr );
				return -2;
			}
			dprintf( D_ALWAYS, "condor_read() failed: recv(fd=%d) returned %d, "
					 "errno = %d %s, reading %d bytes from %s.\n",
					 fd, nro, the_error, the_errorstr, sz,
					 not_null_peer_description(peer_description,fd,sinbuf) );

			return -1;
		}
		nr += nro;
	}
	
/* Post Conditions */
	ASSERT( nr == sz );  // we should have read *ALL* the data
	return nr;
}


int
condor_write( char const *peer_description, SOCKET fd, const char *buf, int sz, int timeout, int flags, bool non_blocking )
{
	int nw = 0, nwo = 0;
	unsigned int start_time = 0, cur_time = 0;
	char tmpbuf[1];
	int nro;
	bool select_for_read = true;
	bool needs_select = true;
	char sinbuf[SINFUL_STRING_BUF_SIZE];

	if( IsDebugLevel( D_NETWORK ) ) {
		dprintf(D_NETWORK,
				"condor_write(fd=%d %s,,size=%d,timeout=%d,flags=%d,non_blocking=%d)\n",
				fd,
				not_null_peer_description(peer_description,fd,sinbuf),
				sz,
				timeout,
				flags,
				non_blocking);
	}

	/* Pre-conditions. */
	ASSERT(sz > 0);      /* Can't write buffers that are have no data */
	ASSERT(fd >= 0);     /* Need valid file descriptor */
	ASSERT(buf != NULL); /* Need valid buffer to write */

	if (non_blocking) {
#ifdef WIN32
		unsigned long mode = 1; // nonblocking mode
		if (ioctlsocket(fd, FIONBIO, &mode) < 0)
			return -1;
#else
		int fcntl_flags;
		if ( (fcntl_flags=fcntl(fd, F_GETFL)) < 0 )
			return -1;
			// set nonblocking mode
		if ( ((fcntl_flags & O_NONBLOCK) == 0) && fcntl(fd, F_SETFL, fcntl_flags | O_NONBLOCK) == -1 )
			return -1;
#endif
		nw = -2;
		while (nw == -2 || (nw == -1 && errno == EINTR)) {
			nw = send(fd, buf, sz, flags);
		}

		if ( nw <= 0 ) {
			int the_error;
			char const *the_errorstr;
#ifdef WIN32
			the_error = WSAGetLastError();
			the_errorstr = "";
#else
			the_error = errno;
			the_errorstr = strerror(the_error);
#endif
			if ( !errno_is_temporary(the_error) ) {
				dprintf( D_ALWAYS, "condor_write() failed: send() %d bytes to %s "
					"returned %d, "     
					"timeout=%d, errno=%d %s.\n",
					sz,                 
					not_null_peer_description(peer_description,fd,sinbuf),
					nw, timeout, the_error, the_errorstr );
			}
			else
			{
				nw = 0;
			}
		}
		if (nw < 0) {
			dprintf(D_NETWORK, "condor_write (non-blocking) wrote %d bytes.\n", nw);
		}       

#ifdef WIN32
		mode = 0; // reset blocking mode
		if (ioctlsocket(fd, FIONBIO, &mode) < 0)
			return -1;
#else
			// reset flags to prior value
		if ( ((fcntl_flags & O_NONBLOCK) == 0) && fcntl(fd, F_SETFL, fcntl_flags) == -1 )
			return -1;
#endif
		return nw;
	}

	Selector selector;
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
							 not_null_peer_description(peer_description,fd,sinbuf) );
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
							 not_null_peer_description(peer_description,fd,sinbuf) );
					return -1;
				
				} else if ( selector.signalled() ) {
					needs_select = true;
					continue;
				} else if ( selector.has_ready() ) {
					if ( selector.fd_ready( fd, Selector::IO_READ ) ) {
						dprintf(D_NETWORK, "condor_write(): socket %d is readable\n", fd);
							// see if the socket was closed
						nro = recv(fd, tmpbuf, 1, MSG_PEEK);
						if( nro == -1 ) {
							int the_error;
                            char const *the_errorstr;
#ifdef WIN32
							the_error = WSAGetLastError();
                            the_errorstr = "";
#else
							the_error = errno;
                            the_errorstr = strerror(the_error);
#endif
							if(errno_is_temporary( the_error )) {
								continue;
							}

							dprintf( D_ALWAYS, "condor_write(): "
									 "Socket closed when trying "
									 "to write %d bytes to %s, fd is %d, "
									 "errno=%d %s\n",
									 sz,
									 not_null_peer_description(peer_description,fd,sinbuf),
									 fd, the_error, the_errorstr );
							return -1;
						}

						if( ! nro ) {
							dprintf( D_ALWAYS, "condor_write(): "
									 "Socket closed when trying "
									 "to write %d bytes to %s, fd is %d\n",
									 sz,
									 not_null_peer_description(peer_description,fd,sinbuf),
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
					dprintf( D_ALWAYS, "condor_write() failed: select() "
							 "returns %d, "
							 "writing %d bytes to %s.\n",
							 selector.select_retval(),
							 sz,
							 not_null_peer_description(peer_description,fd,sinbuf) );
					return -1;
				}
			}
		}
		start_thread_safe("send");

		nwo = send(fd, &buf[nw], sz - nw, flags);
		// Save the error value before stop_thread_safe(), as that may
		// overwrite it.
		int the_error;
#ifdef WIN32
		the_error = WSAGetLastError();
#else
		the_error = errno;
#endif

		stop_thread_safe("send");		

		if( nwo <= 0 ) {
            char const *the_errorstr;
#ifdef WIN32
            the_errorstr = "";
#else
            the_errorstr = strerror(the_error);
#endif
			if ( errno_is_temporary(the_error) ) {
				dprintf( D_FULLDEBUG, "condor_write(): "
				         "send() returned temporary error %d %s,"
						 "still trying to write %d bytes to %s\n", the_error,
						 the_errorstr, sz,
						 not_null_peer_description(peer_description,fd,sinbuf) );
				continue;
			}

			dprintf( D_ALWAYS, "condor_write() failed: send() %d bytes to %s "
					 "returned %d, "
					 "timeout=%d, errno=%d %s.\n",
					 sz,
					 not_null_peer_description(peer_description,fd,sinbuf),
					 nwo, timeout, the_error, the_errorstr );

			return -1;
		}
		
		nw += nwo;
	}

	/* POST conditions. */
	ASSERT( nw == sz ); /* Make sure that we wrote everything */
	return nw;
}


