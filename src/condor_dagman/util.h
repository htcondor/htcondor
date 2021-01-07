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


#ifndef _UTIL_H_
#define _UTIL_H_

#include "condor_system.h"   /* for <stdio.h> */
#include "condor_arglist.h"

#if 0   /* These function are never used! */
int StringHash(char *str, int numBuckets);
int StringCompare(char *s1, char *s2);
int BinaryHash(void *buffer, int nbytes, int numBuckets);
void StringDup(char *&dst, char *src);
/// Create a new string containing the ascii representation of an integer
char *IntToString (int i);

const int UTIL_MAX_LINE_LENGTH = 1024;

/** Execute a command, printing verbose messages and failure warnings.
    @param cmd The command or script to execute
    @return The return status of the command
*/
int util_popen (ArgList &args);

/** Create the given lock file, containing the PID of this process.
	@param lockFileName: the name of the lock file to create
	@return: 0 if successful, -1 if not
*/
int util_create_lock_file(const char *lockFileName, bool abortDuplicates);

/** Check the given lock file and see whether the PID given in it
	does, in fact, exist.
	@param lockFileName: the name of the lock file to check
	@return: 0 if successful, -1 if there was an error, 1 if the
		relevant PID does exist and this DAGMan should abort
*/
int util_check_lock_file(const char *lockFileName);

#endif

#endif /* #ifndef _UTIL_H_ */
