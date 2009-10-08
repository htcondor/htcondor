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
#include "condor_getcwd.h"
#include "MyString.h"

bool condor_getcwd(MyString &path) {
	size_t buflen = 0;
	char *buf = NULL;

		// Some platforms (supposedly) do not support getcwd(NULL),
		// so we do it the hard way...

	while(1) {
		buflen += 256;
		buf = (char *)malloc(buflen);
		if( !buf ) {
			return false;
		}

		if( getcwd(buf,buflen) != NULL ) {
			path = buf;
			free( buf );
			buf = NULL;
			break;
		}
		free( buf );
		buf = NULL;

		if( errno == ERANGE ) {
			continue; // try with a bigger buffer
		}
		return false;
	}

	return true;
}
