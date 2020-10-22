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

#if defined(LINUX)
#  include <sys/sysinfo.h>
#endif

/*
** Try to determine the swap space available on our own machine.  The answer
** is in kilobytes.
*/

/* If for some reason, the call fails, then -1 is returned */

#if defined(WIN32)

int
sysapi_swap_space_raw()
{
	MEMORYSTATUSEX status;
	sysapi_internal_reconfig();
	GlobalMemoryStatusEx(&status);
	if (status.ullAvailPageFile / 1024 > INT_MAX)
		return INT_MAX;
	return (int) (status.ullAvailPageFile / 1024);
		
}

#elif defined(LINUX)

int
sysapi_swap_space_raw()
{
	struct sysinfo si;
	double free_swap;

	sysapi_internal_reconfig();

	if (sysinfo(&si) == -1) {
		dprintf(D_ALWAYS, 
			"sysapi_swap_space_raw(): error: sysinfo(2) failed: %d(%s)",
			errno, strerror(errno));
		return -1;
	}

	/* On Linux before 2.3.23, mem_unit was not present
		and the pad region of space in this structure appears to
		have been occupying was set to 0; units are bytes */
	if (si.mem_unit == 0) {
		si.mem_unit = 1;
	}

	/* in B */
	free_swap = (double)si.freeswap * (double)si.mem_unit +
			(double)si.totalram * (double)si.mem_unit;

	/* in KB */
	free_swap /= 1024.0;

	if (free_swap > INT_MAX)
	{
		return INT_MAX;
	}

	return (int)free_swap;
}

#elif defined(Darwin) || defined(CONDOR_FREEBSD)
#include <sys/sysctl.h>
int
sysapi_swap_space_raw() {
        int mib[2];
        unsigned int usermem;
        size_t len;   
        mib[0] = CTL_HW;     
        mib[1] = HW_USERMEM;        
        len = sizeof(usermem);   
        sysctl(mib, 2, &usermem, &len, NULL, 0);   
	return usermem / 1024;
}

#else

#error "Please define: sysapi_swap_space_raw()"

#endif /* !defined(WIN32) */


/* the cooked version of the function */
int
sysapi_swap_space(void)
{
	sysapi_internal_reconfig();

	return sysapi_swap_space_raw();
}



