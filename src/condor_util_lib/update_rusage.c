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
