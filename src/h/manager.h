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
