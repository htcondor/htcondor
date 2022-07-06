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

#ifndef _FS_UTIL_H
#define _FS_UTIL_H

/* Prototypes */

/** Determine whether the given path is on NFS (note that if path points
	to a non-existant file, this function will test path's parent directory).
	@param path: the file path to test
	@param is_nfs: will be set to true on return if the file is on NFS
	@return: 0 if the test was successful, -1 otherwise (if the function
		returns -1, the value of is_nfs is invalid)
*/
int
fs_detect_nfs(
	const char		*path,
	bool			*is_nfs );

#endif /* _FS_UTIL_H */
