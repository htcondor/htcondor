/* 
** Copyright 1992 by the Condor Design Team
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
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

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
			(void)sprintf( answer, "%s/cluster%d.ickpt.subproc%d",
						directory, cluster, subproc );
		} else {
			(void)sprintf( answer, "cluster%d.ickpt.subproc%d",
						cluster, subproc );
		}
	} else {
		if( directory && directory[0] ) {
			(void)sprintf( answer, "%s/cluster%d.proc%d.subproc%d",
						directory, cluster, proc, subproc );
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
