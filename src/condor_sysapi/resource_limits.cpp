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
#include "condor_constants.h"
#include "limit.h"

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

	PRAGMA_REMIND("FIXME: disk_space truncation to INT_MAX here")
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

#elif defined( Solaris ) 

#define MEG (1024 * 1024)

void
sysapi_set_resource_limits(int)
{
	limit( RLIMIT_CPU, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max cpu time" );
	limit( RLIMIT_FSIZE, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max file size" );
	limit( RLIMIT_DATA, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max data size" );
	limit( RLIMIT_VMEM, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max vmem size" ); 
	/* VMEM seems to be analogous to RSS on Sun ..dhaval 7/2 */

/*
	The stack segment in the sun core file always has a virtual size which
	is the same as the stack size limit in force at the time of the dump.
	This is implemented as a file "hole" so that the actual amount of space
	taken up on disk is only that required to hold the valid part of the
	stack.  Thus the virtual size of the core file is generally much larger
	than the actual size.  This has some unfortunate interactions with
	process limits, see comments below regarding specific limits.
*/

/*
	We would like to unlimit the stack size so that the process can have
	complete flexibility in how it uses its virtual memory resources.
	Unfortunately if we do that, the virtual size of the stack segment
	in the core file will be 2 gigabytes.  For some reason the operating
	system won't produce such a core, even though there is plenty of disk
	to accomodate the actual size of the file.  We therefore set the
	stack limit to a default of 8 megabytes.
*/
	limit( RLIMIT_STACK, 8 * MEG, CONDOR_SOFT_LIMIT, "max stack size" );

/*
	We would like to limit the size of the coredump to something less than
	the actual free disk in the filesystem.  That way we could ensure
	that the system would not try to produce a core that won't fit, overflow
	the file system, and produce ugly messages on the console.  Unfortunately
	if seems that the coredump size limit controls the virtual rather
	than the actual size of the core.  That means if there is 6 Meg
	of free disk and we set the limit accordingly, we would never get
	a core at all, even if the actual size of the core would only be
	2 meg!!  For this reason, we don't limit the core file size on
	Suns.
*/
	limit( RLIMIT_CORE, RLIM_INFINITY, CONDOR_SOFT_LIMIT, "max core size" );

	dprintf( D_ALWAYS, "Done setting resource limits\n" );
}

#elif defined( HPUX ) 

void
sysapi_set_resource_limits(int)
{
		/* These platforms do not support limit() */
	dprintf( D_ALWAYS, "Setting resource limits not supported!\n" );
}

#elif defined( WIN32 ) || defined( AIX ) || defined( Darwin ) || defined( CONDOR_FREEBSD )

void
sysapi_set_resource_limits(int)
{
		/* Not yet implemented on these platforms */
	dprintf( D_ALWAYS, "Setting resource limits not implemented!\n" );
}

#else

#error DO NOT KNOW HOW TO SET RESOURCE LIMITS ON THIS PLATFORM

#endif
