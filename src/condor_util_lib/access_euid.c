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

	/* go through the valid flags that are available to access and see if 
		they are all true */

	if (mode & F_OK) {
		if (stat(path, &buf) < 0) {
			return -1;
		}
	}
	
	if (mode & R_OK) {
		f = fopen(path, "r");
		if (f == NULL) {
			return -1;
		}
		fclose(f);
	}

	if (mode & W_OK) {
		f = fopen(path, "a"); /* don't truncate the file! */
		if (f == NULL) {
			return -1;
		}
		fclose(f);
	}

	if (mode & X_OK) {
		if (stat(path, &buf) < 0) {
			return -1;
		}
		if (!(buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
			return -1;
		}
	}

	return 0;
}


