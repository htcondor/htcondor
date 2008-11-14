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

int
core_is_valid( char *core_fn )
{
	struct stat buf;

	/* be cognizant of the file that we are checking isn't a
		symlink. If it is, then the user is most likely trying
		to perform an exploit to have Condor retrieve any file
		on the system. Condor must return the core file via root
		privledges because a user doesn't have search permissions
		to the sand box of the execute directory where the core
		file will end up */

	if (lstat(core_fn, &buf) == 0)
	{
		/* Hmm... Something is here, make sure it isn't a symlink */
		if ( ! S_ISLNK(buf.st_mode) )
		{
			/* looks like something real.  If it turns out the file is
				corrupt, then let the user figure it out when it they
				inspect it. This is no different than if the core file was
				corrupt to begin with and created outside of Condor 
				altogether. */
			return TRUE;
		}
	}

	/* If this file was a symlink or not present, mark it as not valid. */
	return FALSE;
}



