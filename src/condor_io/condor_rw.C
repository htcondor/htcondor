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
#include "condor_io.h"
#include "condor_debug.h"


/* Generic read/write wrappers for condor.  These function emulate the 
 * read/write system calls under unix except that they are portable, use
 * a timeout, and make sure that all data is read or written.
 */
int
condor_read( SOCKET fd, char *buf, int sz, int timeout )
{
	int nr = 0, nro;
	unsigned int start_time, cur_time;
	struct timeval timer;
	fd_set readfds;
	int nfds = 0, nfound;
	
	/* PRE Conditions. */
	ASSERT(fd >= 0);     /* Need valid file descriptor */
	ASSERT(buf != NULL); /* Need real memory to put data into */
	ASSERT(sz > 0);      /* Need legit size on buffer */
	
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
				timer.tv_sec = (start_time + timeout) - cur_time;
			} else {
				dprintf( D_ALWAYS, 
						 "condor_read(): timeout reading buffer.\n" );
				return -1;
			}
			
			cur_time = 0;
			timer.tv_usec = 0;
#ifndef WIN32
			nfds = fd + 1;
#endif
			FD_ZERO( &readfds );
			FD_SET( fd, &readfds );

			nfound = select( nfds, &readfds, 0, 0, &timer );

			switch(nfound) {
			case 0:
				dprintf( D_ALWAYS, 
						 "condor_read(): timeout reading buffer.\n" );
				return -1;

			case 1:
				break;

			default:
				dprintf( D_ALWAYS, "condor_read(): select() "
						 "returns %d, assuming failure.\n", nfound );
				return -1;
			}
		}

		nro = recv(fd, &buf[nr], sz - nr, 0);
		if( nro <= 0 ) {
			int the_error;
#ifdef WIN32
			the_error = WSAGetLastError();
			if ( the_error == WSAEWOULDBLOCK ) {
				dprintf( D_FULLDEBUG, "condor_read(): "
						 "recv() returns WSAEWOULDBLOCK, try again\n" );
				continue;
			}
#else
			the_error = errno;
#endif
			dprintf( D_ALWAYS, "condor_read(): recv() returned %d, "
					 "errno = %d, assuming failure.\n",
					 nro, the_error );
			return -1;
		}
		nr += nro;
	}
	
/* Post Conditions */
	ASSERT( nr == sz );  // we should have read *ALL* the data
	return nr;
}


int
condor_write( SOCKET fd, char *buf, int sz, int timeout )
{
	int nw = 0, nwo = 0;
	unsigned int start_time = 0, cur_time = 0;
	struct timeval timer;
	fd_set writefds;
	fd_set readfds;
	fd_set exceptfds;
	int nfds = 0, nfound = 0;
	char tmpbuf[1];
	int nro;
	bool select_for_read = true;
	bool needs_select = true;

	/* Pre-conditions. */
	ASSERT(sz > 0);      /* Can't write buffers that are have no data */
	ASSERT(fd >= 0);     /* Need valid file descriptor */
	ASSERT(buf != NULL); /* Need valid buffer to write */
	
	
	memset( &timer, 0, sizeof( struct timeval ) );
	memset( &writefds, 0, sizeof( fd_set ) );
	memset( &readfds, 0, sizeof( fd_set ) );
	
	if(timeout > 0) {
		start_time = time(NULL);
		cur_time = start_time;
	}

	while( nw < sz ) {

		if( timeout > 0 ) {
			while( needs_select ) {
				if( cur_time == 0 ) {
					cur_time = time(NULL);
				}

				if( start_time + timeout > cur_time ) {
					timer.tv_sec = (start_time + timeout) - cur_time;
				} else {
					dprintf( D_ALWAYS, "condor_write(): "
							 "timed out writing buffer\n" );
					return -1;
				}
			
				cur_time = 0;
				timer.tv_usec = 0;

#ifndef WIN32
				nfds = fd + 1;
#endif
				FD_ZERO( &writefds );
				FD_SET( fd, &writefds );
				FD_ZERO( &exceptfds );
				FD_SET( fd, &exceptfds );
				
				FD_ZERO( &readfds );
				if( select_for_read ) {
						// Also, put it in the read fds, so we'll wake
						// up if the socket is closed
					FD_SET( fd, &readfds );
				}
				nfound = select( nfds, &readfds, &writefds, &exceptfds, 
								 &timer ); 

					// unless we decide we need to select() again, we
					// want to break out of our while() loop now that
					// we've actually performed a select()
				needs_select = false;

				switch(nfound) {
				case 0:
					dprintf( D_ALWAYS, "condor_write(): "
							 "timed out writing buffer\n" );
					return -1;
				
				case 1:
				case 2:
				case 3:
					if( FD_ISSET(fd, &readfds) ) {
							// see if the socket was closed
						nro = recv(fd, tmpbuf, 1, MSG_PEEK);
						if( ! nro ) {
							dprintf( D_ALWAYS, "condor_write(): "
									 "Socket closed when trying "
									 "to write buffer\n" );
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
					break;
				default:
					dprintf( D_ALWAYS, "condor_write(): select() "
							 "returns %d, assuming failure.\n", nfound );
					return -1;
				}
			}
		}
		
		nwo = send(fd, &buf[nw], sz - nw, 0);

		if( nwo <= 0 ) {
			int the_error;
#ifdef WIN32
			the_error = WSAGetLastError();
			if ( the_error == WSAEWOULDBLOCK ) {
				dprintf( D_FULLDEBUG, "condor_write(): "
						 "send() returns WSAEWOULDBLOCK, try again\n" );
				continue;
			}
#else
			the_error = errno;
#endif
			dprintf( D_ALWAYS, "condor_write(): send() returned %d, "
					 "timeout=%d, errno=%d.  Assuming failure.\n",
					 nwo, timeout, the_error );
			return -1;
		}
		
		nw += nwo;
	}

	/* POST conditions. */
	ASSERT( nw == sz ); /* Make sure that we wrote everything */
	return nw;
}


