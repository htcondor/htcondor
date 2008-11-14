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
#include "condor_config.h"

/* Note:  this file is now only referenced by the starter.V5 and 
   starter.jim.  Once they change to DaemonCore, this function 
   can go away.  I moved the functionality into DaemonCore->
   CreateProcess(), but the person who calls CreateProcess has
   to do the param'ing themself.  -MEY 4-16-1999 */

extern "C" {

int
renice_self( char* param_name ) {
	int i = 0;
#ifndef WIN32
	char* ptmp = param( param_name );
	if ( ptmp ) {
		i = atoi(ptmp);
		if ( i > 0 && i < 20 ) {
			nice(i);
		} else if ( i >= 20 ) {
			i = 19;
			nice(i);
		} else {
			i = 0;
		}
		free(ptmp);
	}
#endif
	return i;
}

} /* end of extern "C" */
