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
int do_connect ( const char *host, const char *service, u_short port );
int tcp_accept_timeout( int ConnectionSock, struct sockaddr *sinful, 
						int *len, int timeout );
#else
int do_connect ();
int tcp_accept_timeout ();
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _NETWORK */
