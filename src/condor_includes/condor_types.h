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
