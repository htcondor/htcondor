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
int condor_read(SOCKET fd, char *buf, int sz, int timeout)
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
				dprintf(D_ALWAYS, "Timeout reading buffer.\n");
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
				dprintf(D_ALWAYS, "Timeout reading buffer.\n");
				return -1;

			  case 1:
				break;

			  default:
                if(errno == EINTR) continue;
				dprintf( D_ALWAYS, "select returns %d, assuming failure.\n",
						 nfound );
				return -1;
			}
		}

		nro = recv(fd, &buf[nr], sz - nr, 0);
        //cerr << "condor_read: " << nro << endl;
		
		if( nro < 0 ) {
			dprintf( D_ALWAYS, 
					 "recv returned %d, errno = %d, assuming failure.\n",
					 nro, errno );
            //cerr << "condor_read: recv failed: " << strerror(errno) << endl;
			return -1;
		} else if( nro == 0 ) {
			return nr;
		}

		nr += nro;
	}	
	
/* Post Conditions */
	ASSERT(nr > 0); /* We should have read at least SOME data */
	return nr;
}




int condor_write(SOCKET fd, char *buf, int sz, int timeout) {

    int nw = 0, nwo = 0;
    unsigned int start_time = 0, cur_time = 0;
    struct timeval timer;
    fd_set writefds;
    int nfds = 0, nfound = 0;

    /* Pre-conditions. */
    ASSERT(sz > 0);      /* Can't write buffers that are have no data */
    ASSERT(fd >= 0);     /* Need valid file descriptor */
    ASSERT(buf != NULL); /* Need valid buffer to write */


    memset( &timer, 0, sizeof( struct timeval ) );
    memset( &writefds, 0, sizeof( fd_set ) );

    if(timeout > 0) {
        start_time = time(NULL);
        cur_time = start_time;
    }

    while( nw < sz ) {

        if( timeout > 0 ) {

            if( cur_time == 0 ) {
                cur_time = time(NULL);
            }

            if( start_time + timeout > cur_time ) {
                timer.tv_sec = (start_time + timeout) - cur_time;
            } else {
                dprintf(D_ALWAYS, "Timed out writing buffer '%s'\n",
                        buf);
                return -1;
            }

            cur_time = 0;
            timer.tv_usec = 0;

#ifndef WIN32
            nfds = fd + 1;
#endif
            FD_ZERO( &writefds );
            FD_SET( fd, &writefds );

            nfound = select( nfds, 0, &writefds, &writefds, &timer );

            switch(nfound) {
              case 0:
                dprintf( D_ALWAYS, "Timed out writing buffer '%s'\n",
                        buf);
                return -1;

              case 1:
                break;

              default:
                dprintf( D_ALWAYS,
                        "Select returns %d, assuming failure.\n");
                return -1;

            }
        }

        nwo = send(fd, &buf[nw], sz - nw, 0);

        if( nwo <= 0 ) {
            int the_error;
#ifdef WIN32
            the_error = WSAGetLastError();
            if ( the_error == WSAEWOULDBLOCK ) {
                dprintf(D_FULLDEBUG,"send return WSAEWOULDBLOCK, try again\n");
                continue;
            }
#else
            the_error = errno;
#endif

            dprintf( D_ALWAYS,
                    "send returned %d, timeout=%d, errno=%d. Assuming failure.\n",
                    nwo, timeout, the_error);
            return -1;
        }

        nw += nwo;
    }

    /* POST conditions. */
    ASSERT( nw == sz ); /* Make sure that we wrote everything */
    return nw;
}


int
condor_mux_read(SOCKET fd, SOCKET mfd, char *buf, int sz, int timeout, bool &ready)
{
	int nr = 0, nro;
	unsigned int start_time, cur_time;
	struct timeval *timer = NULL;
	fd_set rdfds;
	int maxfd, nfound;

#ifndef WIN32
	maxfd = (fd > mfd) ? fd + 1 : mfd + 1;
#endif
	
	if (timeout > 0) {
		timer = (struct timeval*)malloc(sizeof(struct timeval));
		start_time = time(NULL);
		cur_time = start_time;
	}

	while( nr < sz ) {
		FD_ZERO(&rdfds);
		FD_SET(fd, &rdfds);
		FD_SET(mfd, &rdfds);
		
		// check if timed out
		if(timeout > 0) {
			if(cur_time == 0)
				cur_time = time(NULL);
			if(start_time + timeout > cur_time) {
				timer->tv_sec = start_time + timeout - cur_time;
				timer->tv_usec = 0;
				cur_time = 0;
			} else {
				dprintf(D_ALWAYS, "Timeout reading buffer.\n");
				free(timer);
				return -1;
			}
		}

		nfound = select(maxfd, &rdfds, 0, 0, timer);
        if(nfound < 0) {
            if(errno == EINTR) continue;
			dprintf(D_ALWAYS, "condor_mux_read: select failed: %s\n", strerror(errno));
            return -1;
        }
		if(nfound == 0) {
			dprintf(D_ALWAYS, "Timeout reading buffer.\n");
			if(timer) free(timer);
			return -1;
		}
		if(FD_ISSET(mfd, &rdfds))
			ready = true;
		if(FD_ISSET(fd, &rdfds)) {
			nro = recv(fd, &buf[nr], sz - nr, 0);
            //cerr << "condor_mux_read: " << nro << endl;
			if( nro < 0 ) {
				dprintf(D_ALWAYS, "condor_read -\
				                   read failed, errno = %d\n",
				        nro, errno);
				if(timer) free(timer);
                //cerr << "condor_mux_read: recv failed " << strerror(errno) << endl;
				return -1;
			}
			nr += nro;
		}
	} // end of while
		
	if(timer) free(timer);

	return nr;
}


int
condor_mux_write(SOCKET fd, SOCKET mfd, char *buf, int sz, int timeout,
                 int threshold/*in usec*/, bool &congested, bool &ready)
{
    int nw = 0, nwo;
    unsigned int start_time, cur_time;
    fd_set rdfds, wrfds;
    int maxfd, nfound;
	struct timeval curr, prev;
    struct timeval timer;
    double elapsed;

#ifndef WIN32
    maxfd = (fd > mfd) ? fd + 1 : mfd + 1;
#endif

    if (timeout > 0) {
        start_time = time(NULL);
        cur_time = start_time;
    }

    while( nw < sz ) {
        FD_ZERO(&rdfds);
        FD_SET(mfd, &rdfds);
        FD_ZERO(&wrfds);
        FD_SET(fd, &wrfds);

        // check if timed out
        if(timeout > 0) {
            if(cur_time == 0)
                cur_time = time(NULL);
            if(start_time + timeout > cur_time) {
                timer.tv_sec = start_time + timeout - cur_time;
                timer.tv_usec = 0;
                cur_time = 0;
            } else {
                dprintf(D_ALWAYS, "Timeout writing buffer.\n");
                return -1;
            }
        }

        if(threshold > 0) {
            (void) gettimeofday(&prev, NULL);
        }

        nfound = select(maxfd, &rdfds, &wrfds, 0, &timer);

        if(nfound < 0) {
            if(errno == EINTR) {
                //cerr << "select interrupted\n";
                continue;
            }
            dprintf(D_ALWAYS, "condor_mux_write: select failed: %s\n", strerror(errno));
            return -1;
        }
        if(nfound == 0) {
            dprintf(D_ALWAYS, "Timeout writing buffer.\n");
            return -1;
        }
        if(FD_ISSET(fd, &wrfds)) {
		    nwo = send(fd, &buf[nw], sz - nw, 0);
            if( nwo < 0 ) {
                dprintf(D_ALWAYS, "condor_mux_write -\
                                   send failed, errno = %d\n", errno);
                return -1;
            } else if(nwo == 0) {
                dprintf(D_ALWAYS, "condor_mux_write -\
                                   send failed, connection closed prematurely\n");
                return -1;
            } else if(nwo < sz - nw) {
                congested = true;
            }
            nw += nwo;
            if(threshold > 0) {
                (void) gettimeofday(&curr, NULL);
                elapsed = (double)((curr.tv_sec - prev.tv_sec)*1000000.0 + (curr.tv_usec - prev.tv_usec));
                //cout << "elapsed: " << elapsed << endl;
                if(elapsed >= (double)threshold) {
                    congested = true;
                    //cout << "elapsed: " << elapsed << "      threshold: " << threshold << endl;
                }
            }
        }
        if(FD_ISSET(mfd, &rdfds)) {
            ready = true;
        }
    } // end of while


    return nw;
}
