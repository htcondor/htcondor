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
#include "sysapi.h"
#include "sysapi_externs.h"
#include "stdio.h"
#include "string.h"

int arch_test(int trials)
{
	const char *foo = NULL, *foo2 = NULL;
	const char *bar = NULL, *bar2 = NULL;
	int		return_val = 0;
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
