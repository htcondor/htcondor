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

 

#include "condor_common.h"

#if !defined(WIN32)
#include <sys/types.h>
#include <sys/param.h>

#if defined(SUNOS41) || defined(ULTRIX42) || defined(ULTRIX43)
#include <sys/time.h>
#endif

#include <sys/resource.h>
#endif

#include <stdio.h>

#include "proc.h"

char	*param();

char *
gen_ckpt_name( directory, cluster, proc, subproc )
char	*directory;
int		cluster;
int		proc;
int		subproc;
{
	static char	answer[ MAXPATHLEN ];

	if( proc == ICKPT ) {
		if( directory && directory[0] ) {
			(void)sprintf( answer, "%s%ccluster%d.ickpt.subproc%d",
						directory, DIR_DELIM_CHAR, cluster, subproc );
		} else {
			(void)sprintf( answer, "cluster%d.ickpt.subproc%d",
						cluster, subproc );
		}
	} else {
		if( directory && directory[0] ) {
			(void)sprintf( answer, "%s%ccluster%d.proc%d.subproc%d",
						directory, DIR_DELIM_CHAR, cluster, proc, subproc );
		} else {
			(void)sprintf( answer, "cluster%d.proc%d.subproc%d",
						cluster, proc, subproc );
		}
	}
	return answer;
}

char *
gen_exec_name( cluster, proc, subproc )
int		cluster;
int		proc;
int		subproc;
{
	static char	answer[ MAXPATHLEN ];

	(void)sprintf( answer, "condor_exec%d.%d.%d", cluster, proc, subproc );
	return answer;
}
