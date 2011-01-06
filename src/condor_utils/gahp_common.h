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

#ifndef GAHP_COMMON_H
#define GAHP_COMMON_H

/* This file contains GAHP protocol-related code that's shared between
 * the gridmanager and the c-gahp. Eventually, the parsing and unparsing
 * code should be here, but the way the two daemons read the commands off
 * the wire is too different. For now, we just share the data structure
 * for holding the parsed array of strings.
 */

/* Users of GahpArgs should not manipulate the class data members directly.
 * Changing the object should only be done via the member functions.
 * If argc is 0, then the value of argv is undefined. If argc > 0, then
 * argv[0] through argv[argc-1] point to valid null-terminated strings. If
 * a NULL is passed to add_arg(), it will be ignored. argv may be resized
 * or freed by add_arg() and reset(), so users should not copy the pointer
 * and expect it to be valid after these calls.
 */
class Gahp_Args {
 public:
	Gahp_Args();
	~Gahp_Args();
	void reset();
	void add_arg( char *arg );
	char **argv;
	int argc;
	int argv_size;
};


#endif
