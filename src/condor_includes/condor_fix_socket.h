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
#ifndef FIX_SOCKET_H
#define FIX_SOCKET_H

#if !defined(WIN32)
#include <sys/socket.h>
#endif


/*
  For some reason the g++ includes on Ultrix 4.3 fail to provide
  these prototypes, even though the Ultrix 4.2 includes did...
  Same goes for OSF1  JCP
  Same goes for SUNOS - mike
*/
#if defined(ULTRIX43) || defined(OSF1) || defined(SUNOS41)

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)
	int getpeername( int, struct sockaddr *, int * );
	int socket( int, int, int );
	int connect( int, const struct sockaddr *, int );
#else
	int getpeername();
	int socket();
	int connect();
#endif

#if defined(__cplusplus)
}
#endif

#endif	/* ULTRIX43 */

#endif
