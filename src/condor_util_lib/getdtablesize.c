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

#ifdef OPEN_MAX
static int open_max = OPEN_MAX;
#else
static int open_max = 0;
#endif
#define OPEN_MAX_GUESS 256

/*
** Compatibility routine for systems which don't have this call.
*/
int
getdtablesize()
{
	if( open_max == 0 ) {	/* first time */
		errno = 0;
		if( (open_max = sysconf(_SC_OPEN_MAX)) < 0 ) {
			if( errno == 0 ) {
					/* _SC_OPEN_MAX is indeterminate */
				open_max = OPEN_MAX_GUESS;
			} else {
					/* Error in sysconf */
			}
		}	
	}
	return open_max;
}
