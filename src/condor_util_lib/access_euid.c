/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
			return -1;
		}
		already_stated = 1;
	}

	if ((R_OK == 0) || (mode & R_OK)) {
		f = fopen(path, "r");
		if (f == NULL) {
			return -1;
		}
		fclose(f);
	}

	if ((W_OK == 0) || (mode & W_OK)) {
		f = fopen(path, "a"); /* don't truncate the file! */
		if (f == NULL) {
			return -1;
		}
		fclose(f);
	}

	if ((X_OK == 0) || (mode & X_OK)) {
		if (!already_stated) {
			/* stats are expensive, so only do it if I have to */
			if (stat(path, &buf) < 0) {
				return -1;
			}
		}
		if (!(buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
			return -1;
		}
	}

	return 0;
}



