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


#if !defined(_MANAGER_H)
#define _MANAGER_H

#define MINUTE		60
#define POLICY_TIME atoi(param("POLICY_TIME", "120"))
#define TIMEOUT 15


#define FOREGROUND boolean( "MGR_DEBUG", "Foreground" )
#define TERM_LOG boolean( "MGR_DEBUG", "TermLog" )
#define MGR_LOG param( "MGR_LOG", "/no/mgr_log/specified" )

#if 0 /* now included in condor_types.h */
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

#ifdef COLLECTOR_STATS
#include "stats.h"
#endif

typedef struct mach_rec {
	struct mach_rec	*next;
	struct mach_rec	*prev;
	char			*name;
	struct in_addr	net_addr;
	short			net_addr_type;
	CONTEXT			*machine_context;
	int				time_stamp;
	int				prio;
	int				busy;
	STATUS_LINE		*line;
#ifdef COLLECTOR_STATS
	MACH_STATS		*stats;
#endif
} MACH_REC;

typedef struct prio_rec {
	char	*name;
	int		prio;
} PRIO_REC;

#define DUMMY	(MACH_REC *)0

#endif /* _MANAGER_H */
