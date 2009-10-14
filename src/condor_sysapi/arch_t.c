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
	foo = sysapi_opsys();
	dprintf(D_ALWAYS, "SysAPI: sysapi_opsys -> %s\n", foo);

	return return_val;
}
