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
