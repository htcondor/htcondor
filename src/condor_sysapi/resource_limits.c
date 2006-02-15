/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_constants.h"
#include "limit.h"

#define SLOP 50


#if defined( OSF1 ) || defined( LINUX )

void
sysapi_set_resource_limits()
{
    rlim_t lim;
    int free_blocks = sysapi_disk_space( "." );
	unsigned long core_lim = (free_blocks - SLOP) * 1024;
	if( core_lim > MAXINT ) {
		lim = MAXINT;
	} else {
		lim = (int) core_lim;
	}

    limit( RLIMIT_CORE, lim );
	limit( RLIMIT_CPU, RLIM_INFINITY );
	limit( RLIMIT_FSIZE, RLIM_INFINITY );
	limit( RLIMIT_DATA, RLIM_INFINITY );
	limit( RLIMIT_STACK, RLIM_INFINITY );

	dprintf( D_ALWAYS, "Done setting resource limits\n" );
}

#elif defined( Solaris ) 

#define MEG (1024 * 1024)

void
sysapi_set_resource_limits()
{
	limit( RLIMIT_CPU, RLIM_INFINITY );
	limit( RLIMIT_FSIZE, RLIM_INFINITY );
	limit( RLIMIT_DATA, RLIM_INFINITY );
	limit( RLIMIT_VMEM, RLIM_INFINITY ); 
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
	limit( RLIMIT_STACK, 8 * MEG );

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
	limit( RLIMIT_CORE, RLIM_INFINITY );

	dprintf( D_ALWAYS, "Done setting resource limits\n" );
}

#elif defined( HPUX ) 

void
sysapi_set_resource_limits()
{
		/* These platforms do not support limit() */
	dprintf( D_ALWAYS, "Setting resource limits not supported!\n" );
}

#elif defined( IRIX )  || defined( WIN32 ) || defined( AIX ) || defined( Darwin )

void
sysapi_set_resource_limits()
{
		/* Not yet implemented on these platforms */
	dprintf( D_ALWAYS, "Setting resource limits not implemented!\n" );
}

#else

#error DO NOT KNOW HOW TO SET RESOURCE LIMITS ON THIS PLATFORM

#endif
