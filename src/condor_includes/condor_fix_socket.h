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


#if defined(OSF1) && defined(_XOPEN_SOURCE_EXTENDED)
/* If _XOPEN_SOURCE_EXTENDED is defined on dux4.0 when you include
   sys/socket.h "accept" gets #define'd, which screws up condor_io,
   since we have methods called "accept" in there.  -Derek 3/26/98 */
#	define CONDOR_XOPEN_SOURCE_EXTENDED
#	undef _XOPEN_SOURCE_EXTENDED
#endif

#if defined(LINUX) && defined(GLIBC) 
/* With the new glibc, there are a bunch of changes in
   prototypes... things that the man pages say are ints that the
   headers say are unsigned, things that are supposed to be const are
   not, etc, etc.  So, we hide the broken prototypes and define our
   own. -Derek 4/17/98 */
#define accept hide_accept
#define getpeername hide_getpeername
#define getsockname hide_getsockname
#define setsockopt hide_setsockopt
#define recvfrom hide_recvfrom
#endif /* LINUX && GLIBC */

#if !defined(WIN32)
#	include <sys/socket.h>
#endif

#if defined(LINUX) && defined(GLIBC) 
#undef accept
#undef getpeername
#undef getsockname
#undef setsockopt
#undef recvfrom
#ifdef __cplusplus
extern "C" {
#endif
int accept(int, struct sockaddr *, int *);
int getpeername(int, struct sockaddr *, int *);
int getsockname(int, struct sockaddr *, int *);
int setsockopt(int, int, int, const void *, int);
int recvfrom(int, void *, int, unsigned int, struct sockaddr *, int *);
#ifdef __cplusplus
}
#endif
#endif /* LINUX && GLIBC */


#if defined(OSF1) && defined(CONDOR_XOPEN_SOURCE_EXTENDED)
#	undef CONDOR_XOPEN_SOURCE_EXTENDED
#	define _XOPEN_SOURCE_EXTENDED
#endif


#endif /* FIX_SOCKET_H */
