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
#include "condor_syscall_mode.h"


extern "C" {

void
limit( int resource, rlim_t new_limit )
{
	int		scm;
	struct	rlimit current = {0,0};
	struct	rlimit desired = {0,0};

	scm = SetSyscalls( SYS_LOCAL | SYS_RECORDED );

	/* Find out current limit for this resource */
	if( getrlimit(resource, &current) < 0 ) {
		EXCEPT( "getrlimit(%d): errno: %d(%s)", 
					resource, errno, strerror(errno));
	}

	/* Make a new rlimit structure that holds what I desire to be true */
	desired.rlim_cur = new_limit;
	desired.rlim_max = current.rlim_max;

	/* Don't try to exceed the hard max as supplied by getrlimit() */
	if( desired.rlim_cur > desired.rlim_max ) {
		desired.rlim_cur = desired.rlim_max;
	}

	/* Set the new limit */
	if( setrlimit(resource, &desired) < 0 ) {
		if (errno == EPERM ) {
		/* Under tru64, this setrlimit() fails oddly in that we
			are following the API restrictions exactly(not
			adjusting the maximum up as non-root, and not
			asking for more soft limit than hard limit),
			but still the kernel refuses to honor the
			change we wish to make and responds with a
			nonsensical EPERM failure. So we'll see if we
			can do something reasonable before giving up
			and not setting it at all. This code will be
			kept common for all architectures in the hopes
			that it will handle failures of this kind
			everywhere. We're going to try and bound the
			resource limit to UINT_MAX if we are able. */

		dprintf( D_ALWAYS, 
			"Unexpected permissions failure in setting limits for resource "
			"kind %d. setrlimit(%d, new = [rlim_cur = %lu, rlim_max = %lu]) : "
			"old = [rlim_cur = %lu, rlim_max = %lu], errno: %d(%s). Attempting "
			"workaround.\n",
			resource, 
			resource,
			(unsigned long)desired.rlim_cur, 
			(unsigned long)desired.rlim_max, 
			(unsigned long)current.rlim_cur, 
			(unsigned long)current.rlim_max,
			errno, 
			strerror(errno) );

			/* bound the resource limit to UINT_MAX, if I can */
			if (desired.rlim_cur > UINT_MAX && 
				current.rlim_max >= UINT_MAX)
			{
				desired.rlim_cur = UINT_MAX;
				if (setrlimit(resource, &desired) < 0) {
					dprintf( D_ALWAYS, "Workaround failed with error %d(%s). "
						"Not adjusting limit for resource %d\n", 
						errno, strerror(errno), resource);

				} else {
					dprintf( D_ALWAYS, "Workaround enabled. Limit for "
						"resource %d is this: "
						"new = [rlim_cur = %lu, rlim_max = %lu]\n",
						resource, desired.rlim_cur, desired.rlim_max);
				}
			} else {
				dprintf( D_ALWAYS, "Workaround not applicable, no limit "
					"enforcement for resource %d.\n", resource);
			}

		} else {
			/* if this is something other than a permission problem
				given the legal use of the setrlimit() interface */

			EXCEPT( "Unexpected failure in setting limits for resource "
			"kind %d. setrlimit(%d, new = [rlim_cur = %lu, rlim_max = %lu]) : "
			"old = [rlim_cur = %lu, rlim_max = %lu], errno: %d(%s). \n",
			resource, 
			resource,
			(unsigned long)desired.rlim_cur, 
			(unsigned long)desired.rlim_max, 
			(unsigned long)current.rlim_cur, 
			(unsigned long)current.rlim_max,
			errno, 
			strerror(errno) );
		}
	}

	(void)SetSyscalls( scm );
}

} /* extern "C" */
