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


/* This function access a file with the access() interface, but with stat()
	semantics so that the access doesn't occur with the real uid, but the
	effective uid instead. */
int
access_euid(const char *path, int mode)
{
	FILE *f;
	struct stat buf;
	int already_stated = 0;

		/* clear errno before we begin, so we can ensure we set it to
		   the right thing.  if we return without touching it again,
		   we want it to be 0 (success).
		*/
	errno = 0;

	/* are the arguments valid? */
	if (path == NULL || (mode & ~(R_OK|W_OK|X_OK|F_OK)) != 0) {
		errno = EINVAL;
		return -1;
	}

	/* here we are going to sanity check the constants to make sure that we
		use them correctly if they happen to be a zero value(WHICH USUALLY F_OK
		IS). If it is zero, then by the semantics of the bitwise or of a zeroed
		value against a number, which is idempotent, it automatically needs to
		happen. */

	if ((F_OK == 0) || (mode & F_OK)) {
		if (stat(path, &buf) < 0) {
			if( ! errno ) {
					/* evil!  stat() failed but there's no errno! */
				dprintf( D_ALWAYS, "WARNING: stat() failed, but "
						 "errno is still 0!  Beware of misleading "
						 "error messages\n" );
			}
			return -1;
		}
		already_stated = 1;
	}

	if ((R_OK == 0) || (mode & R_OK)) {
		f = fopen(path, "r");
		if (f == NULL) {
			if( ! errno ) {
				dprintf( D_ALWAYS, "WARNING: fopen() failed, but "
						 "errno is still 0!  Beware of misleading "
						 "error messages\n" );
			}
			return -1;
		}
		fclose(f);
	}

	if ((W_OK == 0) || (mode & W_OK)) {
		f = fopen(path, "a"); /* don't truncate the file! */
		if (f == NULL) {
			if( ! errno ) {
				dprintf( D_ALWAYS, "WARNING: fopen() failed, but "
						 "errno is still 0!  Beware of misleading "
						 "error messages\n" );
			}
			return -1;
		}
		fclose(f);
	}

	if ((X_OK == 0) || (mode & X_OK)) {
		if (!already_stated) {
			/* stats are expensive, so only do it if I have to */
			if (stat(path, &buf) < 0) {
				if( ! errno ) {
					dprintf( D_ALWAYS, "WARNING: stat() failed, but "
							 "errno is still 0!  Beware of misleading "
							 "error messages\n" );
				}
				return -1;
			}
		}
		if (!(buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
				/* since() stat worked, errno will still be 0.  that's
				   no good, since access_euid() is about to return
				   failure.  set errno here to the right thing. */
			errno = EACCES;
			return -1;
		}
	}

	return 0;
}



