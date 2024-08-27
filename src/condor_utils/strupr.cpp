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

#include "strupr.h"

/*
  NOTE: we do *NOT* want to include "condor_common.h" and all the
  associated header files for this, since this file defines two very
  simple functions and we want to be able to build it in a stand-alone
  version of the UserLog parser for libcondorapi.a
*/


#ifndef condor_tolower
#define condor_tolower(c) ((c) + 'a' - 'A')
#endif

#ifndef condor_toupper
#define condor_toupper(c) ((c) + 'A' - 'a')
#endif

/*
** Convert a string, in place, to the uppercase version of it. 
*/
char*
strupr(char* src)
{
	char* tmp;
	tmp = src;
	while( tmp && *tmp ) {
        if( *tmp >= 'a' && *tmp <= 'z' ) {
			*tmp = condor_toupper( *tmp );
		}
		tmp++;
	}
	return src;
}


/*
** Convert a string, in place, to the lowercase version of it. 
*/
char*
strlwr(char* src)
{
	char* tmp;
	tmp = src;
	while( tmp && *tmp ) {
        if( *tmp >= 'A' && *tmp <= 'Z' ) {
			*tmp = condor_tolower( *tmp );
		}
		tmp++;
	}
	return src;
}


