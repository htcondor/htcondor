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

extern "C" {

void
limit( int resource, rlim_t new_limit, int kind, char const *resource_str )
{
	struct	rlimit current = {0,0};
	struct	rlimit desired = {0,0};
	char const *kind_str = "";

	/* Find out current limit for this resource */
	if( getrlimit(resource, &current) < 0 ) {
		EXCEPT( "getrlimit(%d (%s)): errno: %d(%s)", 
					resource, resource_str, errno, strerror(errno));
	}

	switch(kind) {
		case CONDOR_SOFT_LIMIT:
			/* Make a new rlimit structure that holds what I desire to be 
				true */
			desired.rlim_cur = new_limit;
			desired.rlim_max = current.rlim_max;
			kind_str = "soft";

			/* Don't try to exceed the hard max as supplied by getrlimit() */
			if( desired.rlim_cur > desired.rlim_max ) {
				desired.rlim_cur = desired.rlim_max;
			}

			break;

		case CONDOR_HARD_LIMIT:
			desired.rlim_cur = new_limit;
			desired.rlim_max = new_limit;
			kind_str = "hard";

			if ((desired.rlim_max > current.rlim_max) && (getuid() != (pid_t)0))
			{
				/* Due to the fact that this code
					path will happen a lot as a non
					root user due to the the behavior
					in Create_Process where we always
					try to set the core hard limit to
					the maximum of size_t by default,
					we will by default ALWAYS ask
					for a limit which may be larger
					than the current hard max of
					this limit. Especially in the
					case of a personal Condor this
					will result in whatever message
					being written here being printed
					out *all the time*. So, instead
					of doing a dprintf for SNAFU
					behavior, we silently truncate
					the asked for limit the to
					current hard max if it is greater.
				*/

				desired = current;

				desired.rlim_cur = desired.rlim_max;
			}

			break;

		case CONDOR_REQUIRED_LIMIT:
			desired.rlim_cur = new_limit;
			desired.rlim_max = current.rlim_max;
			kind_str = "required";

				// only raise the hard limit if necessary
			if( desired.rlim_cur > desired.rlim_max ) {
				desired.rlim_max = desired.rlim_cur;
			}

			break;

		default:
			EXCEPT("do_limit() unknown limit enforcment policy. Programmer "
				"Error.");
			break;
	}

	/* Set the new limit */
	if( setrlimit(resource, &desired) < 0 ) {
		if (errno == EPERM && kind != CONDOR_REQUIRED_LIMIT ) {
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
			"Unexpected permissions failure in setting %s limit for %s"
			"setrlimit(%d, new = [rlim_cur = %lu, rlim_max = %lu]) : "
			"old = [rlim_cur = %lu, rlim_max = %lu], errno: %d(%s). Attempting "
			"workaround.\n",
			kind_str,
			resource_str, 
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
						"Not adjusting %s limit for %s\n", 
						errno, strerror(errno), kind_str, resource_str);

				} else {
					dprintf( D_ALWAYS, "Workaround enabled. The %s limit for "
						"%s is this: "
						"new = [rlim_cur = %lu, rlim_max = %lu]\n",
						kind_str,resource_str,
						(unsigned long)desired.rlim_cur,
						(unsigned long)desired.rlim_max);
				}
			} else {
				dprintf( D_ALWAYS, "Workaround not applicable, no %s limit "
					"enforcement for %s.\n", kind_str, resource_str);
			}

		} else {
			/* if this is something other than a permission problem
				given the legal use of the setrlimit() interface */

			dprintf(D_ALWAYS, "Failed to set %s limits for %s. "
			"setrlimit(%d, new = [rlim_cur = %lu, rlim_max = %lu]) : "
			"old = [rlim_cur = %lu, rlim_max = %lu], errno: %d(%s). \n",
			kind_str,
			resource_str,
			resource,
			(unsigned long)desired.rlim_cur, 
			(unsigned long)desired.rlim_max, 
			(unsigned long)current.rlim_cur, 
			(unsigned long)current.rlim_max,
			errno, 
			strerror(errno) );
		}
	}
}

} /* extern "C" */
