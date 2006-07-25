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
