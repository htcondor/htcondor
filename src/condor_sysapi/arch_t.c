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
#include "sysapi.h"
#include "sysapi_externs.h"
#include "stdio.h"
#include "string.h"

int arch_test(int trials)
{
	const char *foo,  *foo2;
	const char *bar, *bar2;
	time_t	t0, t1;
	int		return_val = 0;
	int		bad_string = 0;
	int i;

	foo = sysapi_condor_arch();
	dprintf(D_ALWAYS, "SysAPI: sysapi_condor_arch -> %s\n", foo);
	bar = sysapi_uname_arch();
	dprintf(D_ALWAYS, "SysAPI: sysapi_uname_arch -> %s\n", bar);

	if (foo == NULL || strlen(foo) <= 0 || (strcmp(foo, "UNKNOWN")==0)) {
		return_val = return_val || 1;
		dprintf(D_ALWAYS, "SysAPI: ERROR! sysapi_condor_arch returned a bad or unknown string");
	}
	if (bar == NULL || strlen(bar) <= 0 || (strcmp(bar, "UNKNOWN")==0)) {
		return_val = return_val || 1;
		dprintf(D_ALWAYS, "SysAPI: ERROR! sysapi_uname_arch returned a bad string");
	}


	dprintf(D_ALWAYS, "SysAPI: Testing arch %d times for consistency.\n", trials);
	for (i=0; i<trials && return_val==0; i++) {
		if (i%2 == 0) {
			foo2 = sysapi_condor_arch();
			bar2 = sysapi_uname_arch();
		} else{
			foo = sysapi_condor_arch();
			bar = sysapi_uname_arch();
		}

		if (strcmp(foo, foo2) != 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR: sysapi_condor_arch() returned a different value!\n");
			return_val = return_val || 1;
		}
		if (strcmp(bar, bar2) != 0) {
			dprintf(D_ALWAYS, "SysAPI: ERROR: sysapi_condor_arch_raw() returned a different value!\n");
			return_val = return_val || 1;
		}
	}

	foo = sysapi_condor_arch();
	dprintf(D_ALWAYS, "SysAPI: sysapi_condor_arch -> %s\n", foo);
	bar = sysapi_uname_arch();
	dprintf(D_ALWAYS, "SysAPI: sysapi_uname_arch -> %s\n", bar);

	return return_val;
}
