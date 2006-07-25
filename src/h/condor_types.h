/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_TYPES_H
#define _CONDOR_TYPES_H

#include "condor_getmnt.h"

#if !defined(AIX31) && !defined(AIX32)
#define NFDS(x) (x)
#endif


#if !defined(LINUX)
struct qelem {
	struct qelem *q_forw;
	struct qelem *q_back;
	char	q_data[1];
};
#endif

#if !defined(RUSAGE_SELF)

#if defined(_POSIX_SOURCE) && !defined(WIN32)
#define SAVE_POSIX_DEF _POSIX_SOURCE
#define SAVE_POSIX_4DEF _POSIX_4SOURCE
#undef _POSIX_SOURCE
#undef _POSIX_4SOURCE
#include <sys/time.h>
#define _POSIX_SOURCE SAVE_POSIX_DEF
#define _POSIX_4SOURCE SAVE_POSIX_4DEF
#elif !defined(WIN32)
#include <sys/time.h>
#endif /* _POSIX_SOURCE */

#if !defined(WIN32)
#include <sys/resource.h>
#endif

#endif	/* RUSAGE_SELF */

/* XDR is not POSIX.1 conforming, and requires the following typedefs */
#if defined(_POSIX_SOURCE)
#if defined(__cplusplus) 
typedef unsigned int u_int;
typedef char * caddr_t;
#endif	/* __cplusplus */
#endif	/* _POSIX_SOURCE */

#define MALLOC_DEFINED_AS_VOID
#if defined(OSF1)
#define malloc hide_malloc
#endif
#if defined(OSF1)
#undef malloc
#define mem_alloc(bsize)        malloc(bsize)
#endif


#include "proc.h"
#include "condor_status.h"

#endif /* _CONDOR_TYPES_H */
