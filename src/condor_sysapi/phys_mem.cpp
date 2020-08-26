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
 
#if defined(LINUX)
#include <stdio.h>

int 
sysapi_phys_memory_raw_no_param(void) 
{	

	double bytes;
	double megs;

	/* in bytes */
	bytes = 
		(double)sysconf(_SC_PHYS_PAGES) * (double)sysconf(_SC_PAGESIZE);

	/* convert it to Megabytes */
	megs = bytes / (1024.0*1024.0);

	if (megs > INT_MAX) {
		return INT_MAX;
	}

	return (int)megs;
}

#elif defined(WIN32)

int
sysapi_phys_memory_raw_no_param(void)
{
	MEMORYSTATUSEX statex;		
	
	statex.dwLength = sizeof(statex);
	
	GlobalMemoryStatusEx(&statex);
	
	return (int)(statex.ullTotalPhys/(1024*1024));
}

// See GNATS 529. This code should now detect >= 2Gigs properly.
#elif defined(Darwin) || defined(CONDOR_FREEBSD)
#include <sys/sysctl.h>
int
sysapi_phys_memory_raw_no_param(void)
{
	int megs;
	uint64_t mem = 0;
	size_t len = sizeof(mem);

#ifdef Darwin
	if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) < 0) 
#elif defined(CONDOR_FREEBSD)
	if (sysctlbyname("hw.physmem", &mem, &len, NULL, 0) < 0) 
#endif
	{
        dprintf(D_ALWAYS, 
			"sysapi_phys_memory_raw(): sysctlbyname(\"hw.memsize\") "
			"failed: %d(%s)\n",
			errno, strerror(errno));
		return -1;
	}

	megs = mem / (1024 * 1024);

	return megs;
}
#else
#error "sysapi.h: Please define a sysapi_phys_memory_raw() for this platform!"
#endif


int sysapi_phys_memory_raw(void)
{
	sysapi_internal_reconfig();
	return sysapi_phys_memory_raw_no_param();
}

/* This is the cooked version of sysapi_phys_memory_raw(). Where as the raw
	function gives you the raw number, this function can process the number
	for you for some reason and return that instead.
*/
int
sysapi_phys_memory(void)
{
	int mem;
	sysapi_internal_reconfig();
	mem = _sysapi_memory;
	if (!_sysapi_memory) {
		mem = sysapi_phys_memory_raw();
	}
	if (mem < 0) return mem;
	mem -= _sysapi_reserve_memory;
	if (mem < 0) return 0;
	return mem;
}
