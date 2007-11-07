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


#include "condor_common.h" 
#include "condor_debug.h"
#include "util_lib_proto.h"



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
