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

 

#include <sys/types.h>


#if defined(AIX31) || defined(AIX32) || defined(OSF1)
#include <string.h>
#endif

#if 0
#if !defined(AIX31) && !defined(AIX32)  && !defined(HPUX8) && !defined(OSF1)
char	*calloc();
char	*ctime();
char	*getwd();
char	*malloc();
char	*param();
char	*realloc();
char	*strcat();
char	*strcpy();
char	*strncpy();
char	*sprintf();
#endif
#endif

#if !defined( OSF1 ) && !defined(WIN32) && !( defined(LINUX) && defined(GLIBC) ) && !defined(Solaris)
#ifndef htons
u_short	htons();
#endif htons

#ifndef ntohs
u_short	ntohs();
#endif ntohs

#ifndef htonl
u_long	htonl();
#endif htonl

#ifndef ntohl
u_long	ntohl();
#endif ntohl

#ifndef time
time_t	time();
#endif time
#endif	/* !OSF1 && !WIN32 */
