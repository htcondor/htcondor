/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef _NETWORK
#define _NETWORK

#define SCHED_PORT					9605

#define NEGOTIATOR_PORT					9614

#define ACCOUNTANT_PORT					9616

#define START_PORT						9611
#define START_UDP_PORT					9611

#define COLLECTOR_PORT					9618
#define COLLECTOR_UDP_PORT				9618
#define COLLECTOR_COMM_PORT				9618
#define COLLECTOR_UDP_COMM_PORT				9618
#define CREDD_PORT						9620
#define STORK_PORT						9621

#define CONDOR_VIEW_PORT				COLLECTOR_PORT

#define	DEFAULT_CONFIG_SERVER_PORT		    9600

#if defined(__cplusplus)
extern "C" {
#endif

#if defined( __STDC__) || defined(__cplusplus)
int do_connect ( const char *host, const char *service, unsigned int port );
int tcp_accept_timeout( int ConnectionSock, struct sockaddr *sin, 
						int *len, int timeout );
#else
int do_connect ();
int tcp_accept_timeout ();
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _NETWORK */
