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
#include "nullfile.h"

int 
nullFile(const char *filename)
{
	// On WinNT, /dev/null is NUL
	// on UNIX, /dev/null is /dev/null
	
	// a UNIX->NT submit will result in the NT starter seeing /dev/null, so it
	// needs to recognize that /dev/null is the null file

	// an NT->NT submit will result in the NT starter seeing NUL as the null 
	// file

	// a UNIX->UNIX submit ill result in the UNIX starter seeing /dev/null as
	// the null file
	
	// NT->UNIX submits should work fine, since we're forcing the null file to 
	// always canonicalize to /dev/null (even on NT) and then the starter translates it 
	// for us.

	#ifdef WIN32
	if(_stricmp(filename, "NUL") == 0) {
		return 1;
	}
	#endif
	if(strcmp(filename, "/dev/null") == 0 ) {
		return 1;
	}
	return 0;
}
