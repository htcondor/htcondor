/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#ifndef CONDOR_TYPES_INCLUDED
#define CONDOR_TYPES_INCLUDED


#include "condor_expressions.h"

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

/* struct bucket definition removed -- duplicate of definition in config.h */

#if !defined(ULTRIX42) || defined(ULTRIX43)
struct fs_data_req {
	dev_t	dev;
	char	*devname;
	char	*path;
};
struct fs_data {
	struct fs_data_req fd_req;
};
#endif


#if !defined(RUSAGE_SELF)

#if defined(_POSIX_SOURCE)
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

typedef struct status_line {
	char	*name;
	int		run;
	int		tot;
	int		prio;
	char	*state;
	float	load_avg;
	int		kbd_idle;
	char	*arch;
	char	*op_sys;
} STATUS_LINE;


#endif
