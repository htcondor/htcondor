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
