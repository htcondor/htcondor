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
#include "condor_syscall_mode.h"


extern "C" {

void
limit( int resource, rlim_t new_limit )
{
	int		scm;
	struct	rlimit lim;

	scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );

		/* Find out current limits */
	if( getrlimit(resource,&lim) < 0 ) {
		EXCEPT( "getrlimit(%d,0x%x)", resource, &lim );
	}

		/* Don't try to exceed the max */
	if( new_limit > lim.rlim_max ) {
		new_limit = lim.rlim_max;
	}

		/* Set the new limit */
	lim.rlim_cur = new_limit;
	if( setrlimit(resource,&lim) < 0 ) {
		EXCEPT( "setrlimit(%d,0x%x), errno: %d", resource, &lim, errno );
	}

	(void)SetSyscalls( scm );
}

} /* extern "C" */
