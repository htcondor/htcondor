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


#ifndef LIMIT_H
#define LIMIT_H

#if defined(__cplusplus)
extern "C" {
#endif

enum thingy {
	CONDOR_SOFT_LIMIT = 0,
	CONDOR_HARD_LIMIT = 1,
	CONDOR_REQUIRED_LIMIT = 2
};

#ifndef WIN32
void limit( int resource, rlim_t limit, int limit_type, char const *resource_desc);
#endif

#if defined(__cplusplus)
}
#endif

#endif
