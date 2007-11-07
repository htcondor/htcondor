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
