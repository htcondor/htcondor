/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"



/*
** Add ru2 rusage struct to ru1 rusage struct.
*/
void
update_rusage( register struct rusage *ru1, register struct rusage *ru2 )
{
	dprintf( D_FULLDEBUG, "Entering update_rusage()\n");
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
