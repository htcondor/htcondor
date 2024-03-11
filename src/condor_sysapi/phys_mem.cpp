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
#include <cstdio>
#include <algorithm>


// Assuming a cgroup-style file with an integer
// or a string that we don't care about in it,
// return the 64 bit int or 0 if it is a string.
int64_t file_size_contents(const char *filename) {
	int64_t s = 0;
	FILE *f = fopen(filename, "r");

	if (f != nullptr) {
		std::ignore = fscanf(f, "%zd", &s);
		fclose(f);
	}

	// Some cgroup set memory max as 2 ^ 63 to 
	// indicate infinity.  Set to zero
	if (s > (1ll << 60)) {
		s = 0;
	}
	return s;
}

int64_t cgroup_current_memory_limit() {
	// Note all these files should be readable
	// by self without boosting privileges.
	FILE *f = fopen("/proc/self/cgroup", "r");
	if (f == nullptr) {
		return 0;
	}

	char line[512];
	while (fgets(line, 511, f) != nullptr) {
		std::string l = line;
		size_t first_colon = l.find_first_of(':');
		size_t second_colon = l.find_first_of(':', first_colon + 1);
		if (first_colon == (second_colon - 1)) {
			// cgroup v2
			int64_t s = 0;
			std::string cgroup = l.substr(second_colon + 1, l.size() - second_colon - 2);
			std::string cgroup_hi = std::string("/sys/fs/cgroup/") + cgroup + "/memory.high";
			s = file_size_contents(cgroup_hi.c_str());
			if (s == 0) {
				std::string cgroup_max = std::string("/sys/fs/cgroup/") + cgroup + "/memory.max";
				s = file_size_contents(cgroup_max.c_str());
			}
			fclose(f);
			return s;
		} 

		std::string controller = l.substr(first_colon + 1, second_colon - first_colon - 1 );
		// cgroup v1
		if (controller == "memory") {
			int64_t s = 0;
			std::string cgroup = l.substr(second_colon + 1, l.size() - second_colon - 2);
			std::string cgroup_limit = std::string("/sys/fs/cgroup/memory/") + cgroup + "/memory.limit_in_bytes";
			s = file_size_contents(cgroup_limit.c_str());
			fclose(f);
			return s;
		}
	}
	fclose(f);
	return 0;
}

int 
sysapi_phys_memory_raw_no_param(void) 
{	

	int64_t bytes = 0;
	int64_t  megs = 0;

	/* in bytes */
	bytes = 
		(int64_t) sysconf(_SC_PHYS_PAGES) * (int64_t) sysconf(_SC_PAGESIZE);

	int64_t cgroup_limit = cgroup_current_memory_limit();
	if (cgroup_limit > 0) {
		// If the limit is higher than detected memory, clamp to detected
		bytes = std::min(bytes, cgroup_limit);
	}
	/* convert it to Megabytes */
	megs = bytes / (1024ll * 1024ll);

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
