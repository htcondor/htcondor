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
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

/*
** Add ru2 rusage struct to ru1 rusage struct.
*/
update_rusage( ru1, ru2 )
register struct rusage *ru1, *ru2;
{
	ru1->ru_utime.tv_usec += ru2->ru_utime.tv_usec;
	if( ru1->ru_utime.tv_usec >= 1000000 ) {
		ru1->ru_utime.tv_usec -= 1000000;
		ru1->ru_utime.tv_sec += 1;
	}
	ru1->ru_utime.tv_sec += ru2->ru_utime.tv_sec;

	ru1->ru_stime.tv_usec += ru2->ru_stime.tv_usec;
	if( ru1->ru_stime.tv_usec >= 1000000 ) {
		ru1->ru_stime.tv_usec -= 1000000;
		ru1->ru_stime.tv_sec += 1;
	}
	ru1->ru_stime.tv_sec += ru2->ru_stime.tv_sec;

	if( ru2->ru_maxrss > ru1->ru_maxrss ) {
		ru1->ru_maxrss = ru2->ru_maxrss;
	}
	if( ru2->ru_ixrss > ru1->ru_ixrss ) {
		ru1->ru_ixrss = ru2->ru_ixrss;
	}
	if( ru2->ru_idrss > ru1->ru_idrss ) {
		ru1->ru_idrss = ru2->ru_idrss;
	}
	if( ru2->ru_isrss > ru1->ru_isrss ) {
		ru1->ru_isrss = ru2->ru_isrss;
	}
	ru1->ru_minflt += ru2->ru_minflt;
	ru1->ru_majflt += ru2->ru_majflt;
	ru1->ru_nswap += ru2->ru_nswap;
	ru1->ru_inblock += ru2->ru_inblock;
	ru1->ru_oublock += ru2->ru_oublock;
	ru1->ru_msgsnd += ru2->ru_msgsnd;
	ru1->ru_msgrcv += ru2->ru_msgrcv;
	ru1->ru_nsignals += ru2->ru_nsignals;
	ru1->ru_nvcsw += ru2->ru_nvcsw;
	ru1->ru_nivcsw += ru2->ru_nivcsw;
}
