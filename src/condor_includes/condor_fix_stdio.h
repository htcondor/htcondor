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
#ifndef FIX_STDIO_H
#define FIX_STDIO_H

#if defined(LINUX) && defined(GLIBC)
#define dprintf _hide_dprintf
#endif

#include <stdio.h>

#if defined(LINUX) && defined(GLIBC)
#undef dprintf
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/*
  For some reason the stdio.h on IRIX 5.3 fails to provide prototypes
  for popen() and pclose() if _POSIX_SOURCE is defined.
*/
#if defined(IRIX53)
	FILE *popen (const char *command, const char *type);
	int pclose(FILE *stream);
#endif /* IRIX53 */

#if defined(__cplusplus)
}
#endif


#endif
