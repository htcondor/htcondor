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
#include "condor_debug.h"

bool condor_getcwd(std::string &path)
{
	MyString mystr;
	bool rv = condor_getcwd(mystr);
	path = mystr.c_str();
	return rv;
}

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
			// There is a bug in solaris' getcwd(). It returns NULL and
			// ERANGE if getcwd() is called in a directory whose real path
			// is larger than PATH_MAX even though the buf and buflen are
			// clearly big enough to hold it. Since getcwd() does this
			// always in this context, we could consume memory here until we
			// run out unless we keep tabs on how much we actually allocated.
			// 
			// I'm going to fix it so that if we manage to go beyond 20 Megs of
			// memory trying to find room for our path, I'm declaring that to
			// be "too big", and bailing on the algorithm. Since other OSes
			// apparently have a similar style of error, I'm not going to make
			// this solaris specific. Frankly, if we need a 20M path, something
			// is wrong--or it is year 2317, file systems are huge, and someone
			// then can up it to 40M.
			//
			// This isn't going to be configurable, because that would be yet
			// another impossibly arcane knob noone would ever touch.
			//
			// -psilord 10/14/2010

			if (buflen > ((1024 * 1024) * 20)) {
				dprintf(D_ALWAYS, 
					"condor_getcwd(): Unable to determine cwd. Avoiding "
					"a probable OS bug. Assuming getcwd() failed.\n");
				return false;
			}

			// try with a bigger buffer
			continue; 
		}
		return false;
	}

	return true;
}
