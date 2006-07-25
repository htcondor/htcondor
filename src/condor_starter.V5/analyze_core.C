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



