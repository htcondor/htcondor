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
#include "limit.h"
#include "sysapi.h"

#define SLOP 50

#if defined( LINUX )
#define MEG (1024 * 1024)

void
sysapi_set_resource_limits(int stack_size)
{
	rlim_t lim;
	if(stack_size == 0) {
		stack_size = (int) RLIM_INFINITY;
	}
	long long free_blocks = sysapi_disk_space( "." );
	long long core_lim = (free_blocks - SLOP) * 1024;

	//PRAGMA_REMIND("FIXME: disk_space truncation to INT_MAX here")
	if( core_lim > INT_MAX ) {
		lim = INT_MAX;
	} else {
		lim = (int) core_lim;
	}
	limit( RLIMIT_CORE, lim, CONDOR_SOFT_LIMIT, "max core size" );
	limit( RLIMIT_CPU, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max cpu time" );
	limit( RLIMIT_FSIZE, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max file size" );
	limit( RLIMIT_DATA, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max data size" );
	limit( RLIMIT_STACK, stack_size, CONDOR_SOFT_LIMIT, "max stack size" );
	dprintf( D_ALWAYS, "Done setting resource limits\n" );
}

#elif defined( WIN32 ) || defined( Darwin ) || defined( CONDOR_FREEBSD )

void
sysapi_set_resource_limits(int)
{
		/* Not yet implemented on these platforms */
	dprintf( D_ALWAYS, "Setting resource limits not implemented!\n" );
}

#else

#error DO NOT KNOW HOW TO SET RESOURCE LIMITS ON THIS PLATFORM

#endif
