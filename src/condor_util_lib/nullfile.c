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
