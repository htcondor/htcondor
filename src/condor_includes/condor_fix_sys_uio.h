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
#ifndef CONDOR_FIX_SYS_UIO_H
#define CONDOR_FIX_SYS_UIO_H

#if defined(LINUX) && defined(GLIBC)
/* glibc defines the 3rd arg of readv and writev to be int, even
   though the man page (and our code) says it should be size_t. 
   -Derek Wright 4/17/98 */
#define readv __hide_readv
#define writev __hide_writev
#endif /* LINUX && GLIBC */

#if !defined(WIN32)
#include <sys/uio.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(LINUX) && defined(GLIBC)
#undef readv
#undef writev
int readv(int, const struct iovec *, size_t);
int writev(int, const struct iovec *, size_t);
#endif /* LINUX && GLIBC */

#if defined(__cplusplus)
}
#endif

#endif /* CONDOR_FIX_SYS_UIO_H */
