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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include "condor_debug.h"

int calc_virt_memory();
int ctok(int clicks);

/*
 *  DEFAULT_SWAPSPACE
 *
 *  This constant is used when we can't get the available swap space through
 *  other means.  -- Ajitk
 */

#define DEFAULT_SWAPSPACE       100000 /* ..dhaval 7/20 */

/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/
int
calc_virt_memory()
{
	struct anoninfo 	ai;
	int avail;

	memset( &ai, 0, sizeof(ai) );
	if( swapctl(SC_AINFO, &ai) >= 0 ) {
		avail = ctok( ai.ani_max - ai.ani_resv );
		return avail;
	} else {
		dprintf( D_FULLDEBUG, "swapctl call failed, errno = %d\n", errno );
		return DEFAULT_SWAPSPACE;
	}
}

int
ctok(int clicks)
{
	static int factor = -1;
	if( factor == -1 ) {
		factor = sysconf(_SC_PAGESIZE) >> 10;
	}
	return( clicks*factor );
}

